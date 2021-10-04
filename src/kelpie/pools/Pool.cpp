// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstring> //memcpy

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
rc_t Pool::Publish(const Key &key, const fn_publish_callback_t &callback) {
  if(key.IsWildcard()) {
    throw std::runtime_error("Publish using a wildcard is not supported. Key: "+key.str());
  }
  return impl->Publish(key, callback);
}

/**
 * @brief Pull an item from the local store and asynchronously publish to the pool
 * @param[in] key A global Key for referencing the blob
 * @param[in] collector A result collector to capture info about the operation
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */

rc_t Pool::Publish(const Key &key, ResultCollector &collector) {
  return Publish(key,
                 [&collector] (kelpie::rc_t result, object_info_t new_info) {
                     collector.fn_publish_callback(result, new_info);
                 });
}


/**
 * @brief Blocking publish of an object to the pool
 * @param[in] key A global Key for referencing the blob
 * @param[in] user_ldo The data object to publish
 * @retval KELPIE_OK The request was successfully executed
 */
rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo) {
  object_info_t info;
  return Publish(key, user_ldo, &info);
}

/**
 * @brief Blocking publish of an object, returning row/column info for destination
 * @param[in] key A global Key for referencing the blob
 * @param[in] user_ldo The data object to publish
 * @param[out] info Return optional info from the destination
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 * @retval Other Additional error information from the destination may be returned
 */
rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo, object_info_t *info) {
  rc_t rc1, rc2;
  atomic<int> num_left(1);
  rc1 = Publish(key, user_ldo,
               [&num_left, &rc2, info]( kelpie::rc_t result, object_info_t new_info) {
                  rc2=result;
                  if(info) *info = new_info;
                  num_left--;
               });
  if(rc1!=KELPIE_OK) return rc1;
  while(num_left) { std::this_thread::yield(); }
  return rc2;
}

/**
 * @brief Asynchronously publish an object to the pool
 * @param[in] key A global Key for referencing the blob
 * @param[in] user_ldo The data object to publish
 * @param[in] callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo, const fn_publish_callback_t &callback) {
  if(key.IsWildcard()) {
    throw std::runtime_error("Publish using a wildcard is not supported. Key: "+key.str());
  }
  return impl->Publish(key, user_ldo, callback);
}

rc_t Pool::Publish(const Key &key, const lunasa::DataObject &user_ldo, ResultCollector &collector) {
  return Publish(key, user_ldo,
                 [&collector] (kelpie::rc_t result, object_info_t new_info) {
                    collector.fn_publish_callback(result, new_info);
                 });
}

/**
 * @brief Asynchronously request an object of unknown size, call a callback when available
 * @param[in] key The Key for the desired blob
 * @param[in] callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed(or detected it's already been placed)
 */
rc_t Pool::Want(const  Key &key, const fn_want_callback_t &callback) {
  if(key.IsWildcard()) {
    throw std::runtime_error("Want using a wildcard is not supported. Key: "+key.str());
  }
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
rc_t Pool::Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback) {
  if(key.IsWildcard()) {
    throw std::runtime_error("Want using a wildcard is not supported. Key: "+key.str());
  }
  return impl->Want(key, expected_ldo_user_bytes, callback);
}

rc_t Pool::Want(const Key &key, ResultCollector &collector) {
  return Want(key, 0, collector);
}
rc_t Pool::Want(const Key &key, size_t expected_ldo_user_bytes, ResultCollector &collector) {
  if(key.IsWildcard()) {
    throw std::runtime_error("Want using a wildcard is not supported. Key: "+key.str());
  }
  return Want(key, expected_ldo_user_bytes,
              [&collector] (bool success, Key key, lunasa::DataObject user_ldo, const object_info_t &info) {
                 collector.fn_want_callback(success, key, user_ldo, info);
              });
}

/**
 * @brief Blocking request for an object of a known size
 * @param[in] key he Key for the desired blob
 * @param[in] expected_ldo_user_bytes The maximum size of the object
 * @param[out] returned_ldo The DataObject that was returned
 * @retval KELPIE_OK Request placed(or detected it's already been placed)
 * @note Data is truncated if object is larger than expected. Check column info in callback
 */
rc_t Pool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo) {
  F_ASSERT(returned_ldo != nullptr, "Need didn't have a valid returned_ldo pointer");
  F_ASSERT(returned_ldo->GetUserCapacity() == 0, "Need request attempted to use a preallocated ldo. Use deepcopy instead. Key:"+key.str());
  return impl->Need(key, expected_ldo_user_bytes, returned_ldo);
}

/**
 * @brief Perform a computation on a remote object and return a new object (NonBlocking Version)
 * @param[in] key The Key that references an object (foo,bar) or multiple objects in the same row (foo,bar*)
 * @param[in] function_name The text name of the registered function to execute on the remote node
 * @param[in] function_args A string of arguments that are passed to the remote node
 * @param[in] callback A callback to execute when the result is returned
 * @retval KELPIE_OK Operation dispatched properly and completed correctly
 * @retval KELPIE_EINVAL The function name was not known at the object
 */
rc_t Pool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) {
  return impl->Compute(key, function_name, function_args, callback);
}

/**
 * @brief Perform a compute operation on a remote pool and return and object
 * @param[in] key The Key that references an object (foo,bar) or multiple objects in the same row (foo,bar*)
 * @param[in] function_name The text name of the registered function to execute on the remote node
 * @param[in] function_args A string of arguments that are passed to the remote node
 * @param[in] collector A result collector to capture info about the operation
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 * @retval KELPIE_EINVAL The function name was not known at the object
 */

rc_t Pool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, ResultCollector &collector) {
  return Compute(key, function_name, function_args,
                 [&collector] (kelpie::rc_t result, kelpie::Key key, lunasa::DataObject user_ldo) {
                     collector.fn_compute_callback(result, key, user_ldo);
                 });
}



/**
 * @brief Perform a computation on a remote object and return a new object (Blocking Version)
 * @param[in] key The Key that references an object (foo,bar) or multiple objects in the same row (foo,bar*)
 * @param[in] function_name The text name of the registered function to execute on the remote node
 * @param[in] function_args A string of arguments that are passed to the remote node
 * @param[out] returned_ldo  The resulting LDO from the operation (this is NOT cached in the system)
 * @retval KELPIE_OK Operation dispatched properly and completed correctly
 * @retval KELPIE_EINVAL The function name was not known at the object
 */
rc_t Pool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, lunasa::DataObject *returned_ldo) {
  rc_t rc1, rc2;
  atomic<int> num_left(1);
  rc1 = Compute(key, function_name, function_args,
                [&num_left, &rc2, &returned_ldo]( kelpie::rc_t result, Key key, lunasa::DataObject user_ldo) {
                    rc2 = result;
                    if(returned_ldo) *returned_ldo = user_ldo;
                    num_left--;
                });
  if(rc1!=KELPIE_OK) return rc1;
  while(num_left) { std::this_thread::yield(); }
  return rc2;
}

/**
 * @brief Blocking request for info about a particular object. Does not wait for object to be generated
 * @param[in] key The Key label for the blob (must be exact key: wildcards are not supported)
 * @param[out] info Return basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @retval KELPIE_ENOENT Item should be on this node, but isn't and hasn't been requested
 */
rc_t Pool::Info(const Key &key, object_info_t *info) {
  if(key.IsRowWildcard() || key.IsColWildcard()) {
    F_WARN("Kelpie Info called with wildcard key. Wildcards are not currently supported.");
  }
  return impl->Info(key, info);
}

/**
 * @brief Get info about a particular row.
 * @todo Update to work on remotes nodes
 * @param[in] key The Key label for the blob (Wildcards only supported for Columns, not rows)
 * @param[out] info Basic statistics about the row
 * @retval KELPIE_OK if info known
 */
rc_t Pool::RowInfo(const Key &key, object_info_t *info) {
  if(key.IsRowWildcard()) {
    F_WARN("Kelpie RowInfo called with a row wildcard key. Wildcards are only supported for columns.");
  }
  return impl->RowInfo(key, info);
}

/**
 * @brief Signify that this object is no longer needed and should be released by pool
 * @param[in] key The Key label for the blob
 * @param[in] callback An optional callback to execute when the operation completes
 * @retval KELPIE_OK if info known
 */
rc_t Pool::Drop(const Key &key, fn_drop_callback_t callback) {
  return impl->Drop(key, callback);
}

/**
 * @brief Do a blocking drop of a key (which can have wildcards)
 * @param[in] key The item to search for (can contain row/column wildcards)
 * @retval KELPIE_OK Removed one or more items
 * @retval KELPIE_ENOENT Did not locate any items to drop
 */
rc_t Pool::BlockingDrop(const Key &key) {

  std::promise<bool> success_promise;
  std::future<bool> success_future = success_promise.get_future();

  Drop(key, [&success_promise](bool inner_success, const kelpie::Key &inner_key) {
      success_promise.set_value(inner_success);
  });

  bool success = success_future.get();

  return (success) ? KELPIE_OK : KELPIE_ENOENT;
}
/**
 * @brief Perform a search for keys in this pool that match a specific pattern
 * @param[in] search_key The key to search for. Row and/or Key may end in '*' for prefix matching
 * @param[out] object_capacities Info about the objects that match this key search
 * @retval KELPIE_OK Found matches
 * @retval KELPIE_ENOENT Did not find matches
 */
rc_t Pool::List(const Key &search_key, ObjectCapacities *object_capacities) {
  return impl->List(search_key, object_capacities);
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
 * @brief Determine whether this pool initialized successfully
 * @param error_message Optional information about why this pool failed to initialize
 * @retval TRUE This pool is functional
 * @retval FALSE This pool did not initialize ok
 */
bool Pool::Valid(std::string *error_message) {
  if((impl==nullptr) || (impl->TypeName()=="unconfigured")) {
    if(error_message) {
      *error_message = dynamic_cast<UnconfiguredPool *>(impl.get())->error_message;
    }
    return false;
  }
  if(error_message) *error_message = "";
  return true;
}
void Pool::ValidOrDie() {
  string err;
  if(!Valid(&err)) {
    cout<<"Pool.ValidOrDie() shutdown. Reason: "<<err<<endl;
    exit(0);
  }
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
 * @brief Get information about how read/write actions for this pool take place
 * @return Behavior A flag specifying how write/reads for local/remote/ioms take place
 */
kelpie::pool_behavior_t Pool::GetBehavior() { return impl->GetBehavior(); }

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

