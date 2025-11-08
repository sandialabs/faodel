// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <stdexcept>

#include "faodel-common/Debug.hh"
#include "opbox/common/OpArgs.hh"

using namespace std;

namespace opbox {


/**
 * @brief Verify the OpArgs is an expected UpdateType. Throws Exception on error
 *
 * @param[in] expected_type The UpdateType OpArgs is expected to be
 * @param[in] op_name The label of the Op to display in an exception
 */
void OpArgs::VerifyTypeOrDie(UpdateType expected_type, string op_name){
  if(type!=expected_type){
//    cerr << "OpArg.VerifyType fail: Op "+op_name+" got '"+opbox::str(type)+"' when expected '"+opbox::str(expected_type)+"'" << endl;
//    abort();
    throw std::runtime_error("OpArg.VerifyType fail: Op "+op_name+" got '"+opbox::str(type)+"' when expected '"+opbox::str(expected_type)+"'");
  }
}


#if 1

void SanityCheck(OpArgs *args, const char *src_file, int line ) {

  if(args==nullptr) {
    faodel::_f_halt("OpArgs Sanity check fail: null pointer for args", src_file, line);
  }
  switch(args->type) {

  case UpdateType::incoming_message:
    //F_ASSERT((args->incoming_msg!=nullptr), "OpArgs sanity check fail: null pointer for incoming_msg");
    break;

  case UpdateType::user_trigger: break; //Special

  //Everybody Else
  case UpdateType::start: 
  case UpdateType::send_success:
  case UpdateType::get_success:
  case UpdateType::put_success:
  case UpdateType::atomic_success:
  case UpdateType::timeout:
  case UpdateType::send_error:
  case UpdateType::get_error:
  case UpdateType::put_error:
  case UpdateType::atomic_error:
    //F_ASSERT(args->incoming_msg == nullptr, "OpArgs sanity check fail: incoming_msg was not null");
    break;

    //Unrecognized? Probably a bad type
  default: 
    faodel::_f_halt("OpArgs sanity check fail: Type was not valid?", src_file, line);
  }
}
#endif

void OpArgs::print(std::stringstream &ss, int depth, int indent) const {
    if(depth<0) return;
    ss <<string(indent,' ')<<"[OpboxArg] Type: "+opbox::str(type);
}

void OpArgs::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss <<string(indent,' ')<<"[OpboxArg] Type: "+opbox::str(type);
}

} // namespace opbox
