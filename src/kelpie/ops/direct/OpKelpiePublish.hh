// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_OPKELPIEPUBLISH_HH
#define KELPIE_OPKELPIEPUBLISH_HH

#include "opbox/OpBox.hh"
#include "opbox/ops/OpHelpers.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An OpBox state machine for publishing an object to a remote node
 */
class OpKelpiePublish : public opbox::Op {

  //States
  enum class State : int {
    orig_pub_send=0,
    trgt_pub_start,
    trgt_pub_wait_for_rdma,
    orig_pub_wait_for_ack,
    done };

public:

  //Publish
  OpKelpiePublish(  const faodel::nodeid_t target_node,
                    const net::peer_ptr_t target_ptr,
                    const faodel::bucket_t bucket,
                    const Key &key,
                    const lunasa::DataObject &ldo_users_data,
                    fn_publish_callback_t callback);

  //A target starts off the same way no matter what command
  OpKelpiePublish(Op::op_create_as_target_t t);
  ~OpKelpiePublish();

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

  lunasa::DataObject  ldo_msg;     //Outgoing message, allocated/managed by net
  lunasa::DataObject  ldo_data;    //Data to hold on to until complete

  fn_publish_callback_t cb_info_result;  //Just returns rc and column info for destination

  //Origin/Target States (in order)
  WaitingType smo_Publish_Send();
  WaitingType smt_Publish_Start(opbox::OpArgs *args);
  WaitingType smt_Publish_WaitRDMA(opbox::OpArgs *args);
  WaitingType smo_Publish_WaitAck(opbox::OpArgs *args);


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

#endif  // KELPIE_OPKELPIEPUBLISH_HH
