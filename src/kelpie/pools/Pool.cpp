// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <string.h> //memcpy

#include "kelpie/core/Singleton.hh"

#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"

#include "kelpie/pools/UnconfiguredPool/UnconfiguredPool.hh"

using namespace std;

namespace kelpie {


/**
 * @brief Ctor for a pool placeholder. This points to an unconfigured pool
 */
Pool::Pool() {
  //Stick an unconfigured pool here until user gets to us.
  //We host an unconfigured pool in the singleton to avoid alloc/free
  impl = kelpie::internal::Singleton::impl.unconfigured_pool;
}

/**
 * @brief Internal function for copying one pool to another
 * @param[in] iuo  Symbol to keep normal users away
 * @param[in] base The internal shared pointer to an existing pool's base
 */
Pool::Pool(faodel::internal_use_only_t iuo, std::shared_ptr<PoolBase> base)
  : impl(base) {
}

/**
 * @brief Shallow copy for pool. It copies the shared pointer over to a new instance
 * @param[in] source The original pool to copy
 */
Pool::Pool(const Pool &source) {
  impl = source.impl;
}

/**
 * @brief Move. Efficiently moves the underlying data structures from one pool to another. Sets source to unconfigured
 * @param[in] source The source pool to transfer to this Pool
 */
Pool::Pool(Pool &&source) {
  impl = std::move(source.impl);
  source.impl = kelpie::internal::Singleton::impl.unconfigured_pool;
}

Pool::~Pool() {
}

/**
 * @brief Shallow copies a pool. Both source and destination will point to the same implementation
 * @param[in] source The source pool to copy
 * @return This Pool
 */
Pool& Pool::operator=(const Pool &source){
  impl = source.impl;
  return *this;
}

/**
 * @brief Move. Efficiently moves a pool from source to destination.
 * @param[in] source The pool to transfer to here
 * @return This pool
 */
Pool& Pool::operator=(Pool &&source){
  impl = std::move(source.impl);
  return *this;
}

/**
 * @brief Pull an item from the local store and asynchronously publish to the pool
 * @param[in] key A global Key for referencing the blob
 * @param[in] callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t Pool::Publish(const Key &key, fn_publish_callback_t callback){
  return impl->Publish(key, callback);
}

/**
 * @brief Asynchronously publish an object to the pool
 * @param[in] key A global Key for referencing the blob
 * @param[in] user_ldo The data object to publish
 * @param[in] callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo, fn_publish_callback_t callback){
  return impl->Publish(key, user_ldo, callback);
}

/**
 * @brief Blocking publish of an object, returning row/column info for destination
 * @param[in] key A global Key for referencing the blob
 * @param[in] user_ldo The data object to publish
 * @param[out] row_info Optional row info from the destination
 * @param[out] col_info Optional column info from the destination
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 * @retval Other Additional error information from the destination may be returned
 */
rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo, kv_row_info_t *row_info, kv_col_info_t *col_info) {
  rc_t rc1, rc2;
  atomic<int> num_left(1);
  rc1 = Publish(key, user_ldo,
               [&num_left, &rc2, row_info, col_info]( kelpie::rc_t result, kelpie::kv_row_info_t &ri, kelpie::kv_col_info_t &ci) {
                  rc2=result;
                  if(row_info) *row_info = ri;
                  if(col_info) *col_info = ci;
                  num_left--;
               });
  if(rc1!=KELPIE_OK) return rc1;
  while(num_left) { sched_yield(); }
  return rc2;
}

/**
 * @brief Asynchronously request an object of unknown size, call a callback when available
 * @param[in] key The Key for the desired blob
 * @param[in] callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed(or detected it's already been placed)
 */
rc_t Pool::Want(const  Key &key, fn_want_callback_t callback){
  return impl->Want(key, 0, callback);
}

/**
 * @brief Asynchronously request an object of a known size, call a callback when available
 * @param[in] key he Key for the desired blob
 * @param[in] expected_ldo_user_bytes The maximum size of the object
 * @param[in] callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed(or detected it's already been placed)
 * @note Data is truncated if object is larger than expected. Check column info in callback
 */
rc_t Pool::Want(const  Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback){
  return impl->Want(key, expected_ldo_user_bytes, callback);
}

/**
 * @brief Blocking request for an object of a known size
 * @param[in] key he Key for the desired blob
 * @param[in] expected_ldo_user_bytes The maximum size of the object
 * @param[out] returned_ldo The DataObject that was returned
 * @retval KELPIE_OK Request placed(or detected it's already been placed)
 * @note Data is truncated if object is larger than expected. Check column info in callback
 */
rc_t Pool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){
  return impl->Need(key, expected_ldo_user_bytes, returned_ldo);
}

/**
 * @brief Blocking request for info about a particular object. Does not wait for object to be generated
 * @param[in] key The Key label for the blob
 * @param[out] col_info Basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @retval KELPIE_ENOENT Item should be on this node, but isn't and hasn't been requested
 */
rc_t Pool::Info(const Key &key, kv_col_info_t *col_info) {
  return impl->Info(key, col_info);
}

/**
 * @brief Get info about a particular row. Currently only looks locally
 * @todo Update to work on remotes nodes
 * @param[in] key The Key label for the blob
 * @param[out] row_info Basic statistics about the row
 * @retval KELPIE_OK if info known
 */
rc_t Pool::RowInfo(const Key &key, kv_row_info_t *row_info){
  return impl->RowInfo(key, row_info);
}

/**
 * @brief Signify that this object is no longer needed and should be released by pool
 * @note This currently only works on local pools
 * @todo Update pools to perform remote drops
 * @param[in] key The Key label for the blob
 * @retval KELPIE_OK if info known
 */
rc_t Pool::Drop(const Key &key) {
  return impl->Drop(key);
}
/**
 * @brief Locate info about the node in the pool that is responsible for hosting this key
 * @param key The key of the item that needs to be found
 * @param node_id The (portable) id for the node holding the data
 * @param peer_ptr Pointer to local data structures for communicating with the node
 * @return KELPIE_OK if can be reached
 */
int Pool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr) {
  return impl->FindTargetNode(key, node_id, peer_ptr);
}

/**
 * @brief Get the bucket that this pool is configured to use
 * @return Bucket value
 */
faodel::bucket_t Pool::GetBucket()            { return impl->GetBucket(); }

/**
 * @brief Get the ResourceURL this pool was configured with
 * @return Resource URL
 */
faodel::ResourceURL Pool::GetURL()            { return impl->GetURL(); }

/**
 * @brief Get the Dirman DirectoryInfo that was used to configure this pool
 * @return dirinfo Directory Info
 */
faodel::DirectoryInfo Pool::GetDirectoryInfo() { return impl->GetDirectoryInfo(); }

/**
 * @brief Get the IOM driver hash associated with this pool (if exists)
 * @retval 0 An IOM driver is not associated with this pool
 * @retval hash The hash of the IOM associated with this pool
 */
iom_hash_t Pool::GetIomHash() { return impl->GetIomHash();}

/**
 * @brief Report back on the number of local references to this pool
 * @return reference count
 */
int Pool::GetRefCount()                       { return impl.use_count(); }

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void Pool::sstr(std::stringstream &ss, int depth, int indent) const { return impl->sstr(ss, depth, indent); }

}  // namespace kelpie

