// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "opbox/core/Singleton.hh"

#include "dirman/core/Singleton.hh"
#include "dirman/core/DirManCoreStatic.hh"
#include "dirman/core/DirManCoreCentralized.hh"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

using namespace std;

//Initialize the dirman singleton.
dirman::internal::SingletonImpl dirman::internal::Singleton::impl;


namespace dirman {
namespace internal {

SingletonImpl::SingletonImpl()
  : LoggingInterface("dirman"), dirman_service_none(false) {

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
  name = "dirman";
  requires = {"opbox"};
  optional = {"whookie", "mpisyncstart"};
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
  config.GetLowercaseString(&dirman_type, "dirman.type",  "centralized");


  dbg("About to create type "+dirman_type);


  if        (dirman_type == "none") {
    dirman_service_none = true;
    return;
  } else if (dirman_type == "static") {      core = new DirManCoreStatic(config);
  } else if (dirman_type == "centralized"){  core = new DirManCoreCentralized(config);
  } else {
    error("Unknown dirman.type '"+dirman_type+"'. Options are 'none', 'static', or 'centralized'");
    exit(-1);
  }

  whookie::Server::updateHook("/dirman", [this] (const map<string,string> &args, stringstream &results) {
      return core->HandleWhookieStatus(args, results);
    });
  whookie::Server::updateHook("/dirman/entry", [this] (const map<string,string> &args, stringstream &results) {
      return core->HandleWhookieEntry(args, results);
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
  dbg("Dirman ("+core->GetType()+") Starting State is "+core->str(4,2));
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
  whookie::Server::deregisterHook("/dirman");


  if(IsUnconfigured()){
    error("Attempted to finish OpBox that is unconfigured");
  } else {
    delete core;
    core = &unconfigured;
  }

}

} // namespace internal

/**
 * @brief Bootstrap function used to manually register dirman with bootstrap
 *
 * @retval "dirman"
 *
 * @note Users pass this to bootstrap's Start/Init. Only the last
 *       bootstap dependency needs to be supplied.
 */
std::string bootstrap() {

  //register dependencies
  opbox::bootstrap();
  faodel::bootstrap::RegisterComponent(&dirman::internal::Singleton::impl, true);

  return "dirman";
}

} // namespace dirman
