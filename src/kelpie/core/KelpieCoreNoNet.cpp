// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/core/KelpieCoreNoNet.hh"
#include "kelpie/pools/LocalPool/LocalPool.hh"

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
  kassert(rc==KELPIE_OK, "lkv init failed");

  bucket_t bucket;
  config.GetDefaultSecurityBucket(&bucket);
  //Setup any ioms that are part of the Configuration
  iom_registry.init(config);
  pool_registry.init(bucket);
  
  //Register built-in resource creators
  pool_registry.RegisterPoolConstructor("local", &LocalPoolCreate);
  pool_registry.RegisterPoolConstructor("lkv",   &LocalPoolCreate);

  //Register webhook
  webhook::Server::updateHook("/kelpie", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWebhookStatus(args, results);
    });

}

void KelpieCoreNoNet::start(){
  iom_registry.start();
  pool_registry.start();  
}

void KelpieCoreNoNet::finish(){
  pool_registry.finish();
  iom_registry.finish();
  webhook::Server::deregisterHook("/kelpie");
}

void KelpieCoreNoNet::RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) {
  pool_registry.RegisterPoolConstructor(pool_name, ctor_function);
}

Pool KelpieCoreNoNet::Connect(const faodel::ResourceURL &resource_url) {
  return pool_registry.Connect(resource_url);
}

void KelpieCoreNoNet::RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) {
  iom_registry.RegisterIomConstructor(type, ctor_function);
}

void KelpieCoreNoNet::HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {

    webhook::ReplyStream rs(args, "Kelpie Status", &results);

    vector<pair<string,string>> stats;
    stats.push_back({"Parameter", "Setting"});
    stats.push_back({"Core Type", GetType()});
    rs.mkTable(stats, "Kelpie Status");
    lkv.webhookInfo(rs);

    rs.Finish();
}

void KelpieCoreNoNet::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ') + "[Kelpie:NoNet]\n";
  if(depth>0) lkv.sstr(ss, depth-1, indent+2);
}

}  // namespace internal
}  // namespace kelpie
