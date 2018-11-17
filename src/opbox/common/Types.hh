// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_TYPES_HH
#define OPBOX_TYPES_HH

#include <functional>

#include "opbox/common/Message.hh"
#include "opbox/net/peer.hh"


namespace opbox {


//Forward references
class Op;


enum class UpdateType : int {
  start            = 1001,  //OpBox is launching a new Origin/Target op
  incoming_message = 2001,  //An incoming message has arrived for the op
  user_trigger     = 3001,  //User has triggered the op (use args for info)

  // successes are grouped together for easy testing
  send_success     = 4001,
  get_success,
  put_success,
  atomic_success,

  timeout          = 5001,

  // errors are grouped together for easy testing
  send_error       = 6001,
  get_error,
  put_error,
  atomic_error
};
std::string str(UpdateType type);


enum class WaitingType : int {
  waiting_on_cq,        //Op is waiting on network completion queue
  //waiting_on_timeout,
  //waiting_oneway,
  //wait_to_do_user_cb
  wait_on_user,         //Op is waiting on a user trigger
  done_and_destroy,     //Op is done, system should destroy it
  error                 //Something went wrong and opbox should take care of it
};
std::string str(WaitingType type);


// This is an attempt to create a hash from the op name at compile time. This
// hash can be weak- we just want to settle on an id. See:
// From http://stackoverflow.com/questions/2111667/compile-time-string-hashing
// note: This hashes in reverse order, compared to the hash_dbj2 function
unsigned constexpr const_hash(char const *input) {
  return *input ?
    static_cast<unsigned int>(*input) + 33 * const_hash(input + 1) :
    5381;
}

//Generate a 16b hash by xoring top and bottom halves of 32b hash together
uint16_t constexpr const_hash16(char const *input) {
  return (const_hash(input)>>16) ^ (const_hash(input) & 0x0FFFF);
}

using fn_OpCreate_t = std::function<Op * ()>;   //!< Lambda for constructing a particular Op


} // namespace opbox

#endif // OPBOX_OPBOXTYPES_HH
