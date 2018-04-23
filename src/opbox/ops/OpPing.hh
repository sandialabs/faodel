// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPPING_HH
#define OPBOX_OPPING_HH

#include <future>

#include "lunasa/DataObject.hh"

#include "opbox/ops/Op.hh"

namespace opbox {

/**
 * @brief OpPing provides a simple mechanism for pinging a remote node
 *
 * This Op takes a string from the origin and transmits it in an op
 * message to the destination. The destination converts the string to
 * uppercase and then transmits the message back.
 */
class OpPing : public Op {

  enum class State : int  {
      start=0,
      snd_wait_for_sent,
      snd_wait_for_reply,
      rcv_wait_for_reply_sent,
      done };

public:
  OpPing(opbox::net::peer_ptr_t dst, std::string ping_message);

  explicit OpPing(op_create_as_target_t t);
  ~OpPing() override;

  //User can get a future for receiving reply
  std::future<std::string> GetFuture();

  //For Op interface
  const static unsigned int op_id;
  const static std::string  op_name;
  unsigned int getOpID() const override { return op_id; }
  std::string getOpName() const override { return op_name; }
  WaitingType UpdateOrigin(OpArgs *args) override;
  WaitingType UpdateTarget(OpArgs *args) override;

  std::string GetStateName() const override;

private:

  //State Machine functions (smo=origin, smt=target)
  WaitingType smo_Start();
  WaitingType smo_WaitSend(opbox::OpArgs *args);
  WaitingType smo_WaitReply(opbox::OpArgs *args);

  WaitingType smt_Start(OpArgs *args);
  WaitingType smt_WaitOnSent(OpArgs *args);

  std::string str(State s) const;

  State state;

  std::promise<std::string> ping_promise;
  std::future<std::string>  ping_future;

  opbox::net::peer_t *peer;
  lunasa::DataObject  ldo_msg;


  /**
   * @brief Helper function updates state and passes back waiting condiftion in one line
   *
   * @param[in] new_state The state we go to next
   * @param[in] waiting_condition The waiting type to return to opbox for this state machine
   * @return WaitingType
   */
  WaitingType updateState(State new_state, WaitingType waiting_condition) {
    state=new_state;
    return waiting_condition;
  }

};

} // namespace opbox

#endif // OPBOX_OPPING_HH
