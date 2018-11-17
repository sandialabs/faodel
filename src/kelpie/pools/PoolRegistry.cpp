// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <stdexcept>

#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolRegistry.hh"
#include "kelpie/pools/PoolBase.hh"


//TODO: This should have locks around pool creation

using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {


PoolRegistry::PoolRegistry()
  : default_bucket(faodel::BUCKET_UNSPECIFIED),
    default_pool_logging_level(0) {
  mutex = faodel::GenerateMutex();
}

PoolRegistry::~PoolRegistry() {
  if(mutex) delete mutex;
}

void PoolRegistry::init(const faodel::Configuration &config) {

  config.GetDefaultSecurityBucket(&default_bucket);
  default_pool_logging_level = LoggingInterface::GetLoggingLevelFromConfiguration(config, "kelpie.pool");

  webhook::Server::updateHook("/kelpie/pool_registry", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWebhookStatus(args, results);
    });

}
void PoolRegistry::start(){
  //TODO: Add tag stuff to separate from pre-start/post-start registrations?
}

void PoolRegistry::finish() {
  webhook::Server::deregisterHook("/kelpie/pool_registry");

  pool_create_fns.clear();
  mutex->Lock();
  //TODO: Remove this or find a way to replace impls with UnconfiguredPools
  for( auto &url_rptr : known_pools){
    if(url_rptr.second.use_count()>1){
      cout<<"Warning: shutting down with user-space pools still open\n";
    }
  }
  known_pools.clear();
  mutex->Unlock();
}

void PoolRegistry::RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t function_pointer){
  map<string,fn_PoolCreate_t>::iterator ii = pool_create_fns.find(pool_name);
  kassert(ii==pool_create_fns.end(), "Attempting to overwrite existing pool constructor for "+pool_name);
  pool_create_fns[pool_name]=function_pointer;
}

Pool PoolRegistry::Connect(const ResourceURL &pool_url){

  //Make this url more uniform. Lookup kelpie's default bucket if not available
  ResourceURL src_url = pool_url;  //may need to modify
  if(src_url.bucket == BUCKET_UNSPECIFIED){
    src_url.bucket = default_bucket;
  }

  //cout <<"Input url is '"<<src_url.GetFullURL()<<"'  type='"<<src_url.resource_type <<"' path='"<<src_url.path<<"'\n";

  if(src_url.resource_type=="ref") {

    DirectoryInfo dir_info;

    bool ok = dirman::GetDirectoryInfo(src_url, &dir_info);
    if(!ok){
      throw std::runtime_error("Reference not known during Connect to "+src_url.str());
    }
    src_url = dir_info.url; 

    //Copy user requested options onto new url
    auto original_options = pool_url.GetOptions();
    for(auto &kv : original_options) {
      src_url.SetOption(kv.first, kv.second);
    }
  }



  string pool_key = makeKnownPoolKey(src_url);

  //cout <<"Input url is '"<<src_url.GetFullURL()<<"'  type='"<<src_url.resource_type <<"' path='"<<src_url.path<<"' PoolKey='"<<pool_key<<"'\n";


  //See if we already know about this pool. Reuse it if we do.
  mutex->Lock();
  auto rname_rptr = known_pools.find(pool_key);
  if(rname_rptr != known_pools.end() ){
    //cout <<"Found existing pool. Using "<<rname_rptr->first;
    Pool p(faodel::internal_use_only, rname_rptr->second);
    mutex->Unlock();
    return p;
  }

  //Allocate a new PoolBase, since this is a new Pool
  //Look up the allocation function in our table of creators
  auto name_ctor = pool_create_fns.find(src_url.resource_type);
  if(name_ctor==pool_create_fns.end()) {
    mutex->Unlock();
    throw std::runtime_error("Pool registry could not find ctor for pool "+src_url.GetURL());
  }
  

  //cout<<"PR: ITEM MISISNG. Before adding "<<src_url.GetFullURL()<<endl;
  //for(auto &src_ptr : known_pools) {
  //  cout << src_ptr.first <<endl;
  //}
  //cout<<"New pool. Creating "<<pool_key<<" with url "<<src_url.GetFullURL()<<endl;
   
  shared_ptr<PoolBase> rbase_ptr =  name_ctor->second(src_url);
  rbase_ptr->SetLoggingLevel(default_pool_logging_level);
  known_pools[pool_key] = rbase_ptr;

  
  //cout<<"PR: ITEM MISSING. Afer adding:\n";
  //for(auto &src_ptr : known_pools) {
  //  cout << src_ptr.first <<endl;
  //}
  //cout<<"PR END\n";
  Pool p(faodel::internal_use_only, rbase_ptr);
  mutex->Unlock();
  
  return p;
}



void PoolRegistry::HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {
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
      row.push_back(std::to_string(di.children.size()));
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

