// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_DHTPOOL_HH
#define KELPIE_DHTPOOL_HH

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "faodel-common/Common.hh"

#include "opbox/net/net.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"

#include "lunasa/DataObject.hh"

namespace kelpie {

/**
 * @brief Handle to a Distributed Hash Table (DHT) Pool
 *
 * A Distributed Hash Table (DHT) is a pool that spreads data out to a
 * collection of nodes by using a hash of the row part of a key to
 * determine which node should own a particular object. A DHTPool has
 * a static list of one or more nodes participating in the pool and does
 * not provide a mechanism for recovery if any of the nodes stop functioning.
 *
 * A DHTPool with only one node functions as a way to do direct communication.
 *
 * It is important to understand that only the row portion of the key is
 * used in the location hash. Users may use this trait in a positive way to
 * force a number of releate items to be pushed to the same DHT node (eg, use
 * a constant row name, but set different column names in the keys of your
 * related items). Similarly, users may inadvertently cause distribution
 * imbalances by picking bad labels (eg, setting the row to "timestep1" and
 * the column to a variable name like "pressure").
 */
class DHTPool : public PoolBase {

public:

  DHTPool(const faodel::ResourceURL &pool_url);
  ~DHTPool() override;

  //PoolBase functions
  rc_t Publish(const Key &key, fn_publish_callback_t callback) override;  //Lookup a local k/v and publish it
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, fn_publish_callback_t callback) override;

  rc_t Want(const Key &key,  size_t expected_ldo_user_bytes, fn_want_callback_t callback) override;  //Notify when available

  rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo) override;  //Block until get

  rc_t Info(const Key &key, kv_col_info_t *col_info) override;
  rc_t RowInfo(const Key &key, kv_row_info_t *row_info) override;
  rc_t Drop(const Key &key) override;
  rc_t List(const Key &search_key, ObjectCapacities *object_capacities=nullptr) override;

  int FindTargetNode(const Key &key, faodel::nodeid_t *node_id=nullptr, net::peer_ptr_t *peer_ptr=nullptr) override;

  std::string TypeName() const override { return "dht"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

protected:
  std::vector<std::pair<faodel::nodeid_t, net::peer_ptr_t>> nodes;

  virtual uint32_t findNodeIndex(const Key &key);

};  //DHTPool


//For use by connect
std::shared_ptr<PoolBase> DHTPoolCreate(const faodel::ResourceURL &pool_url);


}  // namespace kelpie

#endif  // KELPIE_DHTPOOL_HH

