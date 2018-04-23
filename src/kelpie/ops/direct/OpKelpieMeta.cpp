// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "common/Debug.hh"
#include "kelpie/ops/direct/OpKelpieMeta.hh"

using namespace std;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieMeta::op_id = const_hash("OpKelpieMeta");
const string OpKelpieMeta::op_name = "OpKelpieMeta";

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieMeta::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieMeta::configure(faodel::internal_use_only_t iuo, LocalKV *new_lkv) {
  lkv = new_lkv;
}


/**
 * @brief Create a new colinfo op
 *
 * @param[in] xferdirect_command Type of XferdDirect command this is
 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
 * @param[in] cb_result Callback function invoke when success/failure is known
 * @return OpKelpieMeta
 */
OpKelpieMeta::OpKelpieMeta(
                     const uint16_t xferdirect_command,
                     const faodel::nodeid_t target_node,
                     const net::peer_ptr_t target_ptr,
                     const faodel::bucket_t bucket,
                     const Key &key,
                     fn_publish_callback_t cb_result)
  :
    peer(target_ptr),
    cb_info_result(cb_result), Op(true) {

  bool exceeds = msg_direct_buffer_t::Alloc(ldo_msg,
                                                op_id,
                                                xferdirect_command, //CMD_GET_COLINFO
                                                target_node,
                                                GetAssignedMailbox(),
                                                opbox::MAILBOX_UNSPECIFIED,
                                                bucket, key,
                                                nullptr);

  state_after_start = State::orig_colinfo_wait_for_ack;


  //All states start in a send
  state = State::orig_meta_send;
}

/**
 * @brief Create target-side handler for a new OpKelpieMeta
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieMeta
 */
OpKelpieMeta::OpKelpieMeta(Op::op_create_as_target_t t)
  : state(State::trgt_meta_start), ldo_msg(), Op(t) {

  //No work to do - done in target's state machine
  peer = 0;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieMeta
 */
OpKelpieMeta::~OpKelpieMeta(){
  //cout <<"OpKelpieMeta() dtor : state is "<<GetStateName()<<endl;
  if(state!=State::done){
    //KTODO("Meta op wasn't in done state when ended");
  }
}



//ORIGIN: Start a new get col info operation by sending a message
WaitingType OpKelpieMeta::smo_Meta_Send(){
  //All origin options start here, but branch out to different states
  //cout <<"OPMETA-ORG-COLINFO: About to send message\n";
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(state_after_start, WaitingType::waiting_on_cq);
}


//TARGET: Get a new meta request, start the initial response
WaitingType OpKelpieMeta::smt_Meta_Start(OpArgs *args) {
  //cout <<"OPPUB-TRG: Got a start\n";
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);
  auto cmd  = imsg->GetCommand();

  //Grab essential from this message for later user
  bucket = imsg->bucket;
  key    = imsg->ExtractKey();


  if(cmd == DirectFlags::CMD_GET_COLINFO) {
    //This is a plain get colinfo. Result goes back in a simple message
    auto omsg = msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);
    omsg->remote_rc = lkv->getColInfo(bucket, key, &omsg->col_info);
    if((omsg->remote_rc != 0) && (imsg->CanStall())){
      //Not here but user can wait
      KTODO("Get Column Info");
      //return;
    } else {
      //Good or bad, we're sending back a result
      net::SendMsg(peer, std::move(ldo_msg));
      return updateStateDone(); //TODO
    }

  } else {
    //Unknown op
    KTODO("Unknown op?");
  }
}

//ORIGIN: Process a ColInfo response
WaitingType OpKelpieMeta::smo_colinfo_WaitAck(OpArgs *args) {

  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();
  kassert(imsg->IsStatus(), "Expecting a Status message");

  if(cb_info_result!=nullptr){
    cb_info_result(imsg->remote_rc, imsg->row_info, imsg->col_info);
  }
  return updateStateDone();
}



WaitingType OpKelpieMeta::Update(opbox::OpArgs *args){
  //cout <<"OPMETA-Update (in state "<< GetStateName()<<" Args: "<<args->str(2,1) <<endl;
  switch(state){

  //First steps are the same for all ops
  case State::orig_meta_send:                 return smo_Meta_Send();
  case State::trgt_meta_start:                return smt_Meta_Start(args);

  //Colinfo op just waits for an ack/nack
  case State::orig_colinfo_wait_for_ack:      return smo_colinfo_WaitAck(args);

  case State::done:                           return updateStateDone();
  default: ;
  }
  KFAIL();
  return WaitingType::error;
}


/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieMeta::GetStateName() const {
  switch(state){
  case State::orig_meta_send:                   return "Origin-Meta-Send";
  case State::trgt_meta_start:                  return "Target-Meta-Start";
  case State::orig_colinfo_wait_for_ack:        return "Origin-ColInfo-WaitForAck";
  case State::done:                             return "Done";
  }
  KFAIL();
}
