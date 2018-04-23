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



Pool::Pool() {
  //Stick an unconfigured pool here until user gets to us.
  //We host an unconfigured pool in the singleton to avoid alloc/free
  impl = kelpie::internal::Singleton::impl.unconfigured_pool;
}

Pool::Pool(faodel::internal_use_only_t iuo, shared_ptr<PoolBase> base)
  : impl(base) {
}

//Shallow copy
Pool::Pool(const Pool &source) {
  impl = source.impl;
}

//Move
Pool::Pool(Pool &&source) {
  impl = std::move(source.impl);
  source.impl = kelpie::internal::Singleton::impl.unconfigured_pool;
}

Pool::~Pool() {
}

Pool& Pool::operator=(const Pool &source){
  impl = source.impl;
  return *this;
}

Pool& Pool::operator=(Pool &&source){
  impl = std::move(source.impl);
  return *this;
}

rc_t Pool::Publish(const Key &key, fn_publish_callback_t callback){
  return impl->Publish(key, callback);
}
rc_t Pool::Publish(const Key &key, lunasa::DataObject const &user_ldo, fn_publish_callback_t callback){
  return impl->Publish(key, user_ldo, callback);
}
rc_t Pool::Want(const  Key &key, fn_want_callback_t callback){
  return impl->Want(key, 0, callback);
}
rc_t Pool::Want(const  Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback){
  return impl->Want(key, expected_ldo_user_bytes, callback);
}
rc_t Pool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){
  return impl->Need(key, expected_ldo_user_bytes, returned_ldo);
}
rc_t Pool::Info(const Key &key, kv_col_info_t *col_info) {
  return impl->Info(key, col_info);
}
rc_t Pool::RowInfo(const Key &key, kv_row_info_t *row_info){
  return impl->RowInfo(key, row_info);
}
rc_t Pool::Drop(const Key &key) {
  return impl->Drop(key);
}
int Pool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr) {
  return impl->FindTargetNode(key, node_id, peer_ptr);
}


faodel::bucket_t Pool::GetBucket()            { return impl->GetBucket(); }
faodel::ResourceURL Pool::GetURL()            { return impl->GetURL(); }
opbox::DirectoryInfo Pool::GetDirectoryInfo() { return impl->GetDirectoryInfo(); }
int Pool::GetRefCount()                       { return impl.use_count(); }

void Pool::sstr(std::stringstream &ss, int depth, int indent) const { return impl->sstr(ss, depth, indent); }

}  // namespace kelpie

