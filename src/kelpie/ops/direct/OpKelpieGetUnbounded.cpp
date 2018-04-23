// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "common/Debug.hh"
#include "kelpie/common/OpArgsObjectAvailable.hh"

#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieGetUnbounded::op_id = const_hash("OpKelpieGetUnbounded");
const string OpKelpieGetUnbounded::op_name = "OpKelpieGetUnbounded";

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieGetUnbounded::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieGetUnbounded::configure(faodel::internal_use_only_t iuo, LocalKV *new_lkv) {
  lkv = new_lkv;
}



/**
 * @brief Create a new Get Object operation
 *
 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
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
                    fn_opget_result_t cb_result)
  : state(State::orig_getunbounded_send),
    bucket(bucket), key(key), peer(target_ptr),
    cb_opget_result(cb_result), Op(true) {

  bool exceeds;

  //Create the outgoing message
  exceeds = msg_direct_buffer_t::Alloc(ldo_msg,
                                           op_id,
                                           DirectFlags::CMD_GET_UNBOUNDED,
                                           target_node,
                                           GetAssignedMailbox(),
                                           opbox::MAILBOX_UNSPECIFIED,
                                           bucket, key,
                                           nullptr);
}


/**
 * @brief Create target-side handler for a new OpKelpieGetUnbounded
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieGetUnbounded
 */
OpKelpieGetUnbounded::OpKelpieGetUnbounded(Op::op_create_as_target_t t)
  : state(State::trgt_getunbounded_start), ldo_msg(), Op(t) {
  //No work to do - done in target's state machine
  peer = 0;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieGetUnbounded
 */
OpKelpieGetUnbounded::~OpKelpieGetUnbounded(){
  //cout <<"OpKelpieGetUnbounded() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    KTODO("Dtor when state not done");
  }
}

//ORIGIN: Send the initial request
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_Send(){
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_getunbounded_wait_for_info, WaitingType::waiting_on_cq);
}

//TARGET: Handle new request. Lookup. If here, send the data back. No, then wait
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_Start(OpArgs *args) {

  //Grab essential from this message for later user
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);
  bucket = imsg->bucket;
  key    = imsg->ExtractKey();

  //cout <<"OPDHT-TRG: message is get unbounded for "<<key.str()<<endl;

  //Allocate a reply message that has our pointers in it


  //Lookup the item
  rc_t rc = lkv->get(imsg->bucket, key, mailbox, &ldo_data, nullptr, nullptr);
  if(rc==0) {
    //Item is ready. Send the origin the pointers to its data
    msg_direct_buffer_t::Alloc(ldo_msg,
                                   op_id,
                                   DirectFlags::CMD_GET_UNBOUNDED,
                                   imsg->hdr.src,
                                   GetAssignedMailbox(),
                                   imsg->hdr.src_mailbox,
                                   bucket, key,
                                   &ldo_data); //We have the ldo

    net::SendMsg(peer, std::move(ldo_msg));

    return updateState(State::trgt_getunbounded_wait_for_ack, WaitingType::waiting_on_cq);

  } else {
    //Object was not here. We need to nack or wait

    msg_direct_buffer_t::Alloc(ldo_msg,
                                   op_id,
                                   DirectFlags::CMD_GET_UNBOUNDED,
                                   imsg->hdr.src,
                                   GetAssignedMailbox(),
                                   imsg->hdr.src_mailbox,
                                   bucket, key,
                                   nullptr); //We don't know the ldo yet

    return updateState(State::trgt_getunbounded_wait_for_data, WaitingType::wait_on_user);
    KTODO("WaitOnUser states?");

  }
}
//TARGET: Wait for the object to become available or for someone to cancel us
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_WaitData(OpArgs *args){

  if(args->type != opbox::UpdateType::user_trigger){
    throw std::runtime_error("OpDirect_Target was expecting user trigger in GetUnbounded_WaitData()?");
  }

  auto opargs = reinterpret_cast<OpArgsObjectAvailable *>(args);
  ldo_data = opargs->ldo; //Hold on to a copy

  //Fill in the LDO's size and nbr info
  auto omsg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();
  omsg->SetLDO(&ldo_data);


  //TODO: Should col_info be placed here?

  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::trgt_getunbounded_wait_for_ack, WaitingType::waiting_on_cq);
}


//ORIGIN: Wait for the object to become available or for someone to cancel us
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_WaitInfo(OpArgs *args){

  //Grab essential from this message for later user
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);

  //TODO: Make sure not a nack
  if(imsg->meta_plus_data_size==0){
    KTODO("GetUnbounded SMO NACK not implemented");
  }

  //Create an ack TODO: move to after Get when opbox handles this right
  auto omsg = msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);  //Success changed below

  //Allocate object and setup Get
  ldo_data = lunasa::DataObject(0, imsg->meta_plus_data_size, lunasa::DataObject::AllocatorType::eager);
  net::Get(peer, &imsg->net_buffer_remote, ldo_data, AllEventsCallback(this));
  return updateState(State::orig_getunbounded_wait_for_rdma, WaitingType::waiting_on_cq);
}

//ORIGIN: Wait for RDMA done and Send ACK
WaitingType OpKelpieGetUnbounded::smo_GetUnbounded_WaitRDMA(OpArgs *args) {

  args->VerifyTypeOrDie(UpdateType::get_success, op_name); //TODO: Add better error handling

  //TODO: swap order with SendMsg?
  cb_opget_result(true, key, ldo_data); //TODO: handle not success

  auto omsg = ldo_msg.GetDataPtr<msg_direct_status_t *>();
  omsg->Success(true);
  net::SendMsg(peer, std::move(ldo_msg));

  return updateStateDone();
}

//TARGET: Wait for ACK to release LDO
WaitingType OpKelpieGetUnbounded::smt_GetUnbounded_WaitAck(OpArgs *args) {
  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();
  return updateStateDone();
}



WaitingType OpKelpieGetUnbounded::Update(opbox::OpArgs *args) {
  //cout <<"OPGETU-Update "<< GetStateName()<<" Args: "<<args->str(2,1)<<endl;
  switch(state){
  case State::orig_getunbounded_send:            return smo_GetUnbounded_Send();
  case State::trgt_getunbounded_start:           return smt_GetUnbounded_Start(args);
  case State::trgt_getunbounded_wait_for_data:   return smt_GetUnbounded_WaitData(args);
  case State::orig_getunbounded_wait_for_info:   return smo_GetUnbounded_WaitInfo(args);
  case State::orig_getunbounded_wait_for_rdma:   return smo_GetUnbounded_WaitRDMA(args);
  case State::trgt_getunbounded_wait_for_ack:    return smt_GetUnbounded_WaitAck(args);
  case State::done:                              return updateStateDone();
  }
  KFAIL();
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
  KFAIL();
}

