// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "OpKelpieDrop.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieDrop::op_id = const_hash("OpKelpieDrop");
const string OpKelpieDrop::op_name = "OpKelpieDrop";
bool OpKelpieDrop::debug_enabled = false;

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieDrop::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] config Pointer to configuration so that kelpie.op.drop settings can be retrieved
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void kelpie::OpKelpieDrop::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, kelpie::LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieDrop::debug_enabled, nullptr, nullptr, "kelpie.op.drop");
  }
}

/**
 * @brief Create a new Drop Object operation
 *
 * @param[in] targets A list of nodes where this should be sent
 * @param[in] bucket he bucket namespace for this object
 * @param[in] search_key The key label for the object (may be a wildcard)
 * @param[in] already_dropped_locally Whether we have already dropped item from this node
 * @param[in] callback Function to call when we complete
 */
OpKelpieDrop::OpKelpieDrop(
        vector <pair<faodel::nodeid_t, net::peer_ptr_t>> targets,
        const faodel::bucket_t bucket,
        const Key &search_key,
        bool already_dropped_locally,
        fn_drop_callback_t callback)

  : Op(false),
    targets(std::move(targets)),
    bucket(bucket), search_key(search_key),
    callback(callback),
    state(State::orig_drop_send) {

  dbg("Creating new drop");
  successful_drops = (already_dropped_locally) ? 1 : 0;
}

OpKelpieDrop::OpKelpieDrop(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_drop_start) {
}

OpKelpieDrop::~OpKelpieDrop() {
  if(state!=State::done){
    F_TODO("Dtor when state not done");
  }
}

//Origin: Send out requests to all targets. Set counters on expected replies
WaitingType OpKelpieDrop::smo_Drop_Send() {

  dbg("Starting to send. NumTargets="+to_string(targets.size()));

  bool expects_reply = (callback!=nullptr);
  mailbox_t mbox = (expects_reply) ? GetAssignedMailbox() : opbox::MAILBOX_UNSPECIFIED;
  num_targets_left = (expects_reply) ? targets.size() : 0;

  //Note: assumes caller has correctly filtered this node from list
  for(auto &node_peerptr : targets) {
    dbg("Sending to target "+node_peerptr.first.GetHex());

    lunasa::DataObject ldo;
    msg_direct_simple_t::Alloc(
            ldo,
            op_id, DirectFlags::CMD_DROP, node_peerptr.first,
            mbox, opbox::MAILBOX_UNSPECIFIED,
            bucket, search_key,
            0, PoolBehavior::NoAction); //TODO: Update iom/behavior settings
    net::SendMsg(node_peerptr.second, std::move(ldo));
  }

  if(expects_reply) {
    state = State::orig_drop_wait_for_results;
    return WaitingType::waiting_on_cq;
  } else {
    state = State::done;
    return WaitingType::done_and_destroy;
  }
}

//Target: Got a request. Issue a drop and send back a reply if user expects one
WaitingType OpKelpieDrop::smt_Drop_Start(opbox::OpArgs *args) {

  dbg("Starting a new drop");
  net::peer_ptr_t peer;
  auto imsg = args->ExpectMessageOrDie<msg_direct_simple_t *>(&peer);
  search_key = imsg->ExtractKey();

  rc_t rc = lkv->drop(imsg->bucket, search_key);

  if(imsg->hdr.src_mailbox != MAILBOX_UNSPECIFIED) {

    //Build a reply message and send it
    lunasa::DataObject ldo_reply;
    auto omsg = msg_direct_status_t::AllocAck(ldo_reply, &imsg->hdr);  //Success changed below
    if(rc!=KELPIE_OK)
      omsg->Success(false);
    dbg("Sending a reply message. Dropped items: "+to_string(rc==KELPIE_OK));
    net::SendMsg(peer, std::move(ldo_reply));

    state=State::done;
    return WaitingType::done_and_destroy;

  } else {

    //No reply needed
    state=State::done;
    return WaitingType::done_and_destroy;
  }
}

//Origin: Count replies until we've got them all
WaitingType OpKelpieDrop::smo_Drop_WaitForResults(opbox::OpArgs *args) {

  auto imsg = args->ExpectMessageOrDie<msg_direct_status_t *>();
  if(imsg->Success())
    successful_drops++;

  num_targets_left--;
  dbg("Got a reply message. Num Targets now left "+to_string(num_targets_left));
  if(num_targets_left<1) {
    callback((successful_drops>0), search_key);
    state=State::done;
    return WaitingType::done_and_destroy;
  }
  //Wait for more
  return WaitingType::waiting_on_cq;
}


WaitingType OpKelpieDrop::Update(OpArgs *args) {
  //dbg("Update. Processing state "+GetStateName());
  switch(state) {
    case State::orig_drop_send:             return smo_Drop_Send();
    case State::trgt_drop_start:            return smt_Drop_Start(args);
    case State::orig_drop_wait_for_results: return smo_Drop_WaitForResults(args);
    case State::done:                       return WaitingType::done_and_destroy;
  }
  F_FAIL();
  return WaitingType::error;
}

/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieDrop::GetStateName() const {

  switch(state) {
    case State::orig_drop_send:             return "Origin-Drop-Send";
    case State::trgt_drop_start:            return "Target-Drop-Start";
    case State::orig_drop_wait_for_results: return "Origin-Drop-Wait-for-Results";
    case State::done:                       return "Done";
  }
  F_FAIL();
}
