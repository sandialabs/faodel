// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "kelpie/core/Singleton.hh"

#include "kelpie/core/KelpieCoreNoNet.hh"
#include "kelpie/core/KelpieCoreStandard.hh"
#include "kelpie/common/KelpieInternal.hh"

#include "kelpie/pools/UnconfiguredPool/UnconfiguredPool.hh"

#include "opbox/OpBox.hh"

using namespace std;

//Initialize the singleton
kelpie::internal::SingletonImpl kelpie::internal::Singleton::impl;


namespace kelpie {
namespace internal {

SingletonImpl::SingletonImpl()
  : LoggingInterface("kelpie") {

  //faodel::bootstrap::RegisterComponent(this);
  core = &unconfigured;  //Start in unconfigured state
  unconfigured_pool = make_shared<UnconfiguredPool>(); //Create a default pool
}

SingletonImpl::~SingletonImpl(){
  //cleanup: user or bootstrap should have called finish by now, but
  //         maybe not. stop if we have.
  if(!IsUnconfigured()){
    delete core;
  }
}
bool SingletonImpl::IsUnconfigured(){
  return (core==&unconfigured);
}


/**
 * @brief Bootstrap function for registering Kelpie's dependencies
 *
 * @param[out] name The name of this component (kelpie)
 * @param[out] requires List of components that start before kelpie
 * @param[out] optional List of optional components that start before kelpie
 */
void SingletonImpl::GetBootstrapDependencies(
                       string &name,
                       vector<string> &requires,
                       vector<string> &optional) const {
  name = "kelpie";
  requires = {"opbox"};
  optional = {"webhook"};
}



/**
 * @brief Bootstrap function for initializing Kelpie (creates a new KelpieCore singleton)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void SingletonImpl::Init(const faodel::Configuration &config){
  if(!IsUnconfigured()){
    error("Attempted to initialize Kelpie multiple times");
    exit(-1);
  }

  ConfigureLogging(config);
  string kelpie_type;
  config.GetLowercaseString(&kelpie_type, "kelpie.type",  "standard");


  dbg("About to create type "+kelpie_type);
  if(kelpie_type == "standard"){
    core = new KelpieCoreStandard();

  } else if (kelpie_type == "nonet"){
    core = new KelpieCoreNoNet();

  } else {
    error("Unknown kelpie.type '"+kelpie_type+"' in configuration. Choices: standard");
    exit(-1);
  }

  faodel::bucket_t bucket;
  config.GetDefaultSecurityBucket(&bucket);
  core->init(config);

}

/**
 * @brief Bootstrap function for starting the previously-configured Kelpie core
 *

 */
void SingletonImpl::Start(){
  if(IsUnconfigured()){
    error("Attempted to start an uninitialized Kelpie");
    exit(-1);
  }
  
  core->start();
}

/**
 * @brief Bootstrap function for stopping kelpie (stops/deallocates core and returns to unconfigured state)
 *
 */
void SingletonImpl::Finish() {

  if(IsUnconfigured()){
    error("Attempted to finish Kelpie that is unconfigured");
  } else {
    delete core;
    core = &unconfigured;
  }

}

}  // namespace internal

/**
 * @brief Bootstrap function used to manually register kelpie (and
 *        dependencies) with bootstrap
 *
 * @retval "kelpie"
 *
 * @note Users pass this to bootstrap's Start/Init. Only the last
 *       bootstap dependenciy needs to be supplied.
 */
std::string bootstrap(){

  //register dependencies. Should only need to do opbox and backburner
  opbox::bootstrap();
  faodel::backburner::bootstrap();

  //register ourselves
  faodel::bootstrap::RegisterComponent(&kelpie::internal::Singleton::impl, true);  //kelpie

  return "kelpie";
}


}  // namespace kelpie
