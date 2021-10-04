// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPARGS_HH
#define OPBOX_OPARGS_HH

#include <cstring>
#include <stdexcept>

#include "opbox/common/Types.hh"

namespace opbox {

/**
 * @class OpArgs
 * @brief Base class for passing events to an Op, and returning results
 * @details OpArgs is a base class for passing update information to an Op's
 * state machine. Updates indicate a change in the runtime (eg, start the state
 * machine or a network transfer success/error/timeout), the arrival of an
 * incoming message, or the arrival of a user-defined trigger. A user-defined
 * trigger is the only update that allows the state machine to use the OpArgs
 * as a way to pass back information directly to an app.
 */
class OpArgs :
    public faodel::InfoInterface {

public:
  OpArgs()=delete;                        ///< Intentionally removed to force a real ctor

  //Everything except incoming message
  explicit OpArgs(UpdateType type)
    : type(type),
      result(0),
      incoming_msg_sender(nullptr),
      incoming_msg(nullptr),
      copy_msg(false) {} ///< ctor for all types except incoming message

  //Incoming message
  OpArgs(opbox::net::peer_ptr_t sender, message_t *msg, bool copy_msg=false)
    : type(UpdateType::incoming_message),
      result(0),
      incoming_msg_sender(sender),
      copy_msg(copy_msg)
  {
      if (copy_msg) {
          incoming_msg = (message_t*)malloc(sizeof(message_t) + msg->body_len);
          memcpy(incoming_msg, msg, sizeof(message_t) + msg->body_len);
      } else {
          incoming_msg = msg;
      }
  }                ///< ctor for an incoming message

  ~OpArgs() override { if (copy_msg) free(incoming_msg); }


  void VerifyTypeOrDie(UpdateType expected_type, std::string op_name);

  bool IsIncomingMessage(){ return type==UpdateType::incoming_message; } ///< True if this OpArgs is an incoming message

  /**
   * @brief Verify an op is an incoming message, extract the sender, and recast
   *
   * @details
   * Often an Op expects to get an incoming message that it will then convert
   * to a specific message pointer type. This template does several things in
   * this process in one shot:
   *   - It verifies this is an actual incoming_message type
   *   - It extracts the sender's peer_ptr from the args
   *   - It casts the message to an expected format
   *
   * @param[in] return_sender The peer_ptr_t of the node that sent this
   * @return MPTR A cast to a particular message pointer type (eg, message_t *)
   */
  template<typename MPTR>
  MPTR ExpectMessage(opbox::net::peer_ptr_t *return_sender=nullptr){
    if(!IsIncomingMessage()) return nullptr;
    if(return_sender) *return_sender=incoming_msg_sender;
    return reinterpret_cast<MPTR>(incoming_msg);
  }

  //! @brief Same as ExpectMessage, but throw an exception if expected vals don't match
  template<typename MPTR>
  MPTR ExpectMessageOrDie(opbox::net::peer_ptr_t *return_sender=nullptr){
    if(IsIncomingMessage()){
      if(return_sender)
        *return_sender = incoming_msg_sender;
      return reinterpret_cast<MPTR>(incoming_msg);
    }
    throw std::runtime_error("State machine got an incorrect update (wrong update type or wrong message");
  }

  template<typename OPPTR>
  OPPTR ExpectTrigger(){
    if(type!=UpdateType::user_trigger) return nullptr;
    return reinterpret_cast<OPPTR>(this);
  }

  template<typename OPPTR>
  OPPTR ExpectTriggerOrDie(){
    if(type==UpdateType::user_trigger){
      return reinterpret_cast<OPPTR>(this);
    }
    throw std::runtime_error("State machine Expected a trigger, but got wrong OpArg Type");
  }

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  void print(std::stringstream &ss, int depth=0, int indent=0) const;

  UpdateType type;                            //!< Which type of update is this OpArgs

  int result;                                 //!< Result of last Op update. OpboxCore sets, but Op state machine could set too.

private:
  opbox::net::peer_ptr_t incoming_msg_sender; //!< Only valid on incoming message type
  message_t *incoming_msg;                    //!< Only valid on incoming message type
  bool copy_msg;
};


void SanityCheck(OpArgs *args, const char *src_file, int line);

#if 1
#define SanityCheckArgs(a) { SanityCheck(a.get(), __FILE__, __LINE__); }
#else
#define SanityCheckArgs(a) {}
#endif

} // namespace opbox

#endif // OPBOX_OPARGS_HH
