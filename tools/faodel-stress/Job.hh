// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOB_HH
#define FAODEL_JOB_HH

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>


#include "faodel-common/Configuration.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/StringHelpers.hh"

class Job :
    public faodel::LoggingInterface {
public:

  Job(const faodel::Configuration &config, const std::string &job_category);
  virtual ~Job() = default;

  void DumpJobNames();
  virtual void DumpJobStats(const std::string &job_name);

  std::vector<std::string> GetMatchingJobNames(const std::string &search_names);

  virtual int Init() { return 0; }; //After faodel as initialized but before start

  virtual int Setup(const std::string &search_names);
  virtual int Execute(const std::string &job_name) = 0;
  virtual int ExecuteAll();
  virtual void Teardown() {};


  template<class T,class P>
  int standardExecuteWorker(const std::string &job_name, const std::map<std::string,P> &options) {

    auto it = options.find(job_name);
    if (it==options.end()) return -1;

    dbg("Launching "+std::to_string(num_threads)+" worker threads");
    std::vector<T> w_workers;
    for(int i=0; i<num_threads; i++)
      w_workers.push_back(T(i, it->second));

    testStart();
    for(auto &w : w_workers) w.Start();

    testSleep();
    for(auto &w : w_workers) w.Stop();

    testStop();
    for(auto &w : w_workers) {
      ops_completed += w.GetOpsCompleted();
      dbg("Thread ops completed: "+std::to_string(w.GetOpsCompleted()));
    }

    DumpJobStats(job_name);

    return 0;
  }

  std::string job_category;
  std::vector<std::string> job_names;
  std::vector<std::string> selected_job_names;

protected:
  uint64_t num_threads;
  bool initialized;
  bool dump_tsv;
  uint64_t ops_completed;
  uint64_t run_time_seconds;
  std::chrono::time_point<std::chrono::system_clock> t_start;
  std::chrono::time_point<std::chrono::system_clock> t_stop;
  void testStart() {ops_completed=0; t_start=std::chrono::system_clock::now();}
  void testSleep() const {std::this_thread::sleep_for(std::chrono::seconds(run_time_seconds));}
  void testStop()  {t_stop= std::chrono::system_clock::now();}
  double getTestTimeUS() { return std::chrono::duration_cast<std::chrono::microseconds>(t_stop - t_start).count();}
  double getTestMops() { return ops_completed / getTestTimeUS(); }
};


#endif // FAODEL_STRESSJOB_HHH
