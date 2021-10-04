// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <functional>
#include <vector>
#include <thread>


#include "kelpie/Kelpie.hh"

#include "Worker.hh"
#include "JobLocalPool.hh"

using namespace std;

class WorkerLocalPool
        : public Worker {

public:
  WorkerLocalPool() = default;
  WorkerLocalPool(int id, JobLocalPool::params_t params)
          : Worker(id, params.num_kvs, 0,0),
            params(params)  {

    for(int i=0; i<batch_size; i++) {

      //Generate keys
      switch(params.key_strategy) {
        case 1: keys.push_back( kelpie::Key::Random(255)); break; //Every key goes to its own row
        case 2: keys.push_back( kelpie::Key::Random("worker-"+std::to_string(id),255)); break; //Each worker goes to its own row
        case 3: keys.push_back( kelpie::Key::Random(dummy_name,255)); break; //All workers go to same row
        default:
          throw std::runtime_error("Unknown key strategy");
      }

      //Generate objects if needed
      if(!params.allocate_ondemand) {
        ldos.emplace_back(lunasa::DataObject(params.ldo_size));
      }
    }

    pool = kelpie::Connect("local:");


  }
  ~WorkerLocalPool() = default;

  void server() {

    //Select whether we generated the objects in ctor or on-demand
    std::function<lunasa::DataObject(int id)> f_getobject;
    if(params.allocate_ondemand) f_getobject = [this](int i) { return lunasa::DataObject(params.ldo_size);};
    else                         f_getobject = [this](int i) { return ldos.at(i);};

    do {

      //Insert a bunch of objects
      for(int i=0; i<batch_size; i++) {
        pool.Publish(keys.at(i), f_getobject(i));
      }

      //Get a bunch of objects
      for(int i=0; i<batch_size; i++) {
        lunasa::DataObject ldo;
        pool.Need(keys.at(i), &ldo);
      }

      //Drop a bunch of objects
      for(int i=0; i<batch_size; i++) {
        pool.BlockingDrop(keys.at(i));
      }

      ops_completed += batch_size;
    } while(!kill_server);
  }
private:

  std::vector<kelpie::Key> keys;
  std::vector<lunasa::DataObject> ldos;

  kelpie::Pool pool;

  JobLocalPool::params_t params;
  const std::string dummy_name = "dummy-name";
};


JobLocalPool::JobLocalPool(const faodel::Configuration &config)
  : Job(config, JobCategoryName()) {

  for(auto &name_params : options)
    job_names.push_back(name_params.first);

}

int JobLocalPool::Execute(const std::string &job_name) {
  return standardExecuteWorker<WorkerLocalPool,JobLocalPool::params_t>(job_name, options);
}

