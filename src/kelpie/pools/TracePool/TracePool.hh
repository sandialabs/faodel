// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_TRACEPOOL_HH
#define FAODEL_TRACEPOOL_HH


#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <mutex>

#include "faodel-common/Common.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"
#include "kelpie/pools/NullPool/NullPool.hh"

#include "lunasa/DataObject.hh"



namespace kelpie {

/**
 * @brief A handle for interacting only with the node's local key/blob store
 *
 * A TracePool is a trivial pool that only captures information about when different operations were requested
 */

class TracePool : public PoolBase {

public:

  explicit TracePool(const faodel::ResourceURL &pool_url);
  ~TracePool() override = default;

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

  std::string TypeName() const override { return "trace"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

private:
  Pool next_pool;

  std::chrono::time_point<std::chrono::high_resolution_clock> t_start; //!< Time when this TracePool was created
  std::chrono::time_point<std::chrono::high_resolution_clock> t_last; //!< Time of last operation


  std::mutex f_mutex;
  std::ofstream f;
  std::string rank_flag;
  bool use_relative_time;
  void appendTrace(const std::string &cmd, const std::string &s);


};

//For use by connect
std::shared_ptr<PoolBase> TracePoolCreate(const faodel::ResourceURL &pool_url);



}  // namespace kelpie

#endif //FAODEL_TRACEPOOL_HH
