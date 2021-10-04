// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <kelpie/pools/NullPool/NullPool.hh>
#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/core/KelpieCoreNoNet.hh"
#include "kelpie/pools/LocalPool/LocalPool.hh"
#include "kelpie/pools/NullPool/NullPool.hh"
#include "kelpie/pools/TracePool/TracePool.hh"



using namespace std;
using namespace faodel;


namespace kelpie {
namespace internal {

KelpieCoreNoNet::KelpieCoreNoNet(){
}

KelpieCoreNoNet::~KelpieCoreNoNet(){
}

void KelpieCoreNoNet::init(const faodel::Configuration &config) {

  ConfigureLogging(config);
  
  rc_t rc = lkv.Init(config);
  F_ASSERT(rc == KELPIE_OK, "lkv init failed");
  iom_registry.init(config);  //Setup any ioms that are part of the Configuration
  pool_registry.init(config); //Setup logging/default bucket
  compute_registry.init(config); //Setup built-in compute functions

  //Register built-in resource creators
  pool_registry.RegisterPoolConstructor("local", &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("lkv",   &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("null",  &NullPoolCreate);
  pool_registry.RegisterPoolConstructor("trace", &TracePoolCreate);

  //Register whookie
  whookie::Server::updateHook("/kelpie", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWhookieStatus(args, results);
    });

}

void KelpieCoreNoNet::start(){
  iom_registry.start();
  pool_registry.start();  
}

void KelpieCoreNoNet::finish(){
  pool_registry.finish();
  iom_registry.finish();
  whookie::Server::deregisterHook("/kelpie");
}

void KelpieCoreNoNet::RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) {
  pool_registry.RegisterPoolConstructor(pool_name, ctor_function);
}

Pool KelpieCoreNoNet::Connect(const faodel::ResourceURL &resource_url) {
  return pool_registry.Connect(resource_url);
}

int KelpieCoreNoNet::JoinServerPool(const faodel::ResourceURL &url, const std::string &optional_node_name) {
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

vector<string> KelpieCoreNoNet::GetRegisteredPoolTypes() const {
  return pool_registry.GetRegisteredPoolTypes();
}

void KelpieCoreNoNet::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {

    faodel::ReplyStream rs(args, "Kelpie Status", &results);

    vector<pair<string,string>> stats;
    stats.push_back({"Parameter", "Setting"});
    stats.push_back({"Core Type", GetType()});
    rs.mkTable(stats, "Kelpie Status");
    lkv.whookieInfo(rs);

    rs.Finish();
}

void KelpieCoreNoNet::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ') + "[Kelpie:NoNet]\n";
  if(depth>0) lkv.sstr(ss, depth-1, indent+2);
}

}  // namespace internal
}  // namespace kelpie
