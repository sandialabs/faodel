// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>

#include "faodel-common/Debug.hh"

#include "opbox/ops/Op.hh"
#include "opbox/ops/OpCount.hh"

using namespace std;
using namespace opbox;

const unsigned int OpCount::op_id = const_hash("OpCount");
const std::string OpCount::op_name = "OpCount";


OpCount::OpCount(int num_left)
  : Op(false), num_left(num_left), state(State::start) {
}


WaitingType OpCount::Wait() {
    count_future.get();
    return WaitingType::done_and_destroy;
}

WaitingType OpCount::UpdateOrigin(OpArgs *args) {

  switch(state){
  case State::start:

    F_ASSERT(args->type == UpdateType::user_trigger, "Was expecting a user trigger?");
    if(num_left){
      num_left--;
    }
    {
      int t1 = getMSTimeStamp();
      int diff = t1 - ts_created;
      std::cout <<"Opcount at state "<<GetStateName()<<" with steps left="<<dec<<num_left<<" AliveTime(ms): "<<diff<<std::endl;
      if(!num_left){
        std::cout << "OpCount done\n";
        state=State::done;
        return WaitingType::done_and_destroy;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return WaitingType::wait_on_user;

  case State::done:
    return WaitingType::done_and_destroy;

  default:
    cout <<"Unknown state: "<<GetStateName()<<endl;
    exit(-1);
  }
  //Shouldn't get here
  return WaitingType::error;
}

std::string OpCount::GetStateName() const {
  const static map<State,string> names = {
    { State::start, "Start" },
    { State::done,  "Done" } };
  //TODO: safe version would assert find isn't end
  return names.find(state)->second;
}
