// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "faodel-common/Common.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/core/Singleton.hh"
#include "kelpie/pools/LocalPool/LocalPool.hh"


using namespace std;
using namespace faodel;

namespace kelpie {

LocalPool::LocalPool(const ResourceURL &pool_url)
  : PoolBase(pool_url) {

  //We're a new local pool that has a label the pool registry hasn't seen
  //before. See if we're associated with an iom.

  //Pull out iom info in order to associate the iom with this local pool. While iom option was
  //parsed in base, it didn't save the name (just the hash)
  string iom_option = "";
  if (pool_url.path == "/local/iom") {
    iom_option = pool_url.name;
  } else {
    iom_option = pool_url.GetOption("iom");
  }
  if(!iom_option.empty()) {
    iom = kelpie::internal::Singleton::impl.core->FindIOM(iom_option);
    if(iom==nullptr) {
      throw std::runtime_error("Could not find iom '"+iom_option+"' for local pool with url: "+pool_url.str());
    }
  }

  //Set debug info
  SetSubcomponentName("-Local-"+pool_url.bucket.GetHex());
  
}

LocalPool::~LocalPool(){
  //No extra allocates here. lkv was allocated in core
}

/**
 * @brief Pull an item from the local store (optionally writing to an IOM)
 * @param key A global Key for referencing the blob
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t LocalPool::Publish(const Key &key, fn_publish_callback_t callback){
  kv_col_info_t col_info;
  kv_row_info_t row_info;
  lunasa::DataObject ldo;

  dbg("Publish (from lkv) bucket "+default_bucket.GetHex()+" key "+key.str());

  //Get the ldo
  rc_t rc = lkv->get(default_bucket, key, &ldo, &row_info, &col_info );
  if(rc!=KELPIE_OK) return rc;  //Not found.. publish cannot proceed

  //Write out if we have an iom
  if(iom!=nullptr) {
    iom->WriteObject(default_bucket, key, ldo);
  }
  
  //Found. No instructions on where to publish, so trigger the callback as successfull
  if(callback) callback(KELPIE_OK, row_info, col_info);
  return KELPIE_OK;
}

/**
 * @brief Publish an object to the local pool (optionally writing to an IOM)
 * @param key A global Key for referencing the blob
 * @param user_ldo The data object to publish
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t LocalPool::Publish(const Key &key,
                        const lunasa::DataObject &user_ldo,
                        fn_publish_callback_t callback) {

  kv_col_info_t col_info;
  kv_row_info_t row_info;
  rc_t rc = KELPIE_OK;

  dbg("Publish ldo for bucket "+default_bucket.GetHex()+" key "+key.str());

  //Default to putting in the lkv
  rc = lkv->put(default_bucket, key, user_ldo, behavior_flags, &row_info, &col_info);

  //Write out if we have an iom
  if((iom!=nullptr) && (behavior_flags & PoolBehavior::WriteToIOM)){
    iom->WriteObject(default_bucket, key, user_ldo);
  }
 
  //Launch is always successful. Only send the rc to a callback
  if(callback) callback(rc, row_info, col_info);
  return KELPIE_OK;  //TODO: or should this be rc?
}

/**
 * @brief Request a callback be executed when an item becomes available locally
 * @param key The Key for the desired blob
 * @param expected_ldo_user_bytes Not used in this pool
 * @param callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed (or detected it's already been placed)
 * @note If an IOM is associated with this localpool, it will be queried if the local cache misses
 */
rc_t LocalPool::Want(const Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback) {

  dbg("Want (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  int rc = lkv->want(default_bucket, key, NODE_LOCALHOST, false, callback);

  //See if we can load it from disk
  if((rc==KELPIE_ENOENT) && (iom!=nullptr)) {
    lunasa::DataObject ldo;
    rc = iom->ReadObject(default_bucket, key, &ldo);
    if(rc==KELPIE_OK){
      //We got it. Push it into lkv in order to trigger waiting callbacks
      lkv->put(default_bucket, key, ldo, behavior_flags, nullptr, nullptr);
    }
  }
  
  return rc;
}

/**
 * @brief Blocking request for a blob from the local cache
 * @param key The Key label for the item
 * @param expected_ldo_user_bytes Ignored
 * @param returned_ldo The returned object
 * @retval KELPIE_OK Item was located
 * @note If an IOM is associated with this localpool, it will be queried if the local cache misses
 */
rc_t LocalPool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){

  kassert(returned_ldo!=nullptr, "User must provide an LDO");
  kassert(returned_ldo->internal_use_only.GetRefCount()==0, "Refuse to overwrite an LDO");

  dbg("Need (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  //TODO: Redo and use depends to wakeup
  //Hack: keep polling until someone notifies us
  rc_t rc;
  bool first_time=true;
  bool keep_going=true;
  do {
    rc = lkv->get(default_bucket, key, returned_ldo, nullptr, nullptr);
    if(rc==KELPIE_ENOENT) {

      //First time: See if we can load from disk
      if((first_time) && (iom!=nullptr)) {
        first_time=false;
        rc = iom->ReadObject(default_bucket, key, returned_ldo);
        if(rc==KELPIE_OK) {
          keep_going=false;
          //TODO: Should this be pushed into LKV?
        }
      }

      //Put some delay in the loop if we didn't find it
      if(keep_going){
        //TODO: Update with a wakeup kind of timer?
        std::this_thread::yield();
      }
      
    } else {
      keep_going=false;
    }
  } while(keep_going);

  return rc;
}

/**
 * @brief Get info about a particular key/blob. Does not wait for blob to be generated
 * @param key The Key label for the blob
 * @param col_info Basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @note This currently does NOT query the IOM for info, but will in the future
 */
rc_t LocalPool::Info(const Key &key, kv_col_info_t *col_info) {

  dbg("Info for key "+key.str());


  rc_t rc = lkv->getColInfo(default_bucket, key, col_info);

  //Go out to disk if not here 
  if((rc==KELPIE_ENOENT) && (iom!=nullptr)) {
    rc = iom->GetInfo(default_bucket, key, col_info);
    //TODO: update colinfo with InDisk info?
  }

  return rc;
}

/**
 * @brief Get info about a particular row. Currenly only looks locally
 * @param key The Key label for the blob
 * @param row_info Basic statistics about the row
 * @retval KELPIE_OK if info known
 * @note This currently does NOT query the IOM for info, but will in the future
 */
rc_t LocalPool::RowInfo(const Key &key, kv_row_info_t *row_info) {

  dbg("RowInfo for key "+key.str());

  //TODO: add disk check for row info
  return lkv->getRowInfo(default_bucket, key, row_info);
}

/**
 * @brief Signify that an item is no longer needed
 * @param key The Key label for the blob
 * @retval KELPIE_OK if info known
 * @note This does not affect the IOM
 */
rc_t LocalPool::Drop(const Key &key){
  dbg("Drop key "+key.str());

  //Don't delete from disk
  return lkv->drop(default_bucket, key);
}

/**
 * @brief Perform a search for keys that match a specific pattern
 * @param[in] bucket The bucket id we should limit the search to
 * @param[in] key_prefix The key to search for. Row and/or Key may end in '*' for prefix matching
 * @param[out] object_capacities Info about the objects that match this key search
 * @retval KELPIE_OK Found matches
 * @retval KELPIE_ENOENT Did not find matches
 */
rc_t LocalPool::List(const kelpie::Key &search_key, ObjectCapacities *object_capacities) {
  dbg("List key "+search_key.str());

  return lkv->list(default_bucket, search_key, object_capacities);
}

/**
 * @brief Use the key's row info to determine which node is responsible for the data
 * @param key The Key label for the blob (only ROW portion used)
 * @param node_id Always NODE_LOCALHOST
 * @param peer_ptr Ignored
 * @return KELPIE_OK
 */
int LocalPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr){
  if(node_id)  *node_id=faodel::NODE_LOCALHOST;
  if(peer_ptr) KTODO("peer_ptr must be null here");  //TODO: need to have a null peer_ptr
  return 0;
}


/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void LocalPool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "LocalPool "
     << " Iom: "<< ((iom==nullptr) ? "None" : iom->Name())
     << endl;
  lkv->sstr(ss, depth-1,indent+1);
  //TODO: internals
}

/**
 * @brief Pool constructor function for creating a new LocalPool via a URL
 * @param pool_url The URL for the pool
 * @return New LocalPool
 */
shared_ptr<PoolBase> LocalPoolCreate(const ResourceURL &pool_url) {
  return make_shared<LocalPool>(pool_url);
}

}  // namespace kelpie
