// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <functional>
#include <vector>
#include <thread>

#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "Worker.hh"
#include "JobMemoryAlloc.hh"


using namespace std;

class WorkerMemoryAlloc
        : public Worker {

public:
  WorkerMemoryAlloc() = default;
  WorkerMemoryAlloc(int id, JobMemoryAlloc::params_t params)
          : Worker(id, params.num_items, params.min_ldo_size, params.max_ldo_size),
            params(params) {

    //The eager allocator by default uses pinned memory, while lazy defers until use.
    alloc_type = (params.netmem) ? lunasa::DataObject::AllocatorType::eager
                                 : lunasa::DataObject::AllocatorType::lazy;

  }
  ~WorkerMemoryAlloc() = default;

  void server() {


    std::function<lunasa::DataObject ()> f_createobject;

    if(params.min_ldo_size == params.max_ldo_size) {
      f_createobject = [this]() {
          return lunasa::DataObject(0, params.max_ldo_size, alloc_type);
      };
    } else {
      f_createobject = [this]() {
          return lunasa::DataObject(0, prngGetRangedInteger(), alloc_type);
      };
    }


    do {
      vector<lunasa::DataObject> ldos(params.num_items);
      for(int i=0; i<params.num_items; i++) {
        ldos[i]=f_createobject();
      }
      ops_completed += params.num_items;
    } while(!kill_server);

  }
private:

  JobMemoryAlloc::params_t params;
  int id;
  lunasa::DataObject::AllocatorType alloc_type;

  const std::string dummy_name = "dummy-name";
};





JobMemoryAlloc::JobMemoryAlloc(const faodel::Configuration &config)
        : Job(config, JobCategoryName()) {

  for(auto &name_params : options)
    job_names.push_back(name_params.first);

}

int JobMemoryAlloc::Execute(const std::string &job_name) {
  return standardExecuteWorker<WorkerMemoryAlloc, JobMemoryAlloc::params_t>(job_name, options);
}

