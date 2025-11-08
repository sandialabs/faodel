// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <opbox/common/MessageHelpers.hh>
#include <kelpie/core/Singleton.hh>
#include "OpKelpieList.hh"

using namespace std;
using namespace kelpie;

//Statics: Standard id/name info for an op
const unsigned int OpKelpieList::op_id = const_hash("OpKelpieList");
const string OpKelpieList::op_name = "OpKelpieList";
bool OpKelpieList::debug_enabled = false;

//Statics: This op has a static localkv pointer. lkv lives inside KelpieCore instance
LocalKV * OpKelpieList::lkv = nullptr;

/**
 * @brief Internal startup command for setting static variables
 *
 * @param[in] iuo Designates this function is for internal use only
 * @param[in] config Pointer to configuration so that kelpie.op.list settings can be retrieved
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void kelpie::OpKelpieList::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, kelpie::LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieList::debug_enabled, nullptr, nullptr, "kelpie.op.list");
  }
}

/**
 * @brief Create a new List operation
 *
 * @param[in] targets A list of nodes where this should be sent
 * @param[in] bucket The bucket namespace for this object
 * @param[in] search_key The key label for the object (may be a wildcard)
 * @param[in] iom_hash The hash code of an iom to check if this pool has storage
 * @param[out] object_capacities Place to return information about the found objects
 * @param[out] caller_promise Notification of completion
 */
OpKelpieList::OpKelpieList(
        const vector<pair<faodel::nodeid_t, net::peer_ptr_t>> targets,
        const faodel::bucket_t bucket, const kelpie::Key &search_key,
        const kelpie::iom_hash_t &iom_hash,
        ObjectCapacities *object_capacities,
        std::promise<bool> *caller_promise)

        : Op(true),
          targets(targets), bucket(bucket), search_key(search_key), iom_hash(iom_hash),
          user_object_capacities(object_capacities),
          caller_promise(caller_promise),
          state(State::orig_list_send) {

  num_targets_left = targets.size();

  //Defer message creation until send state
}

OpKelpieList::OpKelpieList(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_list_start) {
  GetAssignedMailbox(); //For safety, get a mailbox. Not needed everywhere?
}


kelpie::OpKelpieList::~OpKelpieList() {
  if(state!=State::done){
    F_TODO("Dtor when state not done");
  }
}

WaitingType OpKelpieList::smo_List_Send() {

  //Check node count to make sure we have work
  if(num_targets_left<1) {
    dbg("Bail: op didn't have any targets");
    return updateStateDone();
  }

  auto mbox = GetAssignedMailbox();

  for(auto &node_peerptr : targets) {
    dbg("Sending to target "+node_peerptr.first.GetHex());

    lunasa::DataObject ldo;
    msg_direct_simple_t::Alloc(
            ldo,
            op_id, DirectFlags::CMD_LIST, node_peerptr.first,
            mbox, opbox::MAILBOX_UNSPECIFIED,
            bucket, search_key,
            iom_hash, PoolBehavior::NoAction);
    net::SendMsg(node_peerptr.second, std::move(ldo));
  }
  return updateState(State::orig_list_wait_for_results, WaitingType::waiting_on_cq);

}

WaitingType OpKelpieList::smt_List_Start(opbox::OpArgs *args) {


  net::peer_ptr_t peer;
  auto imsg = args->ExpectMessageOrDie<msg_direct_simple_t *>(&peer);
  search_key = imsg->ExtractKey();

  ObjectCapacities found_object_capacities;

  dbg("Target received a list request for "+search_key.str());

  // If there's an IOM, attempt to find matching objects there.
  auto *iom = kelpie::internal::FindIOM(imsg->iom_hash);

  rc_t rc = lkv->list(imsg->bucket, search_key, iom, &found_object_capacities);
  dbg("Target list found objects final: "+to_string(found_object_capacities.capacities.size()));

  uint16_t simple_rc = (rc==KELPIE_OK) ? 0 : 1;

  //Create a reply message. For simplicity just cereal pack it in the payload
  lunasa::DataObject ldo_out;
  AllocateCerealReplyMessage<ObjectCapacities>(ldo_out, &imsg->hdr, simple_rc, found_object_capacities);

  net::SendMsg(peer, std::move(ldo_out));

  return updateStateDone();
}

WaitingType OpKelpieList::smo_List_WaitForResults(opbox::OpArgs *args) {

  net::peer_ptr_t peer;
  auto imsg = args->ExpectMessageOrDie<message_t *>(&peer);

  //Annoyingly, we extract to a temp struct and then append the users
  auto found_object_capacities = UnpackCerealMessage<ObjectCapacities>(imsg);
  user_object_capacities->Append(found_object_capacities);

  num_targets_left--;
  dbg("Origin received response. num_left="+std::to_string(num_targets_left));

  if(num_targets_left<1) {
    dbg("Received last item. Notifying user of result");
    caller_promise->set_value(true);
    return updateStateDone();
  }

  //More results expected. Stay here
  return updateState(State::orig_list_wait_for_results, WaitingType::waiting_on_cq);
}

WaitingType OpKelpieList::Update(OpArgs *args) {
  dbg("Got an update. Processing state "+GetStateName());
  switch(state) {
    case State::orig_list_send:             return smo_List_Send();
    case State::trgt_list_start:            return smt_List_Start(args);
    case State::orig_list_wait_for_results: return smo_List_WaitForResults(args);
    case State::done:                       return updateStateDone();
  }
  F_FAIL();
  return WaitingType::error;
}

/**
 * @brief Get a string name for the current state
 * @retval string Human-readable name for state
 */
std::string OpKelpieList::GetStateName() const {

  switch(state) {
    case State::orig_list_send:             return "Origin-List-Send";
    case State::trgt_list_start:            return "Target-List-Start";
    case State::orig_list_wait_for_results: return "Origin-List-WaitForResults";
    case State::done:                       return "Done";
  }
  F_FAIL();
}





