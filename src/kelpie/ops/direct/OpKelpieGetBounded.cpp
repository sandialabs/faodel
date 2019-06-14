// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <stdexcept>

#include "kelpie/core/Singleton.hh"
#include "kelpie/common/OpArgsObjectAvailable.hh"
#include "kelpie/ops/direct/OpKelpieGetBounded.hh"

using namespace std;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieGetBounded::op_id = const_hash("OpKelpieGetBounded");
const string OpKelpieGetBounded::op_name = "OpKelpieGetBounded";

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieGetBounded::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieGetBounded::configure(faodel::internal_use_only_t iuo, LocalKV *new_lkv) {
  lkv = new_lkv;
}



/**
 * @brief Create a new Get Object operation
 *

 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
 * @param[in] expected_ldo_user_size Optional expected size of the data
 * @param[in] iom_hash Hash id of an IOM associated with this request
 * @param[in] behavior_flags Info about how to behave in different cases
 * @param[in] cb_result Callback function invoke when success/failure known
 * @return OpKelpieGetBounded
 *
 * @note This op launches either a bounded get or an unbounded get, depending
 *       on whether the expected user size is known or not
 */
OpKelpieGetBounded::OpKelpieGetBounded(
                    const faodel::nodeid_t target_node,
                    const net::peer_ptr_t target_ptr,
                    const faodel::bucket_t bucket,
                    const Key &key,
                    const size_t expected_ldo_user_size,
                    const iom_hash_t iom_hash,
                    const pool_behavior_t behavior_flags,
                    fn_opget_result_t cb_result)
  : Op(true),
    state(State::orig_getbounded_send), peer(target_ptr),
    bucket(bucket), key(key),
    cb_opget_result(cb_result)  {

  kassert(expected_ldo_user_size>0, "GetBounded op given a zero byte ldo?");

  //Create space for the requested data
  ldo_data = lunasa::DataObject(0, expected_ldo_user_size, lunasa::DataObject::AllocatorType::eager);

  //Create the outgoing message
  msg_direct_buffer_t::Alloc(ldo_msg, op_id,
                                       DirectFlags::CMD_GET_BOUNDED, target_node,
                                       GetAssignedMailbox(), opbox::MAILBOX_UNSPECIFIED,
                                       bucket, key, iom_hash, behavior_flags,
                                       &ldo_data);
}


/**
 * @brief Create target-side handler for a new OpKelpieGetBounded
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieGetBounded
 */
OpKelpieGetBounded::OpKelpieGetBounded(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_getbounded_start), ldo_msg()  {
  //No work to do - done in target's state machine
  peer = 0;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieGetBounded
 */
OpKelpieGetBounded::~OpKelpieGetBounded(){
  //cout <<"OpKelpieGetBounded() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    KTODO("GetBounded dtor called when not in done state");
  }
}

//ORIGIN: Send the initial request
WaitingType OpKelpieGetBounded::smo_GetBounded_Send(){
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_getbounded_wait_for_ack, WaitingType::waiting_on_cq);
}

//TARGET: Handle new request. Lookup. If here, send the data back. No, then wait
WaitingType OpKelpieGetBounded::smt_GetBounded_Start(OpArgs *args) {

  //Grab essential from this message for later user
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);
  nbr    = imsg->net_buffer_remote;  //Copy the remote pointers
  bucket = imsg->bucket;
  key    = imsg->ExtractKey();
  auto target_iom = imsg->iom_hash;

  //cout <<"OPDHT-TRG: message is get bounded for "<<key.str()<<endl;

  //Create an ack message (though success may change below)
  auto omsg = msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);  //Success changed below

  //Lookup data
  rc_t rc = lkv->get(imsg->bucket, key, mailbox, &ldo_data, &omsg->row_info, &omsg->col_info);


  //TODO: Provide row/col info back in ack

  //When miss, see if we need to try loading from an iom
  if((rc!=0) && (target_iom!=0)){

    auto *iom = kelpie::internal::Singleton::impl.core->FindIOM(target_iom);
    if(iom==nullptr) {
      throw runtime_error("OpKelpieGetBounded attempted to read key "+key.str()+" to a node with a bad iom");
    }
    rc = iom->ReadObject(imsg->bucket, key, &ldo_data);

    if(rc==0) {
      //TODO: We need to post this, without triggering our mailbox to be sent
      auto target_behavior_flags = PoolBehavior::ChangeRemoteToLocal(imsg->behavior_flags);
      lkv->put(imsg->bucket, key, ldo_data, target_behavior_flags, &omsg->row_info, &omsg->col_info);
    }
  }


  if(rc==0){
    //cout <<"OPDHT-TRG Result is ready and am putting\n";
    //Data was in memory, get ready to send it back
    //TODO: Does put check lengths at all?
    net::Put(peer, ldo_data, &nbr, AllEventsCallback(this));
    omsg->Success(true);
    return updateState(State::trgt_getbounded_wait_for_rdma, WaitingType::waiting_on_cq);

  } else {
    //cout <<"OPDHT-TRG Result not ready\n";


    //Option 1: Just send a nack
    //omsg->Success(false);
    //net::SendMsg(peer, ldo_msg);
    //return updateStateDone();

    //Option 2: Stall this op and wait for the data
    return updateState(State::trgt_getbounded_wait_for_data, WaitingType::wait_on_user);

    //Option 3?: Stall this op and wait/timeout for the data

  }
}

//TARGET: Data now available.  Start the RDMA.
WaitingType OpKelpieGetBounded::smt_GetBounded_WaitData(OpArgs *args){

  //cout <<"OPDHT-TRG: getbounded wait for data\n";

  //See if this is a timeout
  if(args->type == opbox::UpdateType::timeout){
    //Send a nack and be done. Message ready to go, just toggle the success
    auto omsg = ldo_msg.GetDataPtr<msg_direct_status_t *>();
    omsg->Success(false);
    net::SendMsg(peer, std::move(ldo_msg));
    return updateStateDone();
  }

  //TODO: We should do more error checking before recasting OpArgs to make sure
  //      it's actually the OpArgs we're expecting.
  if(args->type != opbox::UpdateType::user_trigger){
    throw std::runtime_error("OpKelpieGetBounded_Target was expecting user trigger in GetBounded_WaitData()?");
  }

  //Data now available. Setup send
  auto opargs = reinterpret_cast<OpArgsObjectAvailable *>(args);
  ldo_data = opargs->ldo;  //Transfer ownership here
  net::Put(peer, ldo_data, &nbr, AllEventsCallback(this));
  return updateState(State::trgt_getbounded_wait_for_rdma, WaitingType::waiting_on_cq);

}

//TARGET: RDMA PUT is Done, send the ACK notification
WaitingType OpKelpieGetBounded::smt_GetBounded_WaitRDMA(OpArgs *args) {

  //cout <<"OPDHT-TRG: getbounded wait for rdma, sending ack\n";
  args->VerifyTypeOrDie(UpdateType::put_success, op_name);  //TODO: Add better error handling

  //RDMA done, now send an ACK message
  net::SendMsg(peer, std::move(ldo_msg));
  return updateStateDone();
}


//ORIGIN: Received an ACK/NACK. Pass results back to user callback
WaitingType OpKelpieGetBounded::smo_GetBounded_WaitAck(OpArgs *args) {

  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();

  //cout <<"OPXFER-ORG getbounded "<< (imsg->Success() ? "Success":"Fail") <<imsg->str();

  cb_opget_result(imsg->Success(), key, ldo_data);

  //cout <<"OPXFER-ORG: Done with callback\n";
  return updateStateDone();
}


WaitingType OpKelpieGetBounded::Update(opbox::OpArgs *args) {
  //cout <<"OPGETB-Update "<< GetStateName()<<" ArgsL "<<args->str(2,1)<<endl;
  switch(state){
  case State::orig_getbounded_send:            return smo_GetBounded_Send();
  case State::trgt_getbounded_start:           return smt_GetBounded_Start(args);
  case State::trgt_getbounded_wait_for_data:   return smt_GetBounded_WaitData(args);
  case State::trgt_getbounded_wait_for_rdma:   return smt_GetBounded_WaitRDMA(args);
  case State::orig_getbounded_wait_for_ack:    return smo_GetBounded_WaitAck(args);
  case State::done:                            return updateStateDone();
  }
  KFAIL();
  return WaitingType::error;
}


/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieGetBounded::GetStateName() const {
  switch(state){
  case State::orig_getbounded_send:            return "Origin-GetBounded-Send";
  case State::trgt_getbounded_start:           return "Target-GetBounded-Start";
  case State::trgt_getbounded_wait_for_data:   return "Target-GetBounded-WaitForData";
  case State::trgt_getbounded_wait_for_rdma:   return "Target-GetBounded-WaitForRDMA";
  case State::orig_getbounded_wait_for_ack:    return "Origin-GetBounded-WaitForAck";
  case State::done:                            return "Done";
  }
  KFAIL();
}

