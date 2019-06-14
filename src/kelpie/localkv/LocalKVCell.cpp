// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include <memory>

#include "faodel-common/Debug.hh"
#include "faodel-services/BackBurner.hh"
#include "opbox/OpBox.hh"

#include "kelpie/localkv/LocalKVCell.hh"
#include "kelpie/localkv/LocalKVRow.hh"
#include "kelpie/common/OpArgsObjectAvailable.hh"

using namespace std;
using namespace faodel;


namespace kelpie {

LocalKVCell::LocalKVCell()
  : availability(Availability::Unavailable), hold_until(0),
    time_offloaded(0) {
  time_posted = getTime();
  time_accessed = time_posted;
}
/** @brief Get a 32b time marker */
uint32_t LocalKVCell::getTime(){
  return (uint32_t) time(NULL);
}


bool LocalKVCell::operator<( const LocalKVCell &x) const {
  return time_posted < x.time_posted;
}


/**
 * @brief Mechanism for saying item is no longer needed in memory (TODO)
 * @param[in] key Item to be removed
 * @param[in] new_availability Where this will be going next
 * @param[out] new_owners_ldo Where this item was transferred to
 * @retval KELPIE_OK Success (does not indicate data was actually here)
 * @retval KELPIE_TODO Would have meant a disk operation
 * @todo implement flush to disk
 */
int LocalKVCell::evict(const Key &key, const Availability new_availability, lunasa::DataObject *new_owners_ldo){
  KTODO("Evict cell");
#if 0
  if(!in_memory) return KELPIE_OK;
  if(!in_disk) {
    //Call offload io
    return KELPIE_TODO;
  }
  mem_sptr.reset();  //Release our memory
  in_memory = false;
  in_disk = true;
#endif
  return KELPIE_OK;
}

bool LocalKVCell::isDroppable(){
  if((waiting_nodes_list.size()>0) || (waiting_ops_list.size())) return false;
  if(availability!=Availability::InLocalMemory) return false;  //todo

  uint32_t t = getTime();
  if(t<hold_until) return false;  //need to keep until this point

  return true;
}

void LocalKVCell::getInfo(kv_col_info_t *col_info){
  if(!col_info) return;

  auto num_receivers = static_cast<uint32_t>(waiting_nodes_list.size());
  if(!num_receivers) num_receivers = static_cast<uint32_t>(waiting_ops_list.size());
  col_info->num_bytes = ldo.GetUserSize();
  col_info->num_col_receiver_nodes = num_receivers;
  col_info->num_col_dependencies = static_cast<uint32_t>(waiting_nodes_list.size()  + waiting_ops_list.size());
  col_info->availability=availability;
}

/**
 * @brief Add a requesting node to the list of nodes that want this data
 * @param[in] requesting_node The (non-local) node that wants the data
 */
void LocalKVCell::appendWaitingList(nodeid_t requesting_node){
  if(requesting_node!=NODE_LOCALHOST)
    waiting_nodes_list.insert(requesting_node);
}

/**
 * @brief Add an op mailbox to the list of nodes that want this data
 * @param[in] op_mailbox The id of an op waiting on this item
 */
void LocalKVCell::appendWaitingList(opbox::mailbox_t op_mailbox){
  waiting_ops_list.insert(op_mailbox);
}


/**
 * @brief Item wasn't available, leave a callback to execute when available
 * @param[in] callback The (local) want callback to invok
 */
void LocalKVCell::appendCallbackList(fn_want_callback_t callback){
  callback_list.push_back(callback);
}
/**
 * @brief When item becomes available, execute any callbacks that were waiting on it
 */
void LocalKVCell::dispatchCallbacksAndNotifications(LocalKVRow *row, const Key &key){

  //Bail out if no dependencies
  if((callback_list.size()==0)      &&
     (waiting_nodes_list.size()==0) &&
     (waiting_ops_list.size()==0)      ) return;

  //cout <<"LKV Cell: dispatch callbacks for "<<key.str() <<" has "
  //     <<callback_list.size()<<" callbacks, "
  //     <<waiting_nodes_list.size()<<" nodes, and "
  //     <<waiting_ops_list.size()<<" ops\n";

  //Build a list of actions that backburner should perform

  vector<faodel::fn_backburner_work> backburner_work;
  kv_row_info_t row_info;
  kv_col_info_t col_info;
  row->getInfo(&row_info);
  getInfo(&col_info);

  //Callbacks on the local node
  //cout <<"LKV LocalCBs. NumCBs: "<<callback_list.size()<<" LDO Refs: "<<ldo.internal_use_only.GetRefCount()<<endl;
  for(auto &cb : callback_list){
    //cb(true, key, ldo);
    lunasa::DataObject ldotmp = ldo; //LDOs don't like member vars
    //cout <<"   installing cb LDO Refs: "<<ldo.internal_use_only.GetRefCount()<<endl;

    backburner_work.push_back( [cb, key, ldotmp, row_info, col_info] {
        cb(true, key, ldotmp, row_info, col_info);
        return 0;
      });

  }

  //Waiting Ops
  if(waiting_ops_list.size()>0){

    //We need to do some trigger ops here. OpBox spools these up internally
    //(potentially launching on backburner), so just let them do the work
    auto args = make_shared<OpArgsObjectAvailable>(ldo, row_info, col_info);
    for(auto &mb : waiting_ops_list) {
      opbox::TriggerOp(mb, std::move(args)); 
    }
    //TODO: Add a function to spool all the mailboxes/opargs up and then
    //      hand over so opbox can deal with bundles

  }

  //Waiting nodes
  for(auto &nd : waiting_nodes_list){
    //TODO: schedule a send
    KWARN("Need to update kelpie for waiting lists")
  }

  //cout<<"LKV total backburner jobs adding: "<<backburner_work.size()<<endl;

  //Launch all work in one shot
  faodel::backburner::AddWork(backburner_work);

  //Clear out dependency lists
  callback_list.clear();
  waiting_nodes_list.clear();
  waiting_ops_list.clear();

}

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void LocalKVCell::sstr(stringstream &ss, int depth, int indent) const {
  int t = (int) time(NULL);
  ss << string(indent, ' ')
     << " Bytes: "       << getUserSize()
     << " Age: "         << time_posted - t
     << " SinceAccess: " << time_accessed - t
     << endl;


}

}  //namespace kelpie
