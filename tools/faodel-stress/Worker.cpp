// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "Worker.hh"


using namespace std;

Worker::Worker()
  : id(0), thread_launched(false), kill_server(false), batch_size(0), ops_completed(0) {
}
Worker::Worker(int id, uint32_t batch_size, uint32_t min_prng, uint32_t max_prng)
  : id(id),
    thread_launched(false), kill_server(false),
    batch_size(batch_size), ops_completed(0),
    prngGen(id), //Seed with the thread id
    prngDistrib(min_prng, max_prng) {

}

Worker::~Worker(){
  Stop();
}

void Worker::Start(){
  F_ASSERT(!thread_launched, "Attempted to start work when thread already started");
  th_server = thread(&Worker::server, this);
  kill_server=false;
  thread_launched=true;
}

void Worker::Stop(){
  if(thread_launched){
    kill_server=true;
    th_server.join();
    thread_launched=false;
  }

}
