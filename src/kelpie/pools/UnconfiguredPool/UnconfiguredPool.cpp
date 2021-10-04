// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <exception>
#include <sstream>
#include <stdexcept>

#include "kelpie/pools/UnconfiguredPool/UnconfiguredPool.hh"

using namespace std;

namespace kelpie {

UnconfiguredPool::UnconfiguredPool()
  : PoolBase(faodel::ResourceURL("unconfigured:/")),
    error_message("Pool accessed before initialization") {
}
UnconfiguredPool::UnconfiguredPool(const std::string &error_message)
  : PoolBase(faodel::ResourceURL("unconfigured:/")),
    error_message(error_message) {
}
UnconfiguredPool::~UnconfiguredPool()= default;

rc_t UnconfiguredPool::Publish(const Key &key, const fn_publish_callback_t &callback)     { Panic("Publish"); return KELPIE_OK; }
rc_t UnconfiguredPool::Publish(const Key &key, const lunasa::DataObject &user_ldo,
                               const fn_publish_callback_t &callback)                     { Panic("Publish"); return KELPIE_OK; }
rc_t UnconfiguredPool::Want(const Key &key, size_t expected_ldo_user_bytes,
                            const fn_want_callback_t &callback)                           { Panic("Want");    return KELPIE_OK; }
rc_t UnconfiguredPool::Need(const Key &key, size_t expected_ldo_user_bytes,
                               lunasa::DataObject *user_ldo)                              { Panic("Need");    return KELPIE_OK; }

rc_t UnconfiguredPool::Compute(const Key &key, const std::string &function_name,
                               const std::string &function_args,
                               const fn_compute_callback_t &callback)                     { Panic("Compute"); return KELPIE_OK;}
rc_t UnconfiguredPool::Info(const Key &key, object_info_t *col_info)                      { Panic("Info");    return KELPIE_OK; }
rc_t UnconfiguredPool::RowInfo(const Key &key, object_info_t *row_info)                   { Panic("RowInfo"); return KELPIE_OK; }
rc_t UnconfiguredPool::Drop(const Key &key, fn_drop_callback_t callback)                  { Panic("Drop");    return KELPIE_OK; }
rc_t UnconfiguredPool::List(const Key &search_key, ObjectCapacities *capacities)          { Panic("List");    return KELPIE_OK; }

int UnconfiguredPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id,
                                net::peer_ptr_t *peer_ptr)                                { Panic("FindTargetNode"); return 0; }

void UnconfiguredPool::Panic(const std::string &caller) const {
  std::stringstream ss;
  ss << "Operation "<<caller<<"() attempted on an invalid pool." <<error_message;
  throw std::runtime_error(ss.str());
}

void UnconfiguredPool::sstr(std::stringstream &ss, int depth, int indent) const {
  ss<<std::string(indent,' ')<<"UnconfiguredPool\n";
}

/**
 * @brief Pool constructor function for creating a new ErrorPool with an error message
 * @param error_message Information about what went wrong
 * @return New UnconfiguredPool
 */
shared_ptr<PoolBase> UnconfiguredPoolCreate(const std::string &error_message) {
  return make_shared<UnconfiguredPool>(error_message);
}

}  // namespace kelpie
