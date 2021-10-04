// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <functional>
#include <vector>
#include <thread>

#include "Worker.hh"
#include "JobSerdes.hh"
#include "serdes/WorkerSerdes.hh"
#include "serdes/SerdesParticleBundleObject.hh"
#include "serdes/SerdesStringObject.hh"


using namespace std;

JobSerdes::JobSerdes(const faodel::Configuration &config)
        : Job(config, JobCategoryName()) {

  for(auto &name_params : options)
    job_names.push_back(name_params.first);

}

int JobSerdes::Execute(const std::string &job_name) {

  auto it = options.find(job_name);
  if (it==options.end()) return -1;
  dbg("Launching "+to_string(num_threads)+" worker threads");

  vector<Worker *> w_serdes(num_threads);
  for(int i=0; i<num_threads; i++) {
    switch(it->second.obj_type) {
      case 1: w_serdes.at(i) = new WorkerSerdes<SerdesStringObject>(i, it->second); break;
      case 2: w_serdes.at(i) = new WorkerSerdes<SerdesParticleBundleObject>(i, it->second); break;
      default:
        F_ASSERT(0, "Unknown obj_type in JobSerdes");
    }
  }
  testStart();
  for(auto *w : w_serdes) w->Start();

  testSleep();
  for(auto &w : w_serdes) w->Stop();

  testStop();


  for(auto *w : w_serdes) {
    ops_completed += w->GetOpsCompleted();
    dbg("Thread ops completed: "+std::to_string(w->GetOpsCompleted()));
    delete w;
  }

  DumpJobStats(job_name);

  return 0;
}

