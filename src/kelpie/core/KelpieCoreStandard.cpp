// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/core/KelpieCoreStandard.hh"
#include "kelpie/pools/LocalPool/LocalPool.hh"
#include "kelpie/pools/NullPool/NullPool.hh"
#include "kelpie/pools/TracePool/TracePool.hh"
#include "kelpie/pools/DHTPool/DHTPool.hh"

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include "kelpie/pools/RFTPool/RFTPool.hh"
#endif

#include "kelpie/ops/direct/OpKelpieCompute.hh"
#include "kelpie/ops/direct/OpKelpieDrop.hh"
#include "kelpie/ops/direct/OpKelpieGetBounded.hh"
#include "kelpie/ops/direct/OpKelpieGetUnbounded.hh"
#include "kelpie/ops/direct/OpKelpieList.hh"
#include "kelpie/ops/direct/OpKelpieMeta.hh"
#include "kelpie/ops/direct/OpKelpiePublish.hh"


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
  F_ASSERT(rc == KELPIE_OK, "lkv init failed");
  iom_registry.init(config);
  pool_registry.init(config);
  compute_registry.init(config);
  
  //Register built-in pool creators
  pool_registry.RegisterPoolConstructor("local", &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("lkv",   &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("null",  &NullPoolCreate);
  pool_registry.RegisterPoolConstructor("dht",   &DHTPoolCreate);
  pool_registry.RegisterPoolConstructor("trace", &TracePoolCreate);

  #ifdef Faodel_ENABLE_MPI_SUPPORT
  pool_registry.RegisterPoolConstructor("rft",   &RFTPoolCreate);
  #endif

  //Register OPs with opbox, program in their lkv
  opbox::RegisterOp<OpKelpieCompute>();
  opbox::RegisterOp<OpKelpieDrop>();
  opbox::RegisterOp<OpKelpieGetBounded>();
  opbox::RegisterOp<OpKelpieGetUnbounded>();
  opbox::RegisterOp<OpKelpieList>();
  opbox::RegisterOp<OpKelpieMeta>();
  opbox::RegisterOp<OpKelpiePublish>();

  OpKelpieCompute::configure(     faodel::internal_use_only, &config, &lkv);
  OpKelpieDrop::configure(        faodel::internal_use_only, &config, &lkv);
  OpKelpieGetBounded::configure(  faodel::internal_use_only, &config, &lkv);
  OpKelpieGetUnbounded::configure(faodel::internal_use_only, &config, &lkv);
  OpKelpieList::configure(        faodel::internal_use_only, &config, &lkv);
  OpKelpieMeta::configure(        faodel::internal_use_only, &config, &lkv);
  OpKelpiePublish::configure(     faodel::internal_use_only, &config, &lkv);


  whookie::Server::updateHook("/kelpie", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWhookieStatus(args, results);
    });


}

void KelpieCoreStandard::start(){

}

void KelpieCoreStandard::finish(){
  whookie::Server::deregisterHook("/kelpie");
  OpKelpieDrop::configure(faodel::internal_use_only, nullptr, nullptr);
  OpKelpieGetBounded::configure(faodel::internal_use_only, nullptr, nullptr);
  OpKelpieGetUnbounded::configure(faodel::internal_use_only, nullptr, nullptr);
  OpKelpieList::configure(faodel::internal_use_only, nullptr, nullptr);
  OpKelpieMeta::configure(faodel::internal_use_only, nullptr, nullptr );
  OpKelpiePublish::configure(faodel::internal_use_only, nullptr, nullptr);
  pool_registry.finish();
  iom_registry.finish();
}

void KelpieCoreStandard::RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) {
  pool_registry.RegisterPoolConstructor(pool_name, ctor_function);
}

Pool KelpieCoreStandard::Connect(const faodel::ResourceURL &resource_url) {
  return pool_registry.Connect(resource_url);
}

vector<string> KelpieCoreStandard::GetRegisteredPoolTypes() const {
  return pool_registry.GetRegisteredPoolTypes();
}

int KelpieCoreStandard::JoinServerPool(const faodel::ResourceURL &url, const std::string &optional_node_name) {
  bool ok; int rc=0;
  DirectoryInfo dir_info;
  if(optional_node_name.empty()) { ok = dirman::JoinDirWithoutName(url, &dir_info);
  } else {                         ok = dirman::JoinDirWithName(url, optional_node_name, &dir_info);
  }
  if(ok) {
    string iom_name = dir_info.url.GetOption("iom");
    if(!iom_name.empty()) {
      IomBase *iom = iom_registry.Find(iom_name);
      if(iom==nullptr) {
         rc=iom_registry.RegisterIomFromURL(dir_info.url);
         if(rc!=0) return -1; //expect an exception
      }
    }
  }
  return 0;
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
