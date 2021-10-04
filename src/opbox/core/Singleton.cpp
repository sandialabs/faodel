// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "faodel-services/BackBurner.hh"

#include "opbox/core/Singleton.hh"

#include "opbox/core/OpBoxCoreUnconfigured.hh"
#include "opbox/core/OpBoxCoreDeprecatedStandard.hh"
#include "opbox/core/OpBoxCoreThreaded.hh"


#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

#include "lunasa/Lunasa.hh"

using namespace std;

//Initialize the singleton
opbox::internal::SingletonImpl opbox::internal::Singleton::impl;


namespace opbox {
namespace internal {

SingletonImpl::SingletonImpl()
  : LoggingInterface("opbox") {

  core = &unconfigured; //Start in unconfigured state
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
 * @brief Bootstrap function for registering OpBox's dependencies
 *
 * @param[out] name The name of this component (opbox)
 * @param[out] requires List of components that start before opbox
 * @param[out] optional List of optional components that start before opbox
 */
void SingletonImpl::GetBootstrapDependencies(
                       string &name,
                       vector<string> &requires,
                       vector<string> &optional) const {
  name = "opbox";
  requires = {"backburner", "lunasa"};
  optional = {"whookie"};
}



/**
 * @brief Bootstrap function for initializing OpBox (creates a new OpBoxCore singleton)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void SingletonImpl::Init(const faodel::Configuration &config){
  if(!IsUnconfigured()){
    error("Attempted to initialize OpBox multiple times");
    exit(-1);
  }

  ConfigureLogging(config);
  string opbox_type;
  config.GetLowercaseString(&opbox_type, "opbox.type",  "threaded");


  dbg("About to create type "+opbox_type);
  if(opbox_type == "threaded"){
    core = new OpBoxCoreThreaded();
  } else if(opbox_type == "standard"){
    core = new OpBoxCoreDeprecatedStandard();
  } else {
    error("Unknown opbox.type '"+opbox_type+"' in configuration. Choices: threaded, standard");
    exit(-1);
  }


  //Unconfigured can exist before bootstrap, so we need to make sure we don't
  //register any hooks in unconfigured before whookie has been Init'd.
  whookie::Server::updateHook("/opbox/opregistry", [this] (const map<string,string> &args, stringstream &results) {
      return registry.HandleWhookieStatus(args, results);
    });

  core->init(config);

}

/**
 * @brief Bootstrap function for starting the previously-configured OpBox core
 *

 */
void SingletonImpl::Start(){
  if(IsUnconfigured()){
    error("Attempted to start an uninitialized OpBox");
    exit(-1);
  }
  registry.Start(); //Close registry. Can still register, but those values will go through locks
  core->start();
}

/**
 * @brief Bootstrap function for stopping opbox (stops/deallocates core and returns to unconfigured state)
 *
 */
void SingletonImpl::Finish() {

  if(IsUnconfigured()){
    error("Attempted to finish OpBox that is unconfigured");
  } else {
    delete core;
    core = &unconfigured;
  }
  registry.Finish();

}

} // namespace internal

/**
 * @brief Bootstrap function used to manually register opbox (and
 *        dependencies) with bootstrap
 *
 * @retval "opbox"
 *
 * @note Users pass this to bootstrap's Start/Init. Only the last
 *       bootstap dependency needs to be supplied.
 */
std::string bootstrap(){

  //register dependencies
  faodel::backburner::bootstrap();
  lunasa::bootstrap();

  //register ourselves
  faodel::bootstrap::RegisterComponent(&opbox::internal::Singleton::impl, true); //opbox

  return "opbox";
}


} // namespace opbox
