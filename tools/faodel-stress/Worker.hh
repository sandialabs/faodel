// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_WORKER_HH
#define FAODEL_WORKER_HH

#include <string>
#include <thread>
#include <functional>
#include <mutex>
#include <queue>
#include <random>

#include "faodel-common/Debug.hh"

class Worker {
public:
  Worker();
  Worker(int id, uint32_t batch_size, uint32_t min_prng, uint32_t max_prng);
  Worker(const Worker &p)
  : id(p.id),
    kill_server(p.kill_server),
    batch_size(p.batch_size),
    ops_completed(p.ops_completed),
    prngGen(p.prngGen),
    prngDistrib(p.prngDistrib),
    thread_launched(false)  {
    F_ASSERT(!p.thread_launched, "Creating worker when it's already started?");
  }
  virtual ~Worker();

  void Start();
  void Stop();

  uint64_t GetOpsCompleted() const { return ops_completed; }

protected:
  int id;
  bool kill_server;
  uint32_t batch_size;
  uint64_t ops_completed;

  //Ranged pseudo random number generator
  //Most workers need a prng that falls within a range.
  std::mt19937 prngGen;
  std::uniform_int_distribution<uint32_t> prngDistrib;
  uint32_t prngGetRangedInteger() { return prngDistrib(prngGen); }

private:
  bool thread_launched;
  virtual void server() = 0;
  std::thread th_server;

};


#endif //FAODEL_WORKER_HH
