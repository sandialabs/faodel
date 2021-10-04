// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_OPKELPIEGETBOUNDED_HH
#define KELPIE_OPKELPIEGETBOUNDED_HH

#include "opbox/OpBox.hh"
#include "opbox/ops/OpHelpers.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An OpBox state machine for getting an object with a known size
 */
class OpKelpieGetBounded : public opbox::Op {

  //States
  enum class State : int {
    orig_getbounded_send=0,
    trgt_getbounded_start,
    trgt_getbounded_wait_for_data,
    trgt_getbounded_wait_for_rdma,
    orig_getbounded_wait_for_ack,
    done };

public:

  //Get item with a known size
  OpKelpieGetBounded(
          const faodel::nodeid_t target_node,
          const net::peer_ptr_t target_ptr,
          const faodel::bucket_t bucket,
          const Key &key,
          const size_t expected_ldo_user_size,
          const iom_hash_t iom_hash,
          const pool_behavior_t behavior_flags,
          fn_opget_result_t cb_result);

  //A target starts off the same way no matter what command
  OpKelpieGetBounded(Op::op_create_as_target_t t);
  ~OpKelpieGetBounded() override;

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
    if(OpKelpieGetBounded::debug_enabled) {
      std::cout << "\033[1;93mD " << op_name << ": ["<<GetStateName()<<"]:\033[0m\t" << (s) << std::endl;
    }
  }
  #else
  void dbg(std::string s) const {}
  #endif

  static LocalKV *lkv;  //Pointer back to the lkv, set at start time

  State state;
  net::peer_ptr_t peer;

  // We need to hold on to the nbr, bucket, and key in-between updates
  net::NetBufferRemote nbr;
  faodel::bucket_t bucket;
  Key key;

  lunasa::DataObject ldo_msg;     //Outgoing message, allocated/managed by net
  lunasa::DataObject ldo_data;    //Data to hold on to until complete

  fn_opget_result_t cb_opget_result;  //Returns rc, key, ldo, and column info

  //Origin/Target States (in order)
  WaitingType smo_GetBounded_Send();
  WaitingType smt_GetBounded_Start(opbox::OpArgs *args);
  WaitingType smt_GetBounded_WaitData(opbox::OpArgs *args);
  WaitingType smt_GetBounded_WaitRDMA(opbox::OpArgs *args);
  WaitingType smo_GetBounded_WaitAck(opbox::OpArgs *args);



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

#endif  // KELPIE_OPKELPIEGETBOUNDED_HH
