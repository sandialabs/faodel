// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <future>
#include <kelpie/ops/direct/OpKelpieList.hh>
#include <kelpie/ops/direct/OpKelpieDrop.hh>

#include "faodel-common/Common.hh"
#include "faodel-common/Debug.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/pools/DHTPool/DHTPool.hh"
#include "kelpie/ops/direct/OpKelpieCompute.hh"
#include "kelpie/ops/direct/OpKelpieGetBounded.hh"
#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"
#include "kelpie/ops/direct/OpKelpieMeta.hh"
#include "kelpie/ops/direct/OpKelpiePublish.hh"

#include "kelpie/core/Singleton.hh"

using namespace std;
using namespace faodel;

namespace kelpie {


DHTPool::DHTPool(const ResourceURL &pool_url)
  : PoolBase(pool_url, PoolBehavior::DefaultRemote) {


  bool ok = dirman::GetDirectoryInfo(pool_url, &dir_info);
  if(!ok){
    throw std::runtime_error("Pool "+TypeName()+" could not get directory info for "+pool_url.str());
  }


  bool we_are_in_pool=false;

  //Allocate peers and connect to each one
  for(size_t i=0; i<dir_info.members.size(); i++) {

    if(dir_info.members[i].node == my_nodeid)
      we_are_in_pool=true;

    net::peer_ptr_t peer;
    int rc = net::Connect(&peer, dir_info.members[i].node);
    if(rc!=0) {
      throw std::runtime_error("Pool "+TypeName()+" could not connect to peer "+dir_info.members[i].name);
    }
    nodes.emplace_back(pair<nodeid_t, net::peer_ptr_t>(dir_info.members[i].node, peer));
  }

  //Change default behavior when we have an iom attached
  if((iom_hash) && (pool_url.GetOption("behavior").empty())) {
    behavior_flags=PoolBehavior::DefaultRemoteIOM;
  }

  if(we_are_in_pool) {
    //Pull out iom info in order to associate the iom with this local pool. While iom option was
    //parsed in base, it didn't save the name (just the hash)
    string iom_option;
    if(pool_url.path == "/local/iom") {
      iom_option = pool_url.name;
    } else {
      iom_option = pool_url.GetOption("iom");
    }
    if(!iom_option.empty()) {
      iom = kelpie::internal::Singleton::impl.core->iom_registry.Find(iom_option);
      if(iom == nullptr) {
        //IOM isn't known, try registering it from the url
        int rc = kelpie::internal::Singleton::impl.core->iom_registry.RegisterIomFromURL(pool_url);
        if(rc==0) {
          //Registered ok, retry the lookup
          iom = kelpie::internal::Singleton::impl.core->iom_registry.Find(iom_option);
        }
        if(iom == nullptr) {
          throw std::runtime_error(
                "Could not find iom '" + iom_option + "' for dht pool with url: " + pool_url.GetFullURL());
        }

      }
    }
  }
}

/**
 * @brief Pull an item from the local store and asynchronously publish to the pool
 * @param key A global Key for referencing the blob
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t DHTPool::Publish(const Key &key, const fn_publish_callback_t &callback){

  dbg("Publish (from lkv) bucket "+default_bucket.GetHex()+" key "+key.str());

  //Retrieve the item from lkv so we can send to the destination
  lunasa::DataObject ldo;
  rc_t rc = lkv->get(default_bucket, key, &ldo, nullptr);
  if(rc==KELPIE_ENOENT) return KELPIE_ENOENT;

  return Publish(key, ldo, callback);

}

/**
 * @brief Asynchronously publish an object to the pool
 * @param key A global Key for referencing the blob
 * @param user_ldo The data object to publish
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t DHTPool::Publish(const Key &key,
                      const lunasa::DataObject &user_ldo,
                      const fn_publish_callback_t &callback) {

  //Figure out which node in our list gets the spot
  uint32_t spot = findNodeIndex(key);

  dbg("Publish ldo to dht node "+std::to_string(spot)+" for bucket "+default_bucket.GetHex()+" key "+key.str());


  //Skip ops if we're actually the target node in the dht
  if(nodes[spot].first == my_nodeid) {
    object_info_t info;
    rc_t rc = lkv->put(default_bucket, key, user_ldo, PoolBehavior::ChangeRemoteToLocal(behavior_flags), iom, &info);

    if(callback) callback(rc, info);
    return KELPIE_OK;    
  }

  //Send it off to the right destination for processing
  auto *op = new OpKelpiePublish(
                                nodes[spot].first, nodes[spot].second,
                                default_bucket,
                                key,
                                user_ldo,
                                iom_hash,
                                behavior_flags,
                                callback
                                );
  opbox::LaunchOp(op);


  //See if we also need to write it here
  if(behavior_flags & PoolBehavior::WriteToLocal) {
    object_info_t info;
    rc_t rc = lkv->put(default_bucket, key, user_ldo, behavior_flags, nullptr, &info); //ignore iom - this is for caching
  }


  return KELPIE_OK;
}

/**
 * @brief Request an item be brought to this node when published to the pool
 * @param key The Key for the desired blob
 * @param expected_ldo_user_bytes The expected size of the item (if known), or zero (if unknown)
 * @param callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed (or detected it's already been placed)
 */
rc_t DHTPool::Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback){

  dbg("Want (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  //Check the lkv to see if it already exists. If it does, have lkv do
  //the callback. If it doesn't, leave the callback in place so it can
  //trigger when the data arrives.
  rc_t rc = lkv->wantLocal(default_bucket, key, true, callback);
  if((rc==KELPIE_OK) || (rc==KELPIE_WAITING)) return KELPIE_OK;  //Already requested..

  //Figure out which node in our list gets the spot
  uint32_t spot = findNodeIndex(key);

  //See if this item belongs here, we don't send anything
  if(nodes[spot].first == my_nodeid) {

    //See if we can load it from disk
    if((rc==KELPIE_ENOENT) && (iom!=nullptr) && (behavior_flags & PoolBehavior::WriteToIOM)) {
      lunasa::DataObject ldo;
      rc = iom->ReadObject(default_bucket, key, &ldo);
      if(rc==KELPIE_OK){
        //We got it. Push it into lkv in order to trigger waiting callbacks
        lkv->put(default_bucket, key, ldo, behavior_flags, nullptr, nullptr);
      }
    }

    //Item belongs on this node, but we've already marked the lkv for it
    return KELPIE_OK;
  }

  
  if(expected_ldo_user_bytes > 0) {

    //We know length. Do a Bounded Get op
    auto *op = new OpKelpieGetBounded(
                                   nodes[spot].first, nodes[spot].second,
                                   default_bucket,
                                   key,
                                   expected_ldo_user_bytes,
                                   iom_hash,
                                   behavior_flags,
                                   [this] (bool success, Key &key, lunasa::DataObject &ldo) {
                                     //Only push into lkv if we had a successful fetch
                                    if(success) {
                                      lkv->put(default_bucket, key, ldo, behavior_flags, nullptr, nullptr);
                                    }
                                   });
    opbox::LaunchOp(op);

  } else {

    //Don't know length. Use an Unbounded Get op
    auto *op = new OpKelpieGetUnbounded(
                                   nodes[spot].first, nodes[spot].second,
                                   default_bucket,
                                   key,
                                   iom_hash,
                                   behavior_flags,
                                   //TODO: Can this be restricted to a smaller capture list and still work?
                                   [=] (bool success, Key &key, lunasa::DataObject &ldo) {
                                     //Only push into lkv if we had a successful fetch
                                    if(success) {
                                      lkv->put(default_bucket, key, ldo, behavior_flags, nullptr, nullptr);
                                    }
                                   });
    opbox::LaunchOp(op);

  }

  return KELPIE_OK;
}

/**
 * @brief Blocking request for a blob from a pool
 * @param key The Key label for the item
 * @param[in] expected_ldo_user_bytes The expected size of the item (if known), or zero (if unknown)
 * @param[out] returned_ldo The returned object
 * @retval KELPIE_OK Item was located
 */
rc_t DHTPool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){

  dbg("Need (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  std::promise<bool> found_promise;
  std::future<bool> found_future = found_promise.get_future();

  rc_t rc = Want(key, expected_ldo_user_bytes,
                 [&returned_ldo, &found_promise] (bool success, const Key &key, const lunasa::DataObject &result_ldo,
                                                  const object_info_t &info) {
      if(success) {
        *returned_ldo = result_ldo;
      } else {
        //This should not happen in current code. Leaving here in case we implement cancels
        throw  std::runtime_error("DHT Pool could not resolve need for "+key.str());
      }

      *returned_ldo = result_ldo;
      found_promise.set_value(true);

    });

  if(rc!=KELPIE_OK){
    F_TODO("DHTPool could not issue Need");
  }

  bool is_found = found_future.get();

  // There's currently no way for is_found to be false
  if( !is_found ) {
    throw  std::runtime_error("DHT Pool could not resolve need for "+key.str());
  }
  
  return KELPIE_OK;
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
rc_t DHTPool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) {
  dbg("Compute function "+function_name+" for key "+key.str());
  F_ASSERT(!key.IsRowWildcard(), "Requested a key with a row wildcard. Only column wildcards are supported");

  //Figure out which node in our list gets the spot
  uint32_t spot = findNodeIndex(key);
  rc_t rc;

  //See if this item belongs here, we don't send anything
  if(nodes[spot].first == my_nodeid) {
    lunasa::DataObject ldo;
    rc = lkv->doCompute(function_name, function_args, default_bucket, key, &ldo);
    callback(rc, key, ldo);
    return KELPIE_OK; //Always launch ok.. real result is handed back in callback
  }

  //Node lives elsewhere - start an op to dispatch it
  auto *op = new OpKelpieCompute(
            nodes[spot].first, nodes[spot].second,
            default_bucket,
            key,
            iom_hash,
            behavior_flags,
            function_name,
            function_args,
            callback);
  opbox::LaunchOp(op);

  return KELPIE_OK;
}

/**
 * @brief Get info about a particular key/blob. Does not wait for blob to be generated
 * @param[in] key The Key label for the blob
 * @param[out] info Basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @retval KELPIE_ENOENT Item should be on this node, but isn't and hasn't been requested
 */
rc_t DHTPool::Info(const Key &key, object_info_t *info){

  dbg("Info for key "+key.str());

  //Check local first. Only issue request if we aren't already waiting
  rc_t rc = lkv->getInfo(default_bucket, key, info);
  if((rc==KELPIE_OK) || (rc==KELPIE_WAITING)) return rc;


  bool got_result=false;
  uint32_t spot = findNodeIndex(key);

  //See if this node is supposed to be hosting the data. We've already checked, so IOM is only other option
  if(nodes[spot].first == my_nodeid) {
    //Item belongs on this node, but it's not in memory. Check on disk
    //if this pool has an iom associated with it.
    if(iom!=nullptr){
      rc = iom->GetInfo(default_bucket, key, info);
    }
    return rc;
  }

  //Wasn't available locally. Prepare to do some messaging
  std::promise<bool> found_promise;
  std::future<bool> found_future = found_promise.get_future();

  //Hosting node is somewhere else. Launch a new op to get the info
  opbox::LaunchOp( new OpKelpieMeta(DirectFlags::CMD_GET_COLINFO,
                              nodes[spot].first, nodes[spot].second,
                              default_bucket, key,
                              iom_hash,
                              [info, &got_result, &found_promise, &rc](rc_t result, object_info_t &new_info) {
                                   rc=result;
                                   if(info != nullptr) {
                                     if(result == 0) {
                                       *info = new_info;
                                     } else {
                                       info->Wipe();
                                     }
                                   }
                                   found_promise.set_value(true);
                                 }));

  bool is_found = found_future.get();

  //Switch a local memory label to a remote memory label
  if((info!=nullptr) && (rc==KELPIE_OK)){
    info->ChangeAvailabilityFromLocalToRemote();
  }

  return rc;
}

/**
 * @brief Get info about a particular row. Currently only looks locally
 * @param[in] key The Key label for the blob
 * @param[out] info Basic statistics about the row
 * @retval KELPIE_OK if info known
 */
rc_t DHTPool::RowInfo(const Key &key, object_info_t *info) {

  dbg("RowInfo for key "+key.str());

  //Check local first. Only issue request if we aren't already waiting
  rc_t rc = lkv->getInfo(default_bucket, key, info);
  if((rc==KELPIE_OK) || (rc==KELPIE_WAITING)) return rc;


  bool got_result=false;
  uint32_t spot = findNodeIndex(key);

  //See if this node is supposed to be hosting the data. We've already checked, so IOM is only other option
  if(nodes[spot].first == my_nodeid) {
    //todo: iom check: we could check the iom here
    return rc;
  }

  std::promise<bool> found_promise;
  std::future<bool> found_future = found_promise.get_future();

  //Hosting node is somewhere else. Launch a new op to get the info
  opbox::LaunchOp( new OpKelpieMeta(DirectFlags::CMD_GET_ROWINFO,
                                    nodes[spot].first, nodes[spot].second,
                                    default_bucket, key,
                                    iom_hash,
                                    [info, &got_result, &found_promise, &rc](rc_t result, object_info_t &new_info) {
                                        rc=result;
                                        if(info != nullptr) {
                                          if(result == 0) {
                                            *info = new_info;
                                          } else {
                                            info->Wipe();
                                          }
                                        }
                                        found_promise.set_value(true);
                                    }));

  bool is_found = found_future.get();

  //Switch a local memory label to a remote memory label
  if((info!=nullptr) && (rc==KELPIE_OK)){
    info->ChangeAvailabilityFromLocalToRemote();
  }

  return rc;
}


/**
 * @brief Signify that an item is no longer needed
 * @param key The Key label for the blob
 * @param callback Optional function to call if a response is required
 * @retval KELPIE_OK if info known
 */
rc_t DHTPool::Drop(const Key &key, fn_drop_callback_t callback) {

  dbg("Drop key "+key.str());

  //Check here first if caching
  rc_t rc_local = KELPIE_ENOENT;
  bool needs_local_search=(behavior_flags & (PoolBehavior::WriteToLocal | PoolBehavior::ReadToLocal));
  bool needs_external_search=false;

  vector<pair<faodel::nodeid_t, net::peer_ptr_t>> tmp_nodes;

  if( !key.IsRowWildcard() ) {
    uint32_t spot = findNodeIndex(key);
    if(nodes[spot].first == my_nodeid) {
      //Special case: we're the server.
      needs_local_search=true;
      needs_external_search=false;
    } else {
      //Single external node
      needs_external_search=true;
      tmp_nodes.push_back(nodes[spot]);
    }
  } else {
    //Row Wildcard. Build a list of external nodes to check
    for(auto &node_peer : nodes) {
      if (node_peer.first == my_nodeid) needs_local_search=true;
      else {
        needs_external_search=true;
        tmp_nodes.push_back(node_peer);
      }
    }
  }

  dbg("DHT-DROP: needs_local "+to_string(needs_local_search)+" needs_external "+to_string(needs_external_search)+ " num_targets: "+to_string(tmp_nodes.size()));

  //Actually do the local version
  if(needs_local_search) {
    rc_local = lkv->drop(default_bucket, key);
  }

  dbg("DHT-DROP: Cleared local, found was "+to_string(rc_local==KELPIE_OK)+", now working on remote");

  //See if we need to launch to external nodes
  if(needs_external_search) {
    opbox::LaunchOp(new OpKelpieDrop(std::move(tmp_nodes), default_bucket, key, (rc_local==KELPIE_OK), callback));

  } else if (callback) {
    callback((rc_local==KELPIE_OK), key);
  }

  return KELPIE_OK;

}


/**
 * @brief Perform a search for keys that match a specific pattern
 * @param[in] search_key The key to search for. Row and/or Key may end in '*' for prefix matching
 * @param[out] object_capacities Info about the objects that match this key search
 * @retval KELPIE_OK Found matches
 * @retval KELPIE_ENOENT Did not find matches
 */
rc_t DHTPool::List(const kelpie::Key &search_key, ObjectCapacities *object_capacities) {

  dbg("List key "+search_key.str());

  bool needs_local_search=false;
  bool needs_external_search=false;
  vector<pair<faodel::nodeid_t, net::peer_ptr_t>> tmp_nodes;

  if( !search_key.IsRowWildcard() ) {
    uint32_t spot = findNodeIndex(search_key);
    if(nodes[spot].first == my_nodeid) {
      //Special case: we're the server.
      needs_local_search=true;
      needs_external_search=false;
    } else {
      //Single external node
      needs_external_search=true;
      tmp_nodes.push_back(nodes[spot]);
    }
  } else {
    //Row Wildcard. Build a list of external nodes to check
    for(auto &node_peer : nodes) {
      if (node_peer.first == my_nodeid) needs_local_search=true;
      else {
        needs_external_search=true;
        tmp_nodes.push_back(node_peer);
      }
    }
  }



  if(needs_local_search) {
     lkv->list(default_bucket, search_key, iom, object_capacities);
  }

  if(needs_external_search) {
    //Start an op to go fetch results from one or more nodes
    std::promise<bool> found_promise;
    std::future<bool> found_future = found_promise.get_future();

    iom_hash_t iom_hash = GetIomHash();
    opbox::LaunchOp(new OpKelpieList(tmp_nodes, default_bucket, search_key, iom_hash,
            object_capacities, &found_promise));

    bool is_done = found_future.get(); //block
  }

  return KELPIE_OK;

}

/**
 * @brief Use the key's row info to determine which node is responsible for the data
 * @param key The Key label for the blob (only ROW portion used)
 * @param node_id The node id for the node that owns the data
 * @param peer_ptr The peer pointer for the node that owns the data
 * @return
 */
int DHTPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr){

  if(nodes.empty()) return 0;
  uint32_t spot = findNodeIndex(key);
  if(node_id)  *node_id  = nodes[spot].first;
  if(peer_ptr) *peer_ptr = nodes[spot].second;
  return 1;  //Always lives on one of these nodes
}

/**
 * @brief Determine the index of the DHT list that owns the data
 * @param key The Key label for the blob (only ROW portion used)
 * @return The index of the node in the DHT array that is responsible for the data
 */
uint32_t DHTPool::findNodeIndex(const Key &key){

  if(nodes.size()<2) return 0;  //Skip if no hash needed
  uint32_t spot = (faodel::hash_dbj2(default_bucket, key.K1() ) % nodes.size());
  return spot;
}

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void DHTPool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "DHTPool "<< endl;
  dir_info.sstr(ss, depth-1, indent+2);
  lkv->sstr(ss, depth-1,indent+1);
  //TODO: internals
}

/**
 * @brief Pool constructor function for creating a new DHTPool via a URL
 * @param pool_url The URL for the pool
 * @return New DHTPool
 */
shared_ptr<PoolBase> DHTPoolCreate(const ResourceURL &pool_url) {
  return make_shared<DHTPool>(pool_url);
}

}  // namespace kelpie
