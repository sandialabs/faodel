// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <functional>
#include <algorithm>
#include <thread>
#include <iostream>


#include "kelpie/Key.hh"

#include "Worker.hh"
#include "JobKeys.hh"

using namespace std;

class WorkerKeys
  : public Worker {

public:
  WorkerKeys() = default;
  WorkerKeys(int id, JobKeys::params_t params)
    : Worker(id, params.num_keys, 0,0), params(params)  {}
  ~WorkerKeys() = default;

  void server() {

    std::function<kelpie::Key ()> f_keygen;
    if      (params.k1_len==0) f_keygen = [this]() { return kelpie::Key::Random(dummy_name, params.k2_len); };
    else if (params.k2_len==0) f_keygen = [this]() { return kelpie::Key::Random(params.k1_len, dummy_name); };
    else                       f_keygen = [this]() { return kelpie::Key::Random(params.k1_len, params.k2_len); };

    vector<kelpie::Key> keys(batch_size);
    do {
      //Generate a bundle of random keys
      for(int i=0; i<batch_size; i++) {
        keys[i] = f_keygen();
      }

      //Sort them
      sort(keys.begin(), keys.end());

      ops_completed += batch_size;
    } while(!kill_server);
  }
private:
  JobKeys::params_t params;
  const std::string dummy_name = "dummy-name";
};


int JobKeys::Execute(const std::string &job_name) {
  return standardExecuteWorker<WorkerKeys, JobKeys::params_t>(job_name, options);
}