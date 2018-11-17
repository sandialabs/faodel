// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "faodel-common/Common.hh"
#include "faodel-common/Debug.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/pools/DHTPool/DHTPool.hh"
#include "kelpie/ops/direct/OpKelpieGetBounded.hh"
#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"
#include "kelpie/ops/direct/OpKelpieMeta.hh"
#include "kelpie/ops/direct/OpKelpiePublish.hh"

#include "kelpie/core/Singleton.hh"

using namespace std;
using namespace faodel;

namespace kelpie {


DHTPool::DHTPool(const ResourceURL &pool_url)
  : PoolBase(pool_url) {


  bool ok = dirman::GetDirectoryInfo(pool_url, &dir_info);
  if(!ok){
    throw std::runtime_error("Pool "+TypeName()+" could not get directory info for "+pool_url.str());
  }

  //Allocate peers and connect to each one
  for(int i=0; i<dir_info.children.size(); i++){
    net::peer_ptr_t peer;
    int rc = net::Connect(&peer, dir_info.children[i].node);
    if(rc!=0) {
      throw std::runtime_error("Pool "+TypeName()+" could not connect to peer "+dir_info.children[i].name);
    }
    nodes.push_back(pair<nodeid_t, net::peer_ptr_t>(dir_info.children[i].node, peer));
  }

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

}
DHTPool::~DHTPool(){
  //lkv was allocated in core
}

/**
 * @brief Pull an item from the local store and asynchronously publish to the pool
 * @param key A global Key for referencing the blob
 * @param callback Function to call when this operation completes (whether success or failure)
 * @retval KELPIE_ENOENT The item wasn't found locally
 * @retval KELPIE_OK The request was successfully launched (failures may happen in callback)
 */
rc_t DHTPool::Publish(const Key &key, fn_publish_callback_t callback){

  dbg("Publish (from lkv) bucket "+default_bucket.GetHex()+" key "+key.str());

  //Retrieve the item from lkv so we can send to the destination
  lunasa::DataObject ldo;
  rc_t rc = lkv->get(default_bucket, key, &ldo, nullptr, nullptr);
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
                      fn_publish_callback_t callback) {

  //Figure out which node in our list gets the spot
  uint32_t spot = findNodeIndex(key);

  dbg("Publish ldo to dht node "+std::to_string(spot)+" for bucket "+default_bucket.GetHex()+" key "+key.str());


  //Skip ops if local
  if(nodes[spot].first == my_nodeid) {
    kv_row_info_t row_info;
    kv_col_info_t col_info;
    rc_t rc = lkv->put(default_bucket, key, user_ldo, behavior_flags, &row_info, &col_info);

    if((iom!=nullptr) && (behavior_flags & PoolBehavior::WriteToIOM)){
      iom->WriteObject(default_bucket, key, user_ldo);
    }

    if(callback)
      callback(rc, row_info, col_info);
    return KELPIE_OK;    
  }
  


  OpKelpiePublish *op = new OpKelpiePublish(
                                nodes[spot].first, nodes[spot].second,
                                default_bucket,
                                key,
                                user_ldo,
                                iom_hash,
                                behavior_flags,
                                callback
                                );

  opbox::LaunchOp(op);


  //TODO: Add lkv trigger so if anything waiting on publish

  return KELPIE_OK;
}

/**
 * @brief Request an item be brought to this node when published to the pool
 * @param key The Key for the desired blob
 * @param expected_ldo_user_bytes The expected size of the item (if known), or zero (if unknown)
 * @param callback Function to execute when the object arrives
 * @retval KELPIE_OK Request placed (or detected it's already been placed)
 */
rc_t DHTPool::Want(const Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback){

  dbg("Want (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  //Check the lkv to see if it already exists. If it does, have lkv do
  //the callback. If it doesn't, leave the callback in place so it can
  //trigger when the data arrives.
  rc_t rc = lkv->want(default_bucket, key, NODE_LOCALHOST, true, callback);
  if((rc==KELPIE_OK) || (rc==KELPIE_WAITING)) return KELPIE_OK;  //Already requested..



  //Figure out which node in our list gets the spot
  uint32_t spot = findNodeIndex(key);

  //See if this item belongs here, we don't send anything
  if(nodes[spot].first == my_nodeid) {
    //Item belongs on this node, but we've already marked the lkv for it
    return KELPIE_OK;
  }

  
  if(expected_ldo_user_bytes > 0) {

    //We know length. Do a Bounded Get op
    OpKelpieGetBounded *op = new OpKelpieGetBounded(
                                   nodes[spot].first, nodes[spot].second,
                                   default_bucket,
                                   key,
                                   expected_ldo_user_bytes,
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

  } else {

    //Don't know length. Use an Unbounded Get op
    OpKelpieGetUnbounded *op = new OpKelpieGetUnbounded(
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
 * @param expected_ldo_user_bytes The expected size of the item (if known), or zero (if unknown)
 * @param returned_ldo The returned object
 * @retval KELPIE_OK Item was located
 */
rc_t DHTPool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){

  dbg("Need (size="+to_string(expected_ldo_user_bytes)+") key "+key.str());

  std::mutex m;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lk(m);
  bool is_found=false;

  rc_t rc = Want(key, expected_ldo_user_bytes,
                 [&key, &returned_ldo, &cv, &is_found] (bool success, Key key, lunasa::DataObject result_ldo,
                                                        const kv_row_info_t &ri, const kv_col_info_t &c) {
      if(success) {
        *returned_ldo = result_ldo;
      } else {
        throw  std::runtime_error("DHT Pool could not resolve need for "+key.str());
        //TODO: better error handling
      }

      *returned_ldo = result_ldo;
      is_found=true;
      cv.notify_one();

    });

  while(!is_found)
    cv.wait(lk);

  return KELPIE_OK;
}

/**
 * @brief Get info about a particular key/blob. Does not wait for blob to be generated
 * @param key The Key label for the blob
 * @param col_info Basic statistics about the item
 * @retval KELPIE_OK The item was located and info obtained
 * @retval KELPIE_WAITING Local node is already waiting on item, so no request made
 * @retval KELPIE_ENOENT Item should be on this node, but isn't and hasn't been requested
 */
rc_t DHTPool::Info(const Key &key, kv_col_info_t *col_info){

  dbg("Info for key "+key.str());

  //Check local first. Only issue request if we aren't already waiting
  rc_t rc = lkv->getColInfo(default_bucket, key, col_info);
  if((rc==KELPIE_OK) || (rc==KELPIE_WAITING)) return rc;

  //Wasn't available locally. Prepare to do some messaging
  std::mutex m;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lk(m);

  bool got_result=false;
  uint32_t spot = findNodeIndex(key);

  //See if this node is supposed to be hosting the data. We've already checked, so IOM is only other option
  if(nodes[spot].first == my_nodeid) {
    //Item belongs on this node, but it's not in memory. Check on disk
    //if this pool has an iom associated with it.
    if(iom!=nullptr){
      rc = iom->GetInfo(default_bucket, key, col_info);
    }
    return rc;
  }
  
  //Hosting node is somewhere else. Launch a new op to get the info
  auto *op = new OpKelpieMeta(DirectFlags::CMD_GET_COLINFO,
                              nodes[spot].first, nodes[spot].second,
                              default_bucket, key,
                              iom_hash,
                              [col_info, &got_result, &cv, &rc](rc_t result, kv_row_info_t &ri, kv_col_info_t &ci) {
                                   rc=result;
                                   if(col_info != nullptr){
                                     if(result==0){
                                       *col_info = ci;
                                     } else {
                                       memset(col_info, 0, sizeof(kv_col_info_t));
                                     }
                                   }
                                   got_result=true;
                                   cv.notify_one();
                                 });
  opbox::LaunchOp(op);

  while(!got_result)
    cv.wait(lk);

  //Switch a local memory label to a remote memory label
  if((col_info!=nullptr) && (rc==0)){
    col_info->ChangeAvailabilityFromLocalToRemote();
  }

  return rc;
}

/**
 * @brief Get info about a particular row. Currently only looks locally
 * @todo Update to work on remotes nodes
 * @param key The Key label for the blob
 * @param row_info Basic statistics about the row
 * @retval KELPIE_OK if info known
 */
rc_t DHTPool::RowInfo(const Key &key, kv_row_info_t *row_info) {

  dbg("RowInfo for key "+key.str());

  uint32_t spot = findNodeIndex(key);
  if(nodes[spot].first == my_nodeid) {
    return lkv->getRowInfo(default_bucket, key, row_info);
  }
  KTODO("RowInfo does not currently support remote operations");
  return KELPIE_TODO;
}

/**
 * @brief Signify that an item is no longer needed
 * @note This currently only works on the local node
 * @todo Update state machine to trigger a drop of a value on remote side
 * @param key The Key label for the blob
 * @retval KELPIE_OK if info known
 */
rc_t DHTPool::Drop(const Key &key) {

  dbg("Drop key "+key.str());

  uint32_t spot = findNodeIndex(key);
  if(nodes[spot].first == my_nodeid) {
    return lkv->drop(default_bucket, key);
  }
  KTODO("Drop does not currently support remote operations");
  return KELPIE_TODO;
}

/**
 * @brief Use the key's row info to determine which node is responsible for the data
 * @param key The Key label for the blob (only ROW portion used)
 * @param node_id The node id for the node that owns the data
 * @param peer_ptr The peer pointer for the node that owns the data
 * @return
 */
int DHTPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr){

  if(nodes.size()<1) return 0;
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
