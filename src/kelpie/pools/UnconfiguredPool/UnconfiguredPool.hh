// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_UNCONFIGUREDPOOL_HH
#define KELPIE_UNCONFIGUREDPOOL_HH

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "faodel-common/Common.hh"


#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"

#include "lunasa/DataObject.hh"

namespace kelpie {

/**
 * @brief A dummy implementation of a pool that panics on any operation
 *
 * This class is provided as a debugging resource to protect against times
 * when a user requests a pool that cannot be located. The intent is for
 * this pool to trigger a panic operation on any call, in order to help
 * identify that a bad pool request was made.
 *
 * Users should check the pool.Valid() option after connect to make sure
 * it's valid. If they don't and make a call on a bad pool, they'll wind
 * up here, and will receive an exception.
 */
class UnconfiguredPool : public PoolBase {

public:

  UnconfiguredPool();
  explicit UnconfiguredPool(const std::string &error_message);
  ~UnconfiguredPool() override;

  //PoolBase functions
  rc_t Publish(const Key &key, const fn_publish_callback_t &callback) override;
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, const fn_publish_callback_t &callback) override;

  rc_t Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback) override;
  rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo) override;  //Block until get

  rc_t Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) override; //Async compute

  rc_t Info(const Key &key, object_info_t *col_info) override;
  rc_t RowInfo(const Key &key, object_info_t *row_info) override;
  rc_t Drop(const Key &key, fn_drop_callback_t callback) override;
  rc_t List(const Key &search_key, ObjectCapacities *object_capacities) override;

  int FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr) override;
  std::string TypeName() const override { return "unconfigured"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

  std::string error_message;

private:
  void Panic(const std::string &caller) const;

};  //UnconfiguredPool


//For use by connect
std::shared_ptr<PoolBase> UnconfiguredPoolCreate(const std::string &error_message);


}  // namespace kelpie

#endif  // KELPIE_UNCONFIGUREDPOOL_HH

