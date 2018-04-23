// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_OPKELPIEGETUNBOUNDED_HH
#define KELPIE_OPKELPIEGETUNBOUNDED_HH

#include "opbox/OpBox.hh"
#include "opbox/ops/OpHelpers.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An OpBox state machine for getting an unknown-sized object
 */
class OpKelpieGetUnbounded : public opbox::Op {

  //States
  enum class State : int {
    orig_getunbounded_send=0,
    trgt_getunbounded_start,
    trgt_getunbounded_wait_for_data,
    orig_getunbounded_wait_for_info,
    orig_getunbounded_wait_for_rdma,
    trgt_getunbounded_wait_for_ack,
    done };

public:

  //Get item with a known size
  OpKelpieGetUnbounded(
                    const faodel::nodeid_t target_node,
                    const net::peer_ptr_t target_ptr,
                    const faodel::bucket_t bucket,
                    const Key &key,
                    fn_opget_result_t cb_result);

  //A target starts off the same way no matter what command
  OpKelpieGetUnbounded(Op::op_create_as_target_t t);
  ~OpKelpieGetUnbounded();

  //Unique name and id for this op
  const static unsigned int op_id;
  const static std::string  op_name;
  unsigned int getOpID() const { return op_id; }
  std::string  getOpName() const { return op_name; }

  WaitingType Update(OpArgs *args) override; //Combined use
  WaitingType UpdateOrigin(OpArgs *args) {}  //Remove
  WaitingType UpdateTarget(OpArgs *args) {}  //Remove

  std::string GetStateName() const;

  static void configure(faodel::internal_use_only_t iuo, LocalKV *new_lkv);

private:
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
  WaitingType smo_GetUnbounded_Send();
  WaitingType smt_GetUnbounded_Start(opbox::OpArgs *args);
  WaitingType smt_GetUnbounded_WaitData(opbox::OpArgs *args);
  WaitingType smo_GetUnbounded_WaitInfo(opbox::OpArgs *args);
  WaitingType smo_GetUnbounded_WaitRDMA(opbox::OpArgs *args);
  WaitingType smt_GetUnbounded_WaitAck(opbox::OpArgs *args);



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

#endif  // KELPIE_OPKELPIEGETUNBOUNDED_HH
