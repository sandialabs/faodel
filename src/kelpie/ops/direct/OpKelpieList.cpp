// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <opbox/common/MessageHelpers.hh>
#include "OpKelpieList.hh"

using namespace std;
using namespace faodel;
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
 * @param[in] new_lkv A pointer to the kelpie localkv we should uses_allocator
 */
void kelpie::OpKelpieList::configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, kelpie::LocalKV *new_lkv) {
  lkv = new_lkv;
  if(config) {
    config->GetComponentLoggingSettings(&OpKelpieList::debug_enabled, nullptr, nullptr, "OpKelpieList");
  }
}




kelpie::OpKelpieList::OpKelpieList(
        const vector<pair<faodel::nodeid_t, net::peer_ptr_t>> targets,
        const faodel::bucket_t bucket, const kelpie::Key &search_key,
        ObjectCapacities *object_capacities,
        condition_variable *cv, int *num_targets_left)

        : Op(true), targets(targets), bucket(bucket), search_key(search_key),
          user_object_capacities(object_capacities),
          user_cv(cv), user_num_targets_left(num_targets_left),
          state(State::orig_list_send) {


  //Defer message creation until send state
}

OpKelpieList::OpKelpieList(Op::op_create_as_target_t t)
  : Op(t), state(State::trgt_list_start) {
  GetAssignedMailbox(); //For safety, get a mailbox. Not needed everywhere?
}


kelpie::OpKelpieList::~OpKelpieList() {
  if(state!=State::done){
    KTODO("Dtor when state not done");
  }
}

WaitingType OpKelpieList::smo_List_Send() {

  //Remember how many nodes we have left
  *user_num_targets_left = targets.size();
  if(*user_num_targets_left<1) {
    dbg("Bail: op didn't have any targets");
    return updateStateDone();
  }

  auto mbox = GetAssignedMailbox();

  for(auto &node_peerptr : targets) {
    dbg("Sending to target "+node_peerptr.first.GetHex());

    lunasa::DataObject ldo;
    msg_direct_buffer_t::Alloc(
            ldo,
            op_id, DirectFlags::CMD_LIST, node_peerptr.first,
            mbox, opbox::MAILBOX_UNSPECIFIED,
            bucket, search_key,
            0, PoolBehavior::NoAction,    //TODO: Add iom and behavior flags
            nullptr);
    net::SendMsg(node_peerptr.second, std::move(ldo));
  }
  return updateState(State::orig_list_wait_for_results, WaitingType::waiting_on_cq);

}

WaitingType OpKelpieList::smt_List_Start(opbox::OpArgs *args) {


  net::peer_ptr_t peer;
  auto imsg = args->ExpectMessageOrDie<msg_direct_buffer_t *>(&peer);
  search_key = imsg->ExtractKey();

  dbg("Target1 received a request for "+search_key.str());

  ObjectCapacities found_object_capacities;

  rc_t rc = lkv->list(imsg->bucket, search_key, &found_object_capacities);
  uint16_t simple_rc = (rc==KELPIE_OK) ? 0 : 1;

  //Create a reply message. For simplicity just boost pack it in the payload
  lunasa::DataObject ldo_out;
  AllocateBoostReplyMessage<ObjectCapacities>(ldo_out, &imsg->hdr, simple_rc, found_object_capacities);

  net::SendMsg(peer, std::move(ldo_out));

  return updateStateDone();
}

WaitingType OpKelpieList::smo_List_WaitForResults(opbox::OpArgs *args) {

  net::peer_ptr_t peer;
  auto imsg = args->ExpectMessageOrDie<message_t *>(&peer);

  //Annoyingly, we extract to a temp struct and then append the users
  auto found_object_capacities = UnpackBoostMessage<ObjectCapacities>(imsg);
  user_object_capacities->Append(found_object_capacities);

  (*user_num_targets_left)--;
  dbg("Origing received response. num_left="+std::to_string(*user_num_targets_left));

  if(*user_num_targets_left<1) {
    dbg("Received last item. Notifying user of result");
    user_cv->notify_one();
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
  KFAIL();
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
  KFAIL();
}





