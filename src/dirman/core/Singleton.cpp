// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "opbox/core/Singleton.hh"

#include "opbox/services/dirman/core/Singleton.hh"
#include "opbox/services/dirman/core/DirManCoreCentralized.hh"
#include "opbox/services/dirman/core/DirManCoreDistributed.hh"

#include "webhook/WebHook.hh"
#include "webhook/Server.hh"

using namespace std;

//Initialize the singleton. Dirman now lives in opbox/core singleon
opbox::dirman::internal::SingletonImpl opbox::internal::Singleton::impl_dm;


namespace opbox {
namespace dirman {
namespace internal {

SingletonImpl::SingletonImpl()
  : LoggingInterface("dirman"), dirman_service_none(false) {

  //faodel::bootstrap::RegisterComponent(this);
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
  name = "directorymanager";
  requires = {"opbox"};
  optional = {"webhook"};
}



/**
 * @brief Bootstrap function for initializing OpBox (creates a new OpBoxCore singleton)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void SingletonImpl::Init(const faodel::Configuration &config){
  if(!IsUnconfigured()){
    error("Attempted to initialize DirMan multiple times");
    exit(-1);
  }

  ConfigureLogging(config);
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type",  "none");


  dbg("About to create type "+dirman_type);


  if        (dirman_type == "none")       {  dirman_service_none=true; return;
  } else if (dirman_type == "centralized"){  core = new DirManCoreCentralized(config);
  } else if (dirman_type == "distributed"){  core = new DirManCoreDistributed(config);
  } else {
    error("Unknown dirman.type '"+dirman_type+"'. Options are 'none', 'centralized', or 'distributed'");
    exit(-1);
  }

  webhook::Server::updateHook("/dirman", [this] (const map<string,string> &args, stringstream &results) {
      return core->HandleWebhookStatus(args, results);
    });
  webhook::Server::updateHook("/dirman/entry", [this] (const map<string,string> &args, stringstream &results) {
      return core->HandleWebhookEntry(args, results);
    });

}

/**
 * @brief Bootstrap function for starting the previously-configured DirMan core
 *
 */
void SingletonImpl::Start(){
  if(dirman_service_none) return; //Handle dirman.type==none
  if(IsUnconfigured()){
    error("Attempted to start an uninitialized DirMan");
    exit(-1);
  }
  core->start();
}

/**
 * @brief Bootstrap function for stopping opbox (stops/deallocates core and returns to unconfigured state)
 *
 */
void SingletonImpl::Finish() {
  if(dirman_service_none){
    dirman_service_none=false;
    return;
  }
  webhook::Server::deregisterHook("/dirman");

  if(IsUnconfigured()){
    error("Attempted to finish OpBox that is unconfigured");
  } else {
    delete core;
    core = &unconfigured;
  }



}

} // namespace internal
} // namespace dirman
} // namespace opbox
