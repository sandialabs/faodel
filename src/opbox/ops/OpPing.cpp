// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>
#include <algorithm>

#include "faodel-common/Debug.hh"

#include "opbox/common/MessageHelpers.hh"
#include "opbox/ops/OpPing.hh"
#include "opbox/ops/OpHelpers.hh"

using namespace std;
using namespace opbox;

const unsigned int OpPing::op_id = const_hash("OpPing");
const string OpPing::op_name = "OpPing";

/**
 * @brief Create origin side of the op and generate the initial message
 * @param[in] dst Node to send this ping to
 * @param[in] ping_message A short text message to use in the ping
 * @return OpPing - The origin state machine, ready to launch first message
 */
OpPing::OpPing(opbox::net::peer_ptr_t dst, string ping_message)
  :  Op(true), state(State::start), ldo_msg() {

  peer = dst;

  AllocateStringRequestMessage( ldo_msg,
                                 opbox::net::ConvertPeerToNodeID(dst),
                                 GetAssignedMailbox(),
                                 op_id,
                                 0,
                                 ping_message);
}

/**
 * @brief Create the target side of a ping message. Allocates space for a reply message
 * @param[in] t An op_create_as_target_t constexpr marker signifying this is a target op
 * @return OpPing - The target state machine, ready to be triggered for handling a new message
 */
OpPing::OpPing(op_create_as_target_t t)
  : Op(t), state(State::start), peer(nullptr), ldo_msg()  {
}

/**
 * @brief Close out an OpPing message, releasing resources held during its lifetime
 * @return void
 */
OpPing::~OpPing(){
//  if ((state == State::start) && (ldo_msg != nullptr)) {
//    opbox::net::ReleaseMessage(ldo_msg);
//  }
}

/**
 * @brief retrieve a future handle so the origin can see the message
 * @return Future that returns the reply string when completed
 * @note This must be called on origin after creation, but before launch of op
 */
std::future<std::string> OpPing::GetFuture() {
  return ping_promise.get_future();
}

/**
 * @brief Origin Start State - Send the outgoing message
 * @return WaitingType
 */
WaitingType OpPing::smo_Start(){

  F_ASSERT(peer != nullptr, "Didn't get a proper peer?");

  opbox::net::SendMsg(peer, std::move(ldo_msg), AllEventsCallback(this) );

  return updateState(State::snd_wait_for_sent, WaitingType::waiting_on_cq);
}

/**
 * @brief Origin Wait for Send Done State - Wait for notifaction outgoing message is gone
 * @param[in] args OpBox args (expecting send_success)
 * @return WaitingType
 */
WaitingType OpPing::smo_WaitSend(opbox::OpArgs *args){
  //args->VerifyTypeOrDie(UpdateType::send_success, op_name);
  if(args->type==UpdateType::send_success) {
    //Message launched. Wait for reply
    return updateState(State::snd_wait_for_reply, WaitingType::waiting_on_cq);
  }
  //If the actual reply message, handle it
  if(args->type==UpdateType::incoming_message){
    return smo_WaitReply(args);
  }
  return WaitingType::error;
}

/**
 * @brief Origin Wait for Reply - Wait for Target to send us a message
 * @param[in] args OpBox args (expecting incoming reply message)
 * @return WaitingType
 */
WaitingType OpPing::smo_WaitReply(opbox::OpArgs *args){

  auto incoming_msg = args->ExpectMessageOrDie<message_t *>();
  string user_data = UnpackStringMessage(incoming_msg);

  ping_promise.set_value(user_data); //Pass back to user

  return updateState(State::done, WaitingType::done_and_destroy);
}

/**
 * @brief Targt Start State: Get the incoming message and parse it
 * @param[in] args OpBox args (expecting incoming message)
 * @return WaitingType
 */
WaitingType OpPing::smt_Start(OpArgs *args){

  //Should be an incoming message with a standard string in it
  auto incoming_msg = args->ExpectMessageOrDie<message_t *>(&peer);

  //Get the user message and convert to all caps
  string s = UnpackStringMessage(incoming_msg);
  transform(s.begin(), s.end(), s.begin(), ::toupper);

  //Generate a reply, using info from the incoming msg and our string
  AllocateStringReplyMessage(ldo_msg, incoming_msg, 0, s);

  //Send it back
  opbox::net::SendMsg(peer, std::move(ldo_msg), AllEventsCallback(this) );
  return updateState(State::rcv_wait_for_reply_sent, WaitingType::waiting_on_cq);

}

/**
 * @brief Target Wait on Send Done- wait for the outgoing message to finish sending
 * @param[in] args OpBox args (expecting send_success)
 * @return WaitingType
 */
WaitingType OpPing::smt_WaitOnSent(OpArgs *args){
  args->VerifyTypeOrDie(UpdateType::send_success, op_name); //TODO: handle other messages
  return updateState(State::done, WaitingType::done_and_destroy);
}



/**
 * @brief Origin handle all state machine updates
 * @param[in] args New action to process
 * @return WaitingType
 */
WaitingType OpPing::UpdateOrigin(OpArgs *args) {
  switch(state){
  case State::start:                   return smo_Start();          //Send the initial message
  case State::snd_wait_for_sent:       return smo_WaitSend(args);   //wait for send to complete
  case State::snd_wait_for_reply:      return smo_WaitReply(args);  //wait for reply
  case State::done:                    return WaitingType::done_and_destroy;
  default: ;
  }
  F_FAIL(); //Should never reach here
  return WaitingType::error;
}

/**
 * @brief Target handle all state machine updates
 * @param[in] args New action to process
 * @return WaitingType
 */
WaitingType OpPing::UpdateTarget(OpArgs *args){
  switch(state){
  case State::start:                   return smt_Start(args);      //Build and send a reply
  case State::rcv_wait_for_reply_sent: return smt_WaitOnSent(args); //Waiting on outgoing message to leave
  case State::done:                    return WaitingType::done_and_destroy;
  default: ;
  }
  F_FAIL(); //Should never reach here
  return WaitingType::error;
}


/**
 * @brief Get string description of a state
 * @param[in] s The state to translate
 * @return string with name of the state
 */
string OpPing::str(State s) const {
  switch(s){
  case State::start:                   return "start";
  case State::snd_wait_for_sent:       return "origin-WaitForSent";
  case State::snd_wait_for_reply:      return "origin-WaitForReply";
  case State::rcv_wait_for_reply_sent: return "target-WaitForReplySent";
  case State::done:                    return "done";
  default: ;
  }
  return "unknown";
}


/**
 * @brief Get string description of current state
 * @return string
 */
string OpPing::GetStateName() const {
  return str(state);
}
