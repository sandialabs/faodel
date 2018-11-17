// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <kelpie/core/Singleton.hh>

#include "kelpie/ops/direct/OpKelpiePublish.hh"

using namespace std;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpiePublish::op_id = const_hash("OpKelpiePublish");
const string OpKelpiePublish::op_name = "OpKelpiePublish";

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpiePublish::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpiePublish::configure(faodel::internal_use_only_t iuo, LocalKV *new_lkv) {
  lkv = new_lkv;
}


/**
 * @brief Create a new Publish Object operation
 *
 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
 * @param[in] ldo_users_data The ldo for the data
 * @param[in] iom_hash Hash id of an IOM associated with this request
 * @param[in] behavior_flags Info about what actions to take on remote side
 * @param[in] cb_result Callback function invoke when success/failure is known
 * @return OpKelpiePublish
 */
OpKelpiePublish::OpKelpiePublish(
                     const faodel::nodeid_t target_node,
                     const net::peer_ptr_t target_ptr,
                     const faodel::bucket_t bucket,
                     const Key &key,
                     const lunasa::DataObject &ldo_users_data,
                     const iom_hash_t iom_hash,
                     const pool_behavior_t behavior_flags,
                     fn_publish_callback_t cb_result)
  : state(State::orig_pub_send),
    peer(target_ptr),
    cb_info_result(cb_result), Op(true) {

  ldo_data = ldo_users_data;  //We need to keep a copy of the ldo until sent

  bool exceeds = msg_direct_buffer_t::Alloc(ldo_msg, op_id, DirectFlags::CMD_PUBLISH, target_node, GetAssignedMailbox(),
                                            opbox::MAILBOX_UNSPECIFIED, bucket, key, iom_hash, behavior_flags, &ldo_data);

}

/**
 * @brief Create target-side handler for a new OpKelpiePublish
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpiePublish
 */
OpKelpiePublish::OpKelpiePublish(Op::op_create_as_target_t t)
  : state(State::trgt_pub_start), ldo_msg(), Op(t) {

  //No work to do - done in target's state machine
  peer = 0;
  target_iom=0;
  target_behavior_flags=0;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpiePublish
 */
OpKelpiePublish::~OpKelpiePublish(){
  //cout <<"OpKelpiePublish() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    //KTODO("OpKelpiePublish not in done state when destroyed");
  }
}



//ORIGIN: Start a new publish operation by sending a message
WaitingType OpKelpiePublish::smo_Publish_Send(){
  //All origin options start here, but branch out to different states
  //cout <<"OPPUB-ORG: About to send message\n";

  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_pub_wait_for_ack, WaitingType::waiting_on_cq);
}


//TARGET: Get a new request, Pull the data
WaitingType OpKelpiePublish::smt_Publish_Start(OpArgs *args) {
  //cout <<"OPPUB-TRG: Got a start\n";
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);

  //Grab essential from this message for later user
  bucket     = imsg->bucket;
  key        = imsg->ExtractKey();
  nbr        = imsg->net_buffer_remote;  //Copy the remote pointers
  target_iom = imsg->iom_hash;
  target_behavior_flags = PoolBehavior::ChangeRemoteToLocal(imsg->behavior_flags);

  //cout <<"OPPUB-TRG: message is a publish. OMBox="<<imsg->hdr.src_mailbox<<" Meta+Data Length is "<<imsg->meta_plus_data_size<<" key is "<<key.str()<<"\n";

  //TODO: See if Kelpie already has this object

  //Allocate space for incoming data
  ldo_data = lunasa::DataObject(0, imsg->meta_plus_data_size, lunasa::DataObject::AllocatorType::eager);
  //cout <<"OPPUB-TRG ldo allocated size is "<<ldo_data.GetDataSize()<<endl;

  //Create return ack message TODO: move below GET when opbox is safe
  msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);

  //Setup RDMA for transferring data
  net::Get(peer, &nbr, ldo_data, AllEventsCallback(this));

  return updateState(State::trgt_pub_wait_for_rdma, WaitingType::waiting_on_cq);

}

//TARGET: RDMA Completes, send an ACK
WaitingType OpKelpiePublish::smt_Publish_WaitRDMA(opbox::OpArgs *args){

  //cout <<"OPPUB-TRG: got dma done notification. sending ack. Key is "<<key.str()<<"\n";

  //FIXME
  if(args->type == UpdateType::send_success) {
    cout<<"FIXME: got send_success instead of get_success\n";
    return WaitingType::waiting_on_cq;
  }
  //FIXME^^

  args->VerifyTypeOrDie(UpdateType::get_success, op_name);  //TODO: Add better error handling

  auto omsg = ldo_msg.GetDataPtr<msg_direct_status_t *>();

  //Got the data, now store it locally in memory
  rc_t rc = lkv->put(bucket, key, ldo_data, target_behavior_flags, &omsg->row_info, &omsg->col_info);

  //See if it also goes to an iom
  if((target_iom!=0) && (target_behavior_flags & PoolBehavior::WriteToIOM)){
    auto *iom = kelpie::internal::Singleton::impl.core->FindIOM(target_iom);
    if(iom==nullptr) {
      throw runtime_error("OpKelpiePublish attempted to write key "+key.str()+" to a node with a bad iom");
    }
    iom->WriteObject(bucket, key, ldo_data);
  }

  omsg->Success(rc==KELPIE_OK);
  omsg->remote_rc = rc;

  net::SendMsg(peer, std::move(ldo_msg));
  return updateStateDone();

}

//ORIGIN: Wait for an ACK
WaitingType OpKelpiePublish::smo_Publish_WaitAck(opbox::OpArgs *args) {
  //cout <<"OPPUB-ORG: got ack\n";

  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();
  //TODO: better handling of unexpected arg/message

  //Got a reply. We no longer have to hold on to the publish data's ldo. If
  //caller specified a callback, pass the result back
  if(cb_info_result!=nullptr){
    //cout <<"OPPUB-ORG got ack Reply. Need to extract the return code\n";
    imsg->row_info.ChangeAvailabilityFromLocalToRemote();
    imsg->col_info.ChangeAvailabilityFromLocalToRemote();
    cb_info_result(imsg->remote_rc, imsg->row_info, imsg->col_info);
  }
  return updateStateDone();
}


WaitingType OpKelpiePublish::Update(opbox::OpArgs *args){
  //cout <<"OPPUB-Update "<< GetStateName()<<" Args: "<<args->str(2,1)<<endl;
  switch(state){
  case State::orig_pub_send:                   return smo_Publish_Send();
  case State::trgt_pub_start:                  return smt_Publish_Start(args);
  case State::trgt_pub_wait_for_rdma:          return smt_Publish_WaitRDMA(args);
  case State::orig_pub_wait_for_ack:           return smo_Publish_WaitAck(args);
  case State::done:                            return updateStateDone();
  default: ;
  }
  KFAIL();
  return WaitingType::error;
}

/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpiePublish::GetStateName() const {
  switch(state){
  case State::orig_pub_send:                   return "Origin-Publish-Send";
  case State::trgt_pub_start:                  return "Target-Publish-Start";
  case State::trgt_pub_wait_for_rdma:          return "Target-Publish-WaitForRDMA";
  case State::orig_pub_wait_for_ack:           return "Origin-Publish-WaitForAck";
  case State::done:                            return "Done";
  }
  KFAIL();
}
