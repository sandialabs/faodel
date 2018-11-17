// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <exception>
#include <sstream>
#include <stdexcept>

#include "kelpie/pools/UnconfiguredPool/UnconfiguredPool.hh"

namespace kelpie {

UnconfiguredPool::UnconfiguredPool()
  : PoolBase(faodel::ResourceURL("unconfigured:/")) {
}
UnconfiguredPool::~UnconfiguredPool(){
}

rc_t UnconfiguredPool::Publish(const Key &key, fn_publish_callback_t callback)     { Panic("Publish"); return KELPIE_OK; }
rc_t UnconfiguredPool::Publish(const Key &key, const lunasa::DataObject &user_ldo,
                               fn_publish_callback_t callback)                     { Panic("Publish"); return KELPIE_OK; }
rc_t UnconfiguredPool::Want(const Key &key, size_t expected_ldo_user_bytes,
                               fn_want_callback_t callback)                        { Panic("Want");    return KELPIE_OK; }
rc_t UnconfiguredPool::Need(const Key &key, size_t expected_ldo_user_bytes,
                            lunasa::DataObject *user_ldo)                          { Panic("Need");    return KELPIE_OK; }
rc_t UnconfiguredPool::Info(const Key &key, kv_col_info_t *col_info)               { Panic("Info");    return KELPIE_OK; }
rc_t UnconfiguredPool::RowInfo(const Key &key, kv_row_info_t *row_info)            { Panic("RowInfo"); return KELPIE_OK; }
rc_t UnconfiguredPool::Drop(const Key &key)                                        { Panic("Drop");    return KELPIE_OK; }
int UnconfiguredPool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id,
                                     net::peer_ptr_t *peer_ptr)                    { Panic("FindTargetNode"); return 0; }

void UnconfiguredPool::Panic(std::string caller) const {
  std::stringstream ss;
  ss << "Kelpie pool was not initialized before calling "<<caller<<"()";
  throw std::runtime_error(ss.str());
}

void UnconfiguredPool::sstr(std::stringstream &ss, int depth, int indent) const {
  ss<<std::string(indent,' ')<<"UnconfiguredPool\n";
}

}  // namespace kelpie
