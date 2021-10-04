// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <stdexcept>

#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolRegistry.hh"
#include "kelpie/pools/PoolBase.hh"
#include "kelpie/pools/UnconfiguredPool/UnconfiguredPool.hh" //Built in for errors

using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {


PoolRegistry::PoolRegistry()
  : LoggingInterface("kelpie.pool_registry"),
    started(false),
    default_bucket(faodel::BUCKET_UNSPECIFIED),
    default_pool_logging_level(0) {
  mutex = faodel::GenerateMutex();
}

PoolRegistry::~PoolRegistry() {
  if(mutex) delete mutex;
}

void PoolRegistry::init(const faodel::Configuration &config) {

  ConfigureLogging(config); //Set registry's logging level
  config.GetDefaultSecurityBucket(&default_bucket);
  default_pool_logging_level = LoggingInterface::GetLoggingLevelFromConfiguration(config, "kelpie.pool");

  whookie::Server::updateHook("/kelpie/pool_registry", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWhookieStatus(args, results);
    });

}
void PoolRegistry::start(){
  started=true; //Prevent anyone from registering new pool types after this point
}

void PoolRegistry::finish() {
  dbg("Finishing");
  whookie::Server::deregisterHook("/kelpie/pool_registry");

  pool_create_fns.clear();
  mutex->Lock();
  //TODO: Remove this or find a way to replace impls with UnconfiguredPools
  for( auto &url_rptr : known_pools){
    dbg("Removing pool "+url_rptr.second->GetFullName());
    if(url_rptr.second.use_count()>1){
      cout<<"Warning: shutting down with user-space pools still open\n";
    }
  }
  known_pools.clear();
  mutex->Unlock();
  started=false;
}

void PoolRegistry::RegisterPoolConstructor(const std::string &pool_name, fn_PoolCreate_t function_pointer){

  F_ASSERT(!started, "Attempted to register pool constructor after bootstrap Start().");

  dbg("Registering pool ctor "+pool_name);

  auto name_fn = pool_create_fns.find(pool_name);
  F_ASSERT(name_fn == pool_create_fns.end(), "Attempting to overwrite existing pool constructor for "+pool_name);
  pool_create_fns[pool_name]=function_pointer;
}

Pool PoolRegistry::Connect(const ResourceURL &pool_url){

  dbg("Connect to "+pool_url.GetFullURL());

  //Make this url more uniform. Lookup kelpie's default bucket if not available
  ResourceURL src_url = pool_url;  //may need to modify
  if(src_url.bucket == BUCKET_UNSPECIFIED){
    src_url.bucket = default_bucket;
  }

  if(src_url.IsReference()) {

    DirectoryInfo dir_info;

    dbg("Consulting Dirman to resolve pool reference");
    bool ok = dirman::GetDirectoryInfo(src_url, &dir_info);
    if(!ok) {
      auto nodeid = dirman::GetAuthorityNode();
      return createErrorPool("During pool construction, DirMan ("+nodeid.GetHttpLink()+") could not resolve "+src_url.str());
    }

    //Switch to dirman's version of url (has more info)
    src_url = dir_info.url;

    //..but patch up the url with any settings the user also provided (allows bucket override)
    auto original_options = pool_url.GetOptions();
    for(auto &kv : original_options) {
      src_url.SetOption(kv.first, kv.second);
    }
  }

  string pool_tag = makeKnownPoolTag(src_url);

  //See if we already know about this pool. Reuse it if we do.
  mutex->ReaderLock();
  auto rname_rptr = known_pools.find(pool_tag);
  if(rname_rptr != known_pools.end() ){
    dbg("Found existing pool. Using "+rname_rptr->first);
    Pool p(faodel::internal_use_only, rname_rptr->second);
    mutex->Unlock();
    return p;
  }
  mutex->Unlock(); //Unlock here because pool_create_fns only changes during registry


  dbg("Existing pool instance not found. Creating.");

  //Allocate a new PoolBase, since this is a new Pool
  //Look up the allocation function in our table of creators
  auto name_ctor = pool_create_fns.find(src_url.Type());
  if(name_ctor==pool_create_fns.end()) {
    return createErrorPool("Pool registry could not find constructor for pool "+src_url.GetURL());
  }

  //Create a pool instance
  shared_ptr<PoolBase> rbase_ptr =  name_ctor->second(src_url);
  rbase_ptr->SetLoggingLevel(default_pool_logging_level);


  //Writer Lock: register this pool. We must recheck because someone may have created same thing while out of lock
  mutex->WriterLock();
  rname_rptr = known_pools.find(pool_tag);
  if(rname_rptr != known_pools.end() ) {
    //Someone beat us. Get rid of the created pool and use the existing one
    dbg("Found existing pool. Using "+rname_rptr->first);
    rbase_ptr = rname_rptr->second;

  } else {
    //Remember the ptr we just created
    known_pools[pool_tag] = rbase_ptr;
  }

  //Plug this into a pool, unlock and return
  Pool p(faodel::internal_use_only, rbase_ptr);
  mutex->Unlock();
  return p;
}

vector<string> PoolRegistry::GetRegisteredPoolTypes() const {
  vector<string> types;
  for(auto &name_fn : pool_create_fns) {
    types.push_back(name_fn.first);
  }
  return types;
}

/**
 * @brief Rather than throw an exception, create a pool with an error message in it for handling by user
 * @param error_message What broke this pool
 * @return
 */
Pool PoolRegistry::createErrorPool(const std::string &error_message) {
  return Pool(internal_use_only, UnconfiguredPoolCreate(error_message));
}

void PoolRegistry::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {
    faodel::ReplyStream rs(args, "Kelpie Pool Registry", &results);

    vector<pair<string,string>> stats;

    //Just a one-column table with function names for now.. Maybe include pointers later?
    vector<vector<string>> pool_names;
    pool_names.push_back({"Register Pool Names"});
    for(auto &name_fn : pool_create_fns){
      vector<string> row;
      row.push_back(name_fn.first);
      pool_names.push_back(row);
    }
    rs.mkTable(pool_names, "Pool Constructor Functions");

    mutex->Lock();
    vector<vector<string>> existing_pools;
    existing_pools.push_back({"Type", "Bucket", "Name", "Behavior", "Iom", "IomDetail",  "RefCount","NumNodes","Info", "ID"});
    for(auto &url_bptr : known_pools){
      vector<string> row;
      row.push_back(url_bptr.second->TypeName());
      row.push_back(url_bptr.second->GetBucket().GetHex());
      row.push_back(url_bptr.second->GetURL().GetPathName());

      //row.push_back(PoolBehavior::GetString(url_bptr.second->GetBehavior()));
      int behavior = url_bptr.second->GetBehavior();
      stringstream behave_hex;
      behave_hex << "0x"<<hex<<behavior;
      row.push_back(behave_hex.str());

      row.push_back(url_bptr.second->GetIomName(true,false));
      row.push_back(url_bptr.second->GetIomName(true,true));

      row.push_back(to_string(url_bptr.second.use_count()));
      auto di = url_bptr.second->GetDirectoryInfo();
      row.push_back(std::to_string(di.members.size()));
      row.push_back(di.info);
      row.push_back(url_bptr.first);
      existing_pools.push_back(row);
    }
    mutex->Unlock();
    rs.mkTable(existing_pools, "Known Pools");
    rs.Finish();
}

}  //namespace internal
}  //namespace kelpie

