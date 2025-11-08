// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "lunasa/core/Singleton.hh"

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/core/LunasaCoreUnconfigured.hh"
#include "lunasa/core/LunasaCoreSplit.hh"


#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

#include "lunasa/Lunasa.hh"

using namespace std;

//Initialize the singleton
lunasa::internal::SingletonImpl lunasa::internal::Singleton::impl;


namespace lunasa {
namespace internal {

SingletonImpl::SingletonImpl()
  : LoggingInterface("lunasa","Singleton"),
    pin_functions_registered(false),
    used_tcmalloc_before(false) {
  
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
 * @brief Bootstrap function for registering Lunasa's dependencies
 *
 * @param[out] name The name of this component (lunasa)
 * @param[out] requires List of components that start before lunasa
 * @param[out] optional List of optional components that start before lunasa
 */
void SingletonImpl::GetBootstrapDependencies(
                       string &name,
                       vector<string> &requires,
                       vector<string> &optional) const {

  name = "lunasa";
  requires = {};
  optional = {"whookie", "mpisyncstart"};
}



/**
 * @brief Bootstrap function for initializing Lunasa (creates a new LunasaCore singleton)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void SingletonImpl::Init(const faodel::Configuration &config){
  if(!IsUnconfigured()){
    fatal("Attempted to initialize Lunasa multiple times");
  }

  ConfigureLogging(config);

  string lmm_name, emm_name, lunasa_type;
  config.GetLowercaseString(&lmm_name,    "lunasa.lazy_memory_manager",  "malloc");
  config.GetLowercaseString(&emm_name,    "lunasa.eager_memory_manager", "tcmalloc");
  config.GetLowercaseString(&lunasa_type, "lunasa.type", "split");

  //Check to make sure the user hasn't started/finished lunasa multiple times
  //in one run while using tcmalloc.
  bool requested_tcmalloc = ((lmm_name=="tcmalloc") || (emm_name=="tcmalloc"));

  dbg("Creating type "+lunasa_type);

  if(requested_tcmalloc){
    if(used_tcmalloc_before) {
      stringstream ss;
      fatal("Lunasa Init'd multiple times, using tcmalloc\n"
            "       tcmalloc does not truly release its memory when terminated, which \n"
            "       makes it impossible to cleanly stop Lunasa and then restart it in\n"
            "       the same application. Restarting is usually only necessary in test\n"
            "       programs. For these applications, we require the user to use the\n"
            "       malloc memory manager instead of tcmalloc. Users can switch to malloc\n"
            "       by adding the following to their configuration:\n"
            "              lunasa.lazy_memory_manager malloc\n"
            "              lunasa.eager_memory_manager malloc\n");
    }
    //remember for second time through
    used_tcmalloc_before=true;

  }
  
  if(lunasa_type == "split"){
    core = new LunasaCoreSplit();
  } else {
    fatal("Unknown lunasa.type '"+lunasa_type+"'");
  }

  core->init(config);

  //May not be necessary, but pass any pin/unpin functions in
  if(pin_functions_registered){
    core->RegisterPinUnpin(registered_pin_function, registered_unpin_function);
  }

  whookie::Server::updateHook("/lunasa/dataobject_type_registry",
                              [this] (const map<string,string> &args, stringstream &results) {
    faodel::ReplyStream rs(args, "Lunasa DataObject Type Registry", &results);
    dataobject_type_registry.DumpRegistryStatus(rs);
    rs.Finish();
    return;
    });
}


/**
 * @brief Bootstrap function for starting the previously-configured Lunasa core
 *
 */
void SingletonImpl::Start(){
  if(IsUnconfigured()){
    fatal("Attempted to start an uninitialized Lunasa");
  }
  core->start();
}

/**
 * @brief Bootstrap function for stopping lunasa (stops/deallocates core and returns to unconfigured state)
 *
 */
void SingletonImpl::Finish() {

  if(IsUnconfigured()){
    error("Attempted to finish Lunasa that is unconfigured");
  } else {

    whookie::Server::deregisterHook("/lunasa/dataobject_type_registry");

    //Remove core and reset to unconfigured state
    delete core;
    core = &unconfigured;
    //Should we mark pin_functions_registered to false?
  }    
}


/**
 * @brief Pass Lunasa functions for registering memory with the network
 *
 * @param[in] pin Function that registers a block of memory for network use (ie pin)
 * @param[in] unpin Function that unregisters a block of memory for network use (ie unpin)
 *
 * @note: the Lunasa singleton will pass these functions on to a running Lunasa core 
 * (if it exists) and cache them for when a Lunasa Core is Init'd.
 */
void SingletonImpl::RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) {
  registered_pin_function = pin;
  registered_unpin_function = unpin;
  pin_functions_registered = true;
  
  if(!IsUnconfigured()){
    core->RegisterPinUnpin(pin,unpin);
  }
}


} // namespace internal

/**
 * @brief Bootstrap function used to manually register lunasa (and 
 *        dependencies) with bootstrap
 *
 * @retval "lunasa"
 *
 * @note Users pass this to bootstrap's Start/Init. Only the last 
 *       bootstap dependency needs to be supplied.
 */
std::string bootstrap(){

  //register dependencies. Should only need to do whookie
  whookie::bootstrap();

  //register ourselves
  faodel::bootstrap::RegisterComponent(&lunasa::internal::Singleton::impl, true); //lunasa
  
  return "lunasa";
}


} // namespace lunasa
