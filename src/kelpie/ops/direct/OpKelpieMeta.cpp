// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <kelpie/core/Singleton.hh>

#include "faodel-common/Debug.hh"
#include "kelpie/ops/direct/OpKelpieMeta.hh"

using namespace std;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieMeta::op_id = const_hash("OpKelpieMeta");
const string OpKelpieMeta::op_name = "OpKelpieMeta";
bool OpKelpieMeta::debug_enabled = false;

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieMeta::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] config Pointer to configuration so that kelpie.op.meta settings can be retrieved
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void OpKelpieMeta::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieMeta::debug_enabled, nullptr, nullptr, "kelpie.op.meta");
  }
}


/**
 * @brief Create a new colinfo op
 *
 * @param[in] xferdirect_command Type of XferdDirect command this is
 * @param[in] target_node The target node to publish data to
 * @param[in] target_ptr The target node's peer pointer
 * @param[in] bucket The bucket namespace for this object
 * @param[in] key The key label for the object
 * @param[in] iom_hash Hash id of an IOM associated with this request
 * @param[in] cb_result Callback function invoke when success/failure is known
 * @return OpKelpieMeta
 */
OpKelpieMeta::OpKelpieMeta(
                     const uint16_t xferdirect_command,
                     const faodel::nodeid_t target_node,
                     const net::peer_ptr_t target_ptr,
                     const faodel::bucket_t bucket,
                     const Key &key,
                     const iom_hash_t iom_hash,
                     fn_publish_callback_t cb_result)
  :
    Op(true),
    peer(target_ptr),
    cb_info_result(cb_result),
    state(State::orig_meta_send) {

  msg_direct_simple_t::Alloc(ldo_msg, op_id, xferdirect_command, target_node, GetAssignedMailbox(),
                             opbox::MAILBOX_UNSPECIFIED, bucket, key, iom_hash, PoolBehavior::NoAction);

}

/**
 * @brief Create target-side handler for a new OpKelpieMeta
 *
 * @param[in] t Marker designating this ctor is just for creatingf a target op
 * @return OpKelpieMeta
 */
OpKelpieMeta::OpKelpieMeta(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_meta_start), ldo_msg() {

  //No work to do - done in target's state machine
  peer = nullptr;
  GetAssignedMailbox();  //For safety, get a mailbox. Not needed everywhere?
}


/**
 * @brief Destructor for OpKelpieMeta
 */
OpKelpieMeta::~OpKelpieMeta(){
  if(state!=State::done) {
    //KTODO("Meta op wasn't in done state when ended");
  }
}


//ORIGIN: Start a new get col info operation by sending a message
WaitingType OpKelpieMeta::smo_Meta_Send(){
  //All origin options start here, but branch out to different states
  dbg("Sending meta request");
  net::SendMsg(peer, std::move(ldo_msg));
  return updateState(State::orig_wait_for_ack, WaitingType::waiting_on_cq);
}


//TARGET: Get a new meta request, start the initial response
WaitingType OpKelpieMeta::smt_Meta_Start(OpArgs *args) {
  auto imsg = args->ExpectMessageOrDie<msg_direct_simple_t *>(&peer);
  auto cmd  = imsg->GetCommand();

  //Grab essential from this message for later user
  auto bucket     = imsg->bucket;
  auto key        = imsg->ExtractKey();
  auto target_iom = imsg->iom_hash;

  bool is_colinfo = (cmd == DirectFlags::CMD_GET_COLINFO);
  bool is_rowinfo = (cmd == DirectFlags::CMD_GET_ROWINFO);

  dbg("Received meta request for "+key.str());
  if( is_colinfo || is_rowinfo) {

    //Allocate a result message for us to store our row/col info
    auto omsg = msg_direct_status_t::AllocAck(ldo_msg, &imsg->hdr);

    rc_t tmp_rc = lkv->getInfo(bucket, key, &omsg->object_info);

    //For cols, we can dig more info out of the iom if we don't have anything in memory
    if((tmp_rc != KELPIE_OK) && (target_iom != 0) && (is_colinfo)) {
      auto *iom = kelpie::internal::FindIOM(target_iom);
      if(iom == nullptr) {
        throw runtime_error("OpKelpieMeta attempted to read key " + key.str() + " from a node with a bad iom ID");
      }
      tmp_rc = iom->GetInfo(bucket, key, &omsg->object_info);
    }
    omsg->remote_rc = tmp_rc;

    //Send reply
    net::SendMsg(peer, std::move(ldo_msg));
    return updateStateDone();

  } else {
    //Unknown op
    F_TODO("Unknown op?");
  }
}

//ORIGIN: Process a response
WaitingType OpKelpieMeta::smo_WaitAck(OpArgs *args) {

  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();
  F_ASSERT(imsg->IsStatus(), "Expecting a Status message");

  dbg("Received meta info");
  if(cb_info_result!=nullptr) {
    imsg->object_info.ChangeAvailabilityFromLocalToRemote(); //Convert from local to remote
    cb_info_result(imsg->remote_rc, imsg->object_info);
  }
  return updateStateDone();
}


WaitingType OpKelpieMeta::Update(opbox::OpArgs *args) {
  //cout <<"OPMETA-Update (in state "<< GetStateName()<<" Args: "<<args->str(2,1) <<endl;
  switch(state){

  //First steps are the same for all ops
  case State::orig_meta_send:                 return smo_Meta_Send();
  case State::trgt_meta_start:                return smt_Meta_Start(args);

  //Colinfo op just waits for an ack/nack
  case State::orig_wait_for_ack:              return smo_WaitAck(args);

  case State::done:                           return updateStateDone();
  }
  F_FAIL();
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
  case State::orig_wait_for_ack:                return "Origin-WaitForAck";
  case State::done:                             return "Done";
  }
  F_FAIL();
}
