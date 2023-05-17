// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "faodel-common/Common.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/core/Singleton.hh"
#include "kelpie/pools/NullPool/NullPool.hh"


using namespace std;
using namespace faodel;

namespace kelpie {

NullPool::NullPool(const ResourceURL &pool_url)
  : PoolBase(pool_url, PoolBehavior::DefaultLocal) {

  //No real options, just drop everything

  //Set debug info
  SetSubcomponentName("-Null-"+pool_url.bucket.GetHex());
  
}


/**
 * @brief Pull an item from the local store (optionally writing to an IOM)
 * @param key A global Key for referencing the blob
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t NullPool::Publish(const Key &key, const fn_publish_callback_t &callback){
  object_info_t info;
  lunasa::DataObject ldo;

  dbg("Publish (from lkv) bucket "+default_bucket.GetHex()+" key "+key.str());

  info.Wipe();

  //Found. No instructions on where to publish, so trigger the callback as successful
  if(callback) callback(KELPIE_OK, info);
  return KELPIE_OK;
}

/**
 * @brief Publish an object to the local pool (optionally writing to an IOM)
 * @param key A global Key for referencing the blob
 * @param user_ldo The data object to publish
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t NullPool::Publish(const Key &key,
                        const lunasa::DataObject &user_ldo,
                        const fn_publish_callback_t &callback) {

  object_info_t info;
  rc_t rc = KELPIE_OK;

  dbg("Publish ldo for bucket "+default_bucket.GetHex()+" key "+key.str());

  info.Wipe();

  //Launch is always successful. Only send the rc to a callback
  if(callback) callback(rc, info);
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
rc_t NullPool::Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback) {

  dbg("Want (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  lunasa::DataObject ldo;
  if(expected_ldo_user_bytes>0)
    ldo = lunasa::DataObject(0,expected_ldo_user_bytes,lunasa::DataObject::AllocatorType::lazy);

  object_info_t info;
  info.Wipe();
  callback(true, key, ldo, info);

  return KELPIE_OK;

}

/**
 * @brief Blocking request for a blob from the local cache
 * @param key The Key label for the item
 * @param expected_ldo_user_bytes Ignored
 * @param returned_ldo The returned object
 * @retval KELPIE_OK Item was located
 * @note If an IOM is associated with this localpool, it will be queried if the local cache misses
 */
rc_t NullPool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){

  dbg("Key is "+key.str()+" return ldo count is "+to_string(returned_ldo->internal_use_only.GetRefCount())+" expected size "+to_string(expected_ldo_user_bytes));
  F_ASSERT(returned_ldo != nullptr, "User must provide an LDO");
  F_ASSERT(returned_ldo->internal_use_only.GetRefCount() == 0, "User gave an preallocated LDO to Need. Refusing to overwrite it");

  dbg("Need (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  lunasa::DataObject ldo;
  if(expected_ldo_user_bytes>0)
    ldo = lunasa::DataObject(0,expected_ldo_user_bytes,lunasa::DataObject::AllocatorType::lazy);

  *returned_ldo = ldo;

  return KELPIE_OK;
}

/**
 * @brief Perform a computation on a remote object and return a new object (NonBlocking Version)
 * @param[in] key The Key that references an object (foo,bar) or multiple objects in the same row (foo,bar*)
 * @param[in] function_name The text name of the registered function to execute on the remote node
 * @param[in] function_args A string of arguments that are passed to the remote node
 * @param[in] callback A callback to execute when the result is returned (answer is always KELPIE_ENOENT and empty LDO)
 * @retval KELPIE_OK Operation dispatched properly and completed correctly
 */
rc_t NullPool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) {
  dbg("Key is "+key.str()+" function is "+function_name);
  lunasa::DataObject ldo;
  callback(KELPIE_ENOENT,key, ldo);
  return KELPIE_OK;
}

/**
 * @brief Get info about a particular key/blob. Does not wait for blob to be generated
 * @param key The Key label for the blob
 * @param info Basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @note This currently does NOT query the IOM for info, but will in the future
 */
rc_t NullPool::Info(const Key &key, object_info_t *info) {

  dbg("Info for key "+key.str());

  info->Wipe();
  return KELPIE_OK;
}

/**
 * @brief Get info about a particular row. Currenly only looks locally
 * @param key The Key label for the blob
 * @param info Basic statistics about the row
 * @retval KELPIE_OK if info known
 * @note This currently does NOT query the IOM for info, but will in the future
 */
rc_t NullPool::RowInfo(const Key &key, object_info_t *info) {

  dbg("RowInfo for key "+key.str());
  info->Wipe();
  return KELPIE_OK;
}

/**
 * @brief Signify that an item is no longer needed
 * @param[in] key The Key label for the blob
 * @param[in] callback Optional callback for passing back results
 * @retval KELPIE_OK if info known
 * @note This does not affect the IOM
 */
rc_t NullPool::Drop(const Key &key, fn_drop_callback_t callback){
  dbg("Drop key "+key.str());
  return KELPIE_OK;
}

/**
 * @brief Perform a search for keys that match a specific pattern
 * @param[in] search_key The key to search for. Row and/or Key may end in '*' for prefix matching
 * @param[out] object_capacities Info about the objects that match this key search
 * @retval KELPIE_OK Found matches
 * @retval KELPIE_ENOENT Did not find matches
 */
rc_t NullPool::List(const kelpie::Key &search_key, ObjectCapacities *object_capacities) {
  dbg("List key "+search_key.str());

  return KELPIE_OK;
}

/**
 * @brief Use the key's row info to determine which node is responsible for the data
 * @param key The Key label for the blob (only ROW portion used)
 * @param node_id Always NODE_LOCALHOST
 * @param peer_ptr Ignored
 * @return KELPIE_OK
 */
int NullPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr){
  return 0;
}


/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void NullPool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "NullPool "
     << " Iom: "<< ((iom==nullptr) ? "None" : iom->Name())
     << endl;
  lkv->sstr(ss, depth-1,indent+1);
  //TODO: internals
}

/**
 * @brief Pool constructor function for creating a new NullPool via a URL
 * @param pool_url The URL for the pool
 * @return New NullPool
 */
shared_ptr<PoolBase> NullPoolCreate(const ResourceURL &pool_url) {
  return make_shared<NullPool>(pool_url);
}

}  // namespace kelpie
