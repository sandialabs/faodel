// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_BACKBURNER_HH
#define FAODEL_COMMON_BACKBURNER_HH

#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <chrono>

#include <cinttypes>

#include "common/Configuration.hh"
#include "common/BootstrapInterface.hh"
#include "common/LoggingInterface.hh"


namespace faodel {

//Backburner just calls an empty function. User puts all options in the capture
using fn_backburner_work = std::function<int ()>;

using bb_work_queue = std::queue<fn_backburner_work>;


namespace backburner {

std::string bootstrap(); //Function handed to bootstrap for dependency injection

//Public interface
void RegisterPollingFunction(std::string name, uint32_t group_id, fn_backburner_work polling_function);
void DisablePollingFunction(std::string name);
void DisablePollingFunction(std::string name, uint32_t group_id);

void AddWork(fn_backburner_work work);
void AddWork(std::vector<fn_backburner_work> work);
void AddWork(uint32_t tag, fn_backburner_work work);
void AddWork(uint32_t tag, std::vector<fn_backburner_work> work);


namespace internal {

/**
 * @brief A queue for managing work that takes place in the background
 *
 * Several components in FAODEL need a way to queue up tasks that are
 * performed in the background (eg, for deadlock avoidance or performance
 * reasons). Backburner dedicates one thread (or more) to running tasks.
 * While the queue is protected by mutexes, the worked thread will pull
 * several tasks at a time and process them in order.
 */
class BackBurner
  : public faodel::bootstrap::BootstrapInterface,
    public faodel::LoggingInterface    {

public:
  BackBurner();
  ~BackBurner();

  //Bootstrap API
  void Init(const faodel::Configuration &config) override;
  void Start() override;
  void Finish() override;
  void GetBootstrapDependencies(std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const override;

  //Call before Start
  void RegisterPollingFunction(std::string name, uint32_t group_id, fn_backburner_work polling_function);
  void DisablePollingFunction(std::string name);
  void DisablePollingFunction(std::string name, uint32_t group_id);

  //Use to add a task to the queue
  void AddWork(fn_backburner_work work);
  void AddWork(std::vector<fn_backburner_work> work);
  void AddWork(uint32_t tag, fn_backburner_work work);
  void AddWork(uint32_t tag, std::vector<fn_backburner_work> work);

private:

  /**
   * @brief A worker thread that processes bundles of tasks at a time
   */
  class Worker
      : public faodel::LoggingInterface    {

  public:
      Worker();

      explicit Worker(BackBurner *parent);
      ~Worker() override;

      void SetConfiguration(const Configuration &config, int id);

      void AddWork(fn_backburner_work work);
      void AddWork(std::vector<fn_backburner_work> work);
      void RegisterPollingFunction(std::string name, uint32_t group_id, fn_backburner_work polling_function);
      void DisablePollingFunction(std::string name);
  private:
      void server();

      BackBurner *parent;
      int worker_id;
      bool kill_worker;

      bb_work_queue *tasks_consumer;
      bb_work_queue *tasks_producer;
      bb_work_queue tasks_a;
      bb_work_queue tasks_b;
      std::mutex mtx;

      uint32_t producer_num;
      uint32_t consumer_num;

      std::thread th_server;

      std::map<std::string, fn_backburner_work> registered_poll_functions;

  };

  bool configured;
  bool workers_launched;
  uint64_t worker_count;
  std::vector<Worker> *workers; //Pointer to avoid defining worker copy operator

};
} // namespace internal
} // namespace backburner
} // namespace faodel

#endif // FAODEL_COMMON_BACKBURNER_HH
