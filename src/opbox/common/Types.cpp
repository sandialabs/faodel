// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <sstream>

#include "opbox/common/Types.hh"

using namespace std;

namespace opbox {

string str(UpdateType type) {
  switch(type){
  case UpdateType::start:             return "start";
  case UpdateType::incoming_message:  return "incoming_message";
  case UpdateType::user_trigger:      return "user_trigger";
  case UpdateType::send_success:      return "send_success";
  case UpdateType::get_success:       return "get_success";
  case UpdateType::put_success:       return "put_success";
  case UpdateType::atomic_success:    return "atomic_success";
  case UpdateType::timeout:           return "timeout";
  case UpdateType::send_error:        return "send_error";
  case UpdateType::get_error:         return "get_error";
  case UpdateType::put_error:         return "put_error";
  case UpdateType::atomic_error:      return "atomic_error";
  default: ;
  }
  return "unknown";
}

string str(WaitingType type) {
  switch(type){
  case WaitingType::waiting_on_cq:    return "waiting_on_cq";
  case WaitingType::wait_on_user:     return "wait_on_user";
  case WaitingType::done_and_destroy: return "done_and_destroy";
  default: ;
  }
  return "unknown";
}



} // namespace opbox
