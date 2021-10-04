// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <stdexcept>

#include "faodel-common/Debug.hh"
#include "kelpie/common/OpArgsObjectAvailable.hh"
#include "kelpie/core/Singleton.hh"

#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieGetUnbounded::op_id = const_hash("OpKelpieGetUnbounded");
const string OpKelpieGetUnbounded::op_name = "OpKelpieGetUnbounded";
bool OpKelpieGetUnbounded::debug_enabled = false;

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieGetUnbounded::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] config Pointer to configuration so that kelpie.op.getunbounded settings can be retrieved
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieGetUnbounded::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieGetUnbounded::debug_enabled, nullptr, nullptr, "kelpie.op.getunbounded");
  }
}



/**
 * @brief Create a new Get Object operation
 *
 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
 * @param[in] iom_hash Hash id of an IOM associated with this request
 * @param[in] behavior_flags Info about how to behave in different cases
 * @param[in] cb_result Callback function invoke when success/failure known
 * @return OpKelpieGetUnbounded
 *
 * @note This op launches an unbounded get, depending
 *       on whether the expected user size is known or not
 */
OpKelpieGetUnbounded::OpKelpieGetUnbounded(
                    const faodel::nodeid_t target_node,
                    const net::peer_ptr_t target_ptr,
                    const faodel::bucket_t bucket,
                    const Key &key,
                    const iom_hash_t iom_hash,
                    const pool_behavior_t behavior_flags,
                    fn_opget_result_t cb_result)
  : Op(true), state(State::orig_getunbounded_send),
    peer(target_ptr), bucket(bucket), key(key),
    cb_opget_result(cb_result)  {

  //bool exceeds;

  //Create the outgoing message
  /*exceeds =*/ msg_direct_simple_t::Alloc(ldo_msg, op_id,
                                       DirectFlags::CMD_GET_UNBOUNDED, target_node,
                                       GetAssignedMailbox(), opbox::MAILBOX_UNSPECIFIED,
                                       bucket, key, iom_hash, behavior_flags);
}


/**
 * @brief Create target-side handler for a new OpKelpieGetUnbounded
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieGetUnbounded
 */
OpKelpieGetUnbounded::OpKelpieGetUnbounded(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_getunbounded_start), ldo_msg() {
  //No work to do - done in target's state machine
  peer = nullptr;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieGetUnbounded
 */
OpKelpieGetUnbounded::~OpKelpieGetUnbounded(){
  //cout <<"OpKelpieGetUnbounded() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    F_TODO("Dtor when state not done");
  }
}

//ORIGIN: Send the initial request
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_Send() {
  dbg("Send unbounded request for "+key.str());
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_getunbounded_wait_for_info, WaitingType::waiting_on_cq);
}

//TARGET: Handle new request. Lookup. If here, send the data back. No, then wait
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_Start(OpArgs *args) {

  //Grab essential from this message for later use
  auto imsg = args->ExpectMessageOrDie<msg_direct_simple_t *>(&peer);
  bucket = imsg->bucket;
  key    = imsg->ExtractKey();

  dbg("Received new unbounded request for "+key.str());

  //Create a message that can hold our buffer pointer
  msg_direct_buffer_t::Alloc(ldo_msg, op_id, DirectFlags::CMD_GET_UNBOUNDED, imsg->hdr.src, GetAssignedMailbox(),
                             imsg->hdr.src_mailbox, bucket, key, 0, PoolBehavior::NoAction, nullptr);


  //Have the lkv take care of all the fetching. We either get ok or op is queued up
  rc_t rc = lkv->getForOp(bucket, key, mailbox, imsg->behavior_flags, imsg->iom_hash,
                          &ldo_data, nullptr); //todo: does not pass back row/col info


  dbg("lkv-get success was "+to_string(rc)+" iom hash is "+to_string(imsg->iom_hash));

  if(rc==KELPIE_OK) {
    dbg("Item located. Sending pointers");
    auto omsg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();
    omsg->SetLDO(&ldo_data);
    net::SendMsg(peer, std::move(ldo_msg));
    return updateState(State::trgt_getunbounded_wait_for_ack, WaitingType::waiting_on_cq);

  } else {
    dbg("Item Not available. Waiting for it to be published.");
    return updateState(State::trgt_getunbounded_wait_for_data, WaitingType::wait_on_user);
  }

}

//TARGET: Wait for the object to become available or for someone to cancel us
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_WaitData(OpArgs *args){

  if(args->type != opbox::UpdateType::user_trigger){
    throw std::runtime_error("OpDirect_Target was expecting user trigger in GetUnbounded_WaitData()?");
  }

  auto opargs = reinterpret_cast<OpArgsObjectAvailable *>(args);
  ldo_data = opargs->ldo; //Hold on to a copy

  dbg("Data available. Sending info.");

  //Add the buffer pointer to the outgoing message we created in previous step, then send it
  auto omsg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();
  omsg->SetLDO(&ldo_data);
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::trgt_getunbounded_wait_for_ack, WaitingType::waiting_on_cq);
}


//ORIGIN: Wait for the object to become available or for someone to cancel us
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_WaitInfo(OpArgs *args){

  //Grab essential from this message for later user
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);

  //Make sure not a nack. Current code does not do this, but future version may have a cancel
  if(imsg->meta_plus_data_size==0) {
    F_TODO("GetUnbounded SMO NACK not implemented");
  }
  dbg("Retrieving data");

  //Create an ack TODO: move to after Get when opbox handles this right
  msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);  //Success changed below

  //Allocate object for the data and get it via an RDMA pull
  ldo_data = lunasa::DataObject(0, imsg->meta_plus_data_size, lunasa::DataObject::AllocatorType::eager);
  net::Get(peer, &imsg->net_buffer_remote, ldo_data, AllEventsCallback(this));
  return updateState(State::orig_getunbounded_wait_for_rdma, WaitingType::waiting_on_cq);
}

//ORIGIN: Wait for RDMA done and Send ACK
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_WaitRDMA(OpArgs *args) {

  dbg("Done retrieving. Notifying target we are done");
  args->VerifyTypeOrDie(UpdateType::get_success, op_name);

  cb_opget_result(true, key, ldo_data); //Current retrieval ends in success. Future code may have timeout/cancels

  //Send back an ack to release the target
  auto omsg = ldo_msg.GetDataPtr<msg_direct_status_t *>();
  omsg->Success(true);
  net::SendMsg(peer, std::move(ldo_msg));

  return updateStateDone();
}

//TARGET: Wait for ACK to release LDO
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_WaitAck(OpArgs *args) {
  dbg("Received ack. Done.");
  /*auto imsg =*/ args->ExpectMessageOrDie<msg_direct_status_t *>();
  return updateStateDone();
}



WaitingType OpKelpieGetUnbounded::Update(opbox::OpArgs *args) {
  switch(state){
  case State::orig_getunbounded_send:            return smo_GetUnbounded_Send();
  case State::trgt_getunbounded_start:           return smt_GetUnbounded_Start(args);
  case State::trgt_getunbounded_wait_for_data:   return smt_GetUnbounded_WaitData(args);
  case State::orig_getunbounded_wait_for_info:   return smo_GetUnbounded_WaitInfo(args);
  case State::orig_getunbounded_wait_for_rdma:   return smo_GetUnbounded_WaitRDMA(args);
  case State::trgt_getunbounded_wait_for_ack:    return smt_GetUnbounded_WaitAck(args);
  case State::done:                              return updateStateDone();
  }
  F_FAIL();
  return WaitingType::error;
}

/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieGetUnbounded::GetStateName() const {
  switch(state){
  case State::orig_getunbounded_send:            return "Origin-GetUnbounded-Send";
  case State::trgt_getunbounded_start:           return "Target-GetUnbounded-Start";
  case State::trgt_getunbounded_wait_for_data:   return "Target-GetUnbounded-WaitForData";
  case State::orig_getunbounded_wait_for_info:   return "Origin-GetUnbounded-WaitForInfo";
  case State::orig_getunbounded_wait_for_rdma:   return "Origin-GetUnbounded-WaitForRDMA";
  case State::trgt_getunbounded_wait_for_ack:    return "Target-GetUnbounded-WaitForAck";
  case State::done:                              return "Done";
  }
  F_FAIL();
}

