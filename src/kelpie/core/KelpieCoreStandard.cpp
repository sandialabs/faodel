// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/core/KelpieCoreStandard.hh"
#include "kelpie/pools/LocalPool/LocalPool.hh"
#include "kelpie/pools/DHTPool/DHTPool.hh"

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include "kelpie/pools/RFTPool/RFTPool.hh"
#endif

#include "kelpie/ops/direct/OpKelpieMeta.hh"
#include "kelpie/ops/direct/OpKelpiePublish.hh"
#include "kelpie/ops/direct/OpKelpieGetBounded.hh"
#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"
#include "kelpie/ops/direct/OpKelpieList.hh"



using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {

KelpieCoreStandard::KelpieCoreStandard(){
}

KelpieCoreStandard::~KelpieCoreStandard(){
}

void KelpieCoreStandard::init(const faodel::Configuration &config) {

  ConfigureLogging(config);

  rc_t rc = lkv.Init(config);
  kassert(rc==KELPIE_OK, "lkv init failed");
  iom_registry.init(config);
  pool_registry.init(config);
  
  
  //Register built-in pool creators
  pool_registry.RegisterPoolConstructor("local", &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("lkv",   &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("dht",   &DHTPoolCreate);

  #ifdef Faodel_ENABLE_MPI_SUPPORT
  pool_registry.RegisterPoolConstructor("rft",   &RFTPoolCreate);
  #endif

  //Register OPs with opbox, program in their lkv
  opbox::RegisterOp<OpKelpieMeta>();
  opbox::RegisterOp<OpKelpiePublish>();
  opbox::RegisterOp<OpKelpieGetBounded>();
  opbox::RegisterOp<OpKelpieGetUnbounded>();
  opbox::RegisterOp<OpKelpieList>();
  OpKelpieMeta::configure(faodel::internal_use_only, &lkv);
  OpKelpiePublish::configure(faodel::internal_use_only, &lkv);
  OpKelpieGetBounded::configure(faodel::internal_use_only, &lkv);
  OpKelpieGetUnbounded::configure(faodel::internal_use_only, &lkv);
  OpKelpieList::configure(faodel::internal_use_only, &config, &lkv);


  whookie::Server::updateHook("/kelpie", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWhookieStatus(args, results);
    });


}

void KelpieCoreStandard::start(){

}

void KelpieCoreStandard::finish(){
  whookie::Server::deregisterHook("/kelpie");
  OpKelpieMeta::configure(faodel::internal_use_only, nullptr);  //TODO: Would be nice to be a dummy lkv
  OpKelpiePublish::configure(faodel::internal_use_only, nullptr);  //TODO: Would be nice to be a dummy lkv
  OpKelpieGetBounded::configure(faodel::internal_use_only, nullptr);  //TODO: Would be nice to be a dummy lkv
  OpKelpieGetUnbounded::configure(faodel::internal_use_only, nullptr);  //TODO: Would be nice to be a dummy lkv
  OpKelpieList::configure(faodel::internal_use_only, nullptr, nullptr);
  pool_registry.finish();
  iom_registry.finish();
}

void KelpieCoreStandard::RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) {
  pool_registry.RegisterPoolConstructor(pool_name, ctor_function);
}

Pool KelpieCoreStandard::Connect(const faodel::ResourceURL &resource_url) {
  return pool_registry.Connect(resource_url);
}

void KelpieCoreStandard::RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) {
  iom_registry.RegisterIomConstructor(type, ctor_function);
}

IomBase * KelpieCoreStandard::FindIOM(iom_hash_t iom_hash) {
  return iom_registry.Find(iom_hash);
}

void KelpieCoreStandard::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {
  faodel::ReplyStream rs(args, "Kelpie Status", &results);

  vector<pair<string,string>> stats;
  stats.push_back(pair<string,string>("Core Type", GetType()));
  rs.mkTable(stats, "Kelpie Status");
  lkv.whookieInfo(rs);

  rs.Finish();
}

void KelpieCoreStandard::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ') + "[Kelpie:Standard]\n";
  if(depth>0) lkv.sstr(ss, depth-1, indent+2);
}


}  // namespace internal
}  // namespace kelpie
