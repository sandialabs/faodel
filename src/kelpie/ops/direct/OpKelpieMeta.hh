// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_OPKELPIEMETA_HH
#define KELPIE_OPKELPIEMETA_HH

#include "opbox/OpBox.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An OpBox state machine for retrieving meta data about an object
 */
class OpKelpieMeta : public opbox::Op {

  //States
  enum class State : int {
    orig_meta_send=0,
    trgt_meta_start,

    orig_wait_for_ack,

    done };

public:

  //Meta
  OpKelpieMeta(const uint16_t xferdirect_command, //Either CMD_COLINFO
               const faodel::nodeid_t target_node,
               const net::peer_ptr_t target_ptr,
               const faodel::bucket_t bucket,
               const Key &key,
               const kelpie::iom_hash_t iom_hash,
               fn_publish_callback_t cb_result);

  //A target starts off the same way no matter what command
  explicit OpKelpieMeta(Op::op_create_as_target_t t);
  ~OpKelpieMeta() override;

  //Unique name and id for this op
  const static unsigned int op_id;
  const static std::string  op_name;
  static bool debug_enabled; //!< Dump debug messages

  unsigned int getOpID() const override { return op_id; }
  std::string  getOpName() const override { return op_name; }

  WaitingType Update(OpArgs *args) override; //Combined use
  WaitingType UpdateOrigin(OpArgs *args) override { return WaitingType::error; }  //Remove
  WaitingType UpdateTarget(OpArgs *args) override { return WaitingType::error; }  //Remove

  std::string GetStateName() const override;

  static void configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, LocalKV *new_lkv);

private:

   //todo: put this in a standard form so it can be reused
  #if Faodel_LOGGINGINTERFACE_DISABLED==0
  void dbg(const std::string &s) const {
    if(OpKelpieMeta::debug_enabled) {
      std::cout << "\033[1;93mD " << op_name << ": ["<<GetStateName()<<"]:\033[0m\t" << (s) << std::endl;
    }
  }
  #else
  void dbg(std::string s) const {}
  #endif


  static LocalKV *lkv;  //Pointer back to the lkv, set at start time

  State state;

  net::peer_ptr_t       peer;            //For handoffs between ctro and start
  lunasa::DataObject    ldo_msg;         //Outgoing message, allocated/managed by net
  fn_publish_callback_t cb_info_result;  //Just returns rc and column info for destination

  //Origin/Target States (in order)
  WaitingType smo_Meta_Send();
  WaitingType smt_Meta_Start(opbox::OpArgs *args);
  WaitingType smo_WaitAck(opbox::OpArgs *args);

  WaitingType updateState(State new_state, WaitingType waiting_condition) {
    state=new_state;
    return waiting_condition;
  }
  WaitingType updateStateDone(){
    state=State::done;
    return WaitingType::done_and_destroy;
  }

};

}  // namespace kelpie

#endif  // KELPIE_OPKELPIEMETA_HH
