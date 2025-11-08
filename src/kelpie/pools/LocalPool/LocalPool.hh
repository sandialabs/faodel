// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_LOCALPOOL_HH
#define KELPIE_LOCALPOOL_HH

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "faodel-common/Common.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"

#include "kelpie/ioms/IomBase.hh"

#include "lunasa/DataObject.hh"

namespace kelpie {

/**
 * @brief A handle for interacting only with the node's local key/blob store
 *
 * A LocalPool is a simple handle for inspecting the contents of the node's
 * local key/blob cache. The functions used in this pool do not incur
 * network operations and therefore can be used with the NoNet core for
 * testing.
 */
class LocalPool : public PoolBase {

public:

  explicit LocalPool(const faodel::ResourceURL &pool_url);
  ~LocalPool() override = default;

  //PoolBase functions
  rc_t Publish(const Key &key, const fn_publish_callback_t &callback) override;
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, const fn_publish_callback_t &callback) override;

  rc_t Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback) override;  //Notify when available
  rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo) override;  //Block until get

  rc_t Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) override; //Async compute

  rc_t Info(const Key &key, object_info_t *col_info) override;
  rc_t RowInfo(const Key &key, object_info_t *row_info) override;
  rc_t Drop(const Key &key, fn_drop_callback_t callback) override;
  rc_t List(const Key &search_key, ObjectCapacities *object_capacities) override;

  int FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr) override;

  std::string TypeName() const override { return "local"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;


};  //LocalPool


//For use by connect
std::shared_ptr<PoolBase> LocalPoolCreate(const faodel::ResourceURL &pool_url);


}  // namespace kelpie

#endif  // KELPIE_LOCALPOOL_HH

