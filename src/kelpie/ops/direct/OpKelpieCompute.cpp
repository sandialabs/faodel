// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <stdexcept>

#include "faodel-common/Debug.hh"
#include "kelpie/common/OpArgsObjectAvailable.hh"
#include "kelpie/core/Singleton.hh"

#include "kelpie/ops/direct/OpKelpieCompute.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;


//Statics: Standard id/name info for an op
const unsigned int OpKelpieCompute::op_id = const_hash("OpKelpieCompute");
const string OpKelpieCompute::op_name = "OpKelpieCompute";
bool OpKelpieCompute::debug_enabled = false;

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieCompute::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] config Pointer to configuration so that kelpie.op.getunbounded settings can be retrieved
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieCompute::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieCompute::debug_enabled, nullptr, nullptr, "kelpie.op.compute");
  }
}



/**
 * @brief Create a new remote compute operation
 *
 * @param[in] target_node The target node to query
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for the object
 * @param[in] key The key label for the object
 * @param[in] iom_hash Hash id of an IOM associated with this request
 * @param[in] behavior_flags Info about how to behave in different cases
 * @param[in] function_name The name of the user-defined function to call
 * @param[in] function_args A string of arguments to pass to the function *
 * @param[in] cb_result Callback function invoke when success/failure known
 * @return OpKelpieCompute
 */
OpKelpieCompute::OpKelpieCompute(
                    const faodel::nodeid_t target_node,
                    const net::peer_ptr_t target_ptr,
                    const faodel::bucket_t bucket,
                    const Key &key,
                    const iom_hash_t iom_hash,
                    const pool_behavior_t behavior_flags,
                    const string &function_name,
                    const string &function_args,
                    fn_compute_callback_t cb_result)
  : Op(true), state(State::orig_compute_send),
    peer(target_ptr), bucket(bucket), key(key),
    cb_compute_result(cb_result)  {

  //bool exceeds;

  //Create the outgoing message
  /*exceeds =*/ msg_direct_simple_t::Alloc(ldo_msg, op_id,
                                       DirectFlags::CMD_COMPUTE, target_node,
                                       GetAssignedMailbox(), opbox::MAILBOX_UNSPECIFIED,
                                       bucket, key, iom_hash, behavior_flags,
                                       function_name, function_args);
}


/**
 * @brief Create target-side handler for a new OpKelpieCompute
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieCompute
 */
OpKelpieCompute::OpKelpieCompute(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_compute_start), ldo_msg() {
  //No work to do - done in target's state machine
  peer = nullptr;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieCompute
 */
OpKelpieCompute::~OpKelpieCompute(){
  //cout <<"OpKelpieCompute() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    F_TODO("Dtor when state not done");
  }
}

//ORIGIN: Send the initial request
WaitingType OpKelpieCompute::smo_Compute_Send() {
  dbg("Send compute request for "+key.str());
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_compute_wait_for_info, WaitingType::waiting_on_cq);
}

//TARGET: Handle new request. Lookup. If here, send the data back. No, then wait
WaitingType OpKelpieCompute::smt_Compute_Start(OpArgs *args) {

  //Grab essential from this message for later user
  auto *imsg = args->ExpectMessageOrDie<msg_direct_simple_t *>(&peer);
  bucket = imsg->bucket;
  string function_name, function_args;
  imsg->ExtractComputeArgs(&key, &function_name, &function_args);

  dbg("Received new compute request for function "+function_name+" on key "+key.str()+" args "+function_args);

  //Have the lkv take care of all the fetching. We either get ok or op is queued up
  rc_t rc = lkv->doCompute(function_name, function_args,
                           bucket, key, &ldo_data);

  dbg("lkv-compute success was "+to_string(rc));


  if(rc==KELPIE_OK) {
    //Create an ack that includes buffer pointers
    msg_direct_buffer_t::Alloc(ldo_msg, op_id, DirectFlags::CMD_STATUS_ACK, imsg->hdr.src, GetAssignedMailbox(),
                               imsg->hdr.src_mailbox, bucket, key, 0, PoolBehavior::NoAction,
                               &ldo_data);
  } else {
    //Create a nack (no pointer)
    msg_direct_buffer_t::Alloc(ldo_msg, op_id, DirectFlags::CMD_STATUS_NACK, imsg->hdr.src, GetAssignedMailbox(),
                               imsg->hdr.src_mailbox, bucket, key, 0, PoolBehavior::NoAction,
                               nullptr);
  }
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::trgt_compute_wait_for_ack, WaitingType::waiting_on_cq);

}



//ORIGIN: Wait for the object to become available or for someone to cancel us
WaitingType OpKelpieCompute::smo_Compute_WaitInfo(OpArgs *args){

  //Grab essential from this message for later user
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);

  if(DirectFlags::IsNack(&imsg->hdr)) {
    //Nack, just finish the request
    dbg("Origin received Nack");

    cb_compute_result(KELPIE_ENOENT,  key, ldo_data);
    return updateStateDone();
  }

  if(imsg->meta_plus_data_size==0) {
    dbg("Origin received Ack, but no data");
    cb_compute_result(KELPIE_OK,  key, ldo_data);
    return updateStateDone();
  }

  dbg("Origin got ack and info for rdma");

  //Allocate object for the data and get it via an RDMA pull
  ldo_data = lunasa::DataObject(0, imsg->meta_plus_data_size, lunasa::DataObject::AllocatorType::eager);
  net::Get(peer, &imsg->net_buffer_remote, ldo_data, AllEventsCallback(this));

  msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);  //Success changed below

  return updateState(State::orig_compute_wait_for_rdma, WaitingType::waiting_on_cq);

}

//ORIGIN: Wait for RDMA done and Send ACK
WaitingType OpKelpieCompute::smo_Compute_WaitRDMA(OpArgs *args) {

  dbg("Done retrieving. Notifying target we are done");
  args->VerifyTypeOrDie(UpdateType::get_success, op_name); //TODO: Add better error handling

  //TODO: swap order with SendMsg?
  cb_compute_result(KELPIE_OK,  key, ldo_data); //TODO: handle not success

  auto omsg = ldo_msg.GetDataPtr<msg_direct_status_t *>();
  omsg->Success(true);
  net::SendMsg(peer, std::move(ldo_msg));

  return updateStateDone();
}

//TARGET: Wait for ACK to release LDO
WaitingType OpKelpieCompute::smt_Compute_WaitAck(OpArgs *args) {
  dbg("Received ack. Done.");
  /*auto imsg =*/ args->ExpectMessageOrDie<msg_direct_status_t *>();
  return updateStateDone();
}



WaitingType OpKelpieCompute::Update(opbox::OpArgs *args) {
  switch(state){
  case State::orig_compute_send:            return smo_Compute_Send();
  case State::trgt_compute_start:           return smt_Compute_Start(args);
  case State::orig_compute_wait_for_info:   return smo_Compute_WaitInfo(args);
  case State::orig_compute_wait_for_rdma:   return smo_Compute_WaitRDMA(args);
  case State::trgt_compute_wait_for_ack:    return smt_Compute_WaitAck(args);
  case State::done:                         return updateStateDone();
  }
  F_FAIL();
  return WaitingType::error;
}

/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieCompute::GetStateName() const {
  switch(state){
  case State::orig_compute_send:            return "Origin-Compute-Send";
  case State::trgt_compute_start:           return "Target-Compute-Start";
  case State::orig_compute_wait_for_info:   return "Origin-Compute-WaitForInfo";
  case State::orig_compute_wait_for_rdma:   return "Origin-Compute-WaitForRDMA";
  case State::trgt_compute_wait_for_ack:    return "Target-Compute-WaitForAck";
  case State::done:                         return "Done";
  }
  F_FAIL();
}


