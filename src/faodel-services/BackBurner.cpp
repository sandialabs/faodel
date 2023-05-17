// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <utility>
#include <vector>
#include <string>
#include <unistd.h>


#include "faodel-services/BackBurner.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/Debug.hh"

#if FAODEL_LOGGING_INTERFACE_DISABLE==1
#define dbg(a) {}
#endif


using namespace std;


namespace faodel {
namespace backburner {


namespace internal {

BackBurner::BackBurner() :
  LoggingInterface("backburner"),
  configured(false), workers_launched(false) {
}

BackBurner::~BackBurner() {
  if(configured) {
    Finish();
  }
}
void BackBurner::RegisterPollingFunction(const string &name, uint32_t group_id, fn_backburner_work polling_function) {

  F_ASSERT(!configured, "BackBurner RegisterPollingFunction called after Start called");

  workers->at(group_id%worker_count).RegisterPollingFunction(name, group_id, std::move(polling_function));
}

void BackBurner::DisablePollingFunction(string name) {
  for (uint64_t i=0;i<worker_count;i++) {
    workers->at(i).DisablePollingFunction(name);
  }
}

void BackBurner::DisablePollingFunction(const string &name, uint32_t group_id) {

  workers->at(group_id%worker_count).DisablePollingFunction(name);
}

void BackBurner::Init(const Configuration &config) {
  F_ASSERT(!configured, "BackBurner Init called twice");

  ConfigureLogging(config);

  workers_launched=false; //Wait until start to fire up
  configured=true;

  config.GetUInt(&worker_count, "backburner.threads", "1");

  //Note: Don't use push_back(worker(id)) here, because doing so means
  //      you need to define copy operator.
  workers = new std::vector<Worker>(worker_count);
  for(size_t i=0; i<worker_count; i++) {
    workers->at(i).SetConfiguration(config, i);
  }
}
void BackBurner::Start() {
  //workers = new std::vector<Worker>(worker_count);
  workers_launched=true;
}

void BackBurner::Finish() {
  F_ASSERT(configured, "Backburner Finish called when not in configured state");
  if(workers_launched) {
    workers_launched=false;
  }
  delete workers;
  configured=false;
}


void BackBurner::GetBootstrapDependencies(
                       string &name,
                       vector<string> &requires,
                       vector<string> &optional) const {
  name = "backburner";
  requires = {};
  optional = {};
}


void BackBurner::AddWork(fn_backburner_work work) {
  dbg("Add Work");
  workers->at(0).AddWork(std::move(work));
}

void BackBurner::AddWork(vector<fn_backburner_work> work) {
  dbg("Add Work["+std::to_string(work.size())+"]");
  workers->at(0).AddWork(work);
}

void BackBurner::AddWork(uint32_t tag, fn_backburner_work work) {
  dbg("Add work with tag "+std::to_string(tag));
  workers->at(tag%worker_count).AddWork(std::move(work));
}

void BackBurner::AddWork(uint32_t tag, vector<fn_backburner_work> work) {
  dbg("Add work["+std::to_string(work.size())+"] with tag "+std::to_string(tag));
  workers->at(tag%worker_count).AddWork(work);
}


BackBurner::Worker::Worker() :
  LoggingInterface("backburner.worker"),
  parent(nullptr),
  worker_id(0),
  kill_worker(false),
  tasks_consumer(&tasks_a), tasks_producer(&tasks_b),
  producer_num(0), consumer_num(0)  {
}

BackBurner::Worker::Worker(BackBurner *parent) :
  LoggingInterface("backburner.worker"),
  parent(parent),
  tasks_consumer(&tasks_a), tasks_producer(&tasks_b),
  producer_num(0), consumer_num(0) {

  th_server = thread(&Worker::server, this);
}

BackBurner::Worker::~Worker() {
  kill_worker = true;
  notifyNewWork(); //Some notification methods may need to be unblocked to see kill_worker
  th_server.join();
}

void BackBurner::Worker::SetConfiguration(const Configuration &config, int id) {
  worker_id = id;
  SetSubcomponentName("["+std::to_string(id)+"]");
  ConfigureLogging(config);

  string notification_method;
  config.GetString(&notification_method, "backburner.notification_method", "pipe");

  if(notification_method == "polling") {
    //This version doesn't do any special notification and will burn the cpu
    notifyNewWork = []() {};
    blockUntilWork = []() { return 1; };

  } else if (notification_method == "sleep_polling") {
    //This version polls, but injects sleep time so we don't eat as much time
    uint64_t time_us;
    config.GetTimeUS(&time_us, "backburner.sleep_polling_time","100us");
    dbg("Notification method: sleep_polling with a delay of "+std::to_string(time_us)+" us");
    notifyNewWork = []() {};
    blockUntilWork = [=]() {
        std::this_thread::sleep_for(std::chrono::microseconds(time_us));
        return 1;
    };

  } else if (notification_method == "pipe") {
    dbg("Notification method: pipe");

    int rc = pipe(notification_pipe);
    F_ASSERT(!rc, "Trouble setting up notification pipe in backburner?");
    //Use non blocking writes
    int flags = fcntl(notification_pipe[1], F_GETFL);
    F_ASSERT(flags>=0, "Pipe flags were bad?");
    rc = fcntl(notification_pipe[1], F_SETFL, flags | O_NONBLOCK);
    F_ASSERT(rc>=0, "Backburner could not set notification pipe writes to non blocking?");

    notifyNewWork = [=] {
        dbg("Notifying through pipe");
        uint32_t new_work=1;
        ssize_t write_bytes = write(notification_pipe[1], &new_work, 4);
        F_ASSERT(write_bytes==4, "notifyNewWork could not write full word to pipe?");
    };
    blockUntilWork = [=] {
        dbg("Blocking on pipe");
        uint32_t val;
        ssize_t read_size;
        do {
          read_size = read(notification_pipe[0], &val, 4);
          F_ASSERT(read_size==4, "blockUntilWork did not get full word from pipe?");
        } while(read_size!=4);
        return 1;
    };
  }

  th_server = thread(&Worker::server, this);
}

void BackBurner::Worker::AddWork(fn_backburner_work work) {
  dbg("Add Work");
  mtx.lock();
  tasks_producer->push(work);
  producer_num++;
  mtx.unlock();
  notifyNewWork();
}

void BackBurner::Worker::AddWork(vector<fn_backburner_work> work) {
  dbg("Add Work ["+std::to_string(work.size())+"]");
  mtx.lock();
  for(auto &w : work)
    tasks_producer->push(w);
  producer_num+=work.size();
  mtx.unlock();
  notifyNewWork();
}

void BackBurner::Worker::RegisterPollingFunction(string name, uint32_t group_id, fn_backburner_work polling_function) {
  dbg("Register polling function "+name);
  auto it = registered_poll_functions.find(name);
  if(it != registered_poll_functions.end()) {
    cerr <<"Attempted to register function "<<name<<" more than once in BackBurner\n";
    exit(-1);
  }
  registered_poll_functions[name] = std::move(polling_function);
}

void BackBurner::Worker::DisablePollingFunction(string name) {
  dbg("Disabling polling function "+name);
  auto it = registered_poll_functions.find(name);
  if(it != registered_poll_functions.end()) {
    registered_poll_functions[name] = nullptr;
  }
}

void BackBurner::Worker::server() {

  int num_bundles=0;
  int num=0;
  bool no_extra_poll_functions = registered_poll_functions.empty();

  while(!kill_worker) {
    dbg("Polling");

    //If user didn't add any polling functions we can use notifications to save some cycles
    if(no_extra_poll_functions) blockUntilWork();

    //Only lock when there's work
    if(producer_num!=consumer_num) {

      mtx.lock();
      consumer_num = producer_num; //We will consume all work
      if(!tasks_producer->empty()) {
        //Swap the queues.
        bb_work_queue *tmp = tasks_consumer;
        tasks_consumer=tasks_producer;
        tasks_producer=tmp;
        mtx.unlock();

        dbg("Found "+std::to_string(tasks_consumer->size())+" tasks to consume");
        num_bundles++;

        int bundle_spot=0;
        int num_in_bundle=tasks_consumer->size();

        //Work through all the entries
        while(!tasks_consumer->empty()) {
          fn_backburner_work work = tasks_consumer->front();
          tasks_consumer->pop();
          work();
          bundle_spot++;
          dbg("Finished task "+std::to_string(num)+" ["+std::to_string(bundle_spot)+"/"+std::to_string(num_in_bundle)+"]");
          num++;
        }
      } else {
        mtx.unlock();
      }
    }

    //Walk through any registered poll functions
    for(auto &name_fn : registered_poll_functions) {
      fn_backburner_work work = name_fn.second;
      if(work != nullptr) {
        work();
        dbg("Finished registered polling function");
      }
    }

    std::this_thread::yield();
  }
  //cout<<"Total time: "<<times<<"   avg time: "<<times/(double)num<<"  avgInBundle: "<<num/num_bundles<<endl;

}

//Global, but not visible
BackBurner bb;

} // namespace internal

void RegisterPollingFunction(const string &name, uint32_t group_id, fn_backburner_work polling_function) {
  return internal::bb.RegisterPollingFunction(name, group_id, std::move(polling_function));
}
void DisablePollingFunction(const string &name) {
  return internal::bb.DisablePollingFunction(name);
}
void DisablePollingFunction(const string &name, uint32_t group_id) {
  return internal::bb.DisablePollingFunction(name, group_id);
}
void AddWork(fn_backburner_work work) {
  return internal::bb.AddWork(std::move(work));
}
void AddWork(vector<fn_backburner_work> work) {
  return internal::bb.AddWork(std::move(work));
}
void AddWork(uint32_t tag, fn_backburner_work work) {
  return internal::bb.AddWork(tag, std::move(work));
}
void AddWork(uint32_t tag, vector<fn_backburner_work> work) {
  return internal::bb.AddWork(tag, std::move(work));
}

std::string bootstrap() {
  //No dependencies
  faodel::bootstrap::RegisterComponent(&internal::bb, true);
  return "backburner";
}

} // namespace backburner
} // namespace faodel
