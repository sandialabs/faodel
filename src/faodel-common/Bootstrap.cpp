// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <atomic>
#include <chrono>



#include "faodel-common/Common.hh"

#include "faodel-common/Debug.hh"
#include "faodel-common/NodeID.hh"
#include "faodel-common/BootstrapImplementation.hh"

using namespace std;

namespace faodel {
namespace bootstrap {

//Global but hidden
namespace internal {

class Singleton {
public:
  static faodel::bootstrap::internal::Bootstrap impl;
};

//The static in the singleton must be initialized
faodel::bootstrap::internal::Bootstrap faodel::bootstrap::internal::Singleton::impl;

} // namespace internal

//BS is a singleton. Use this macro to make the code easier to understand
#define BSCORE (faodel::bootstrap::internal::Singleton::impl)


/**
 * @brief Register a new bootstrap and declare its dependencies/functions
 *
 * @param[in] name Name of the component
 * @param[in] requires Other bootstraps that must start before this one
 * @param[in] optional Optional bootstraps that should start before this one if available
 * @param[in] init_function The function bootstrap should call during Init()
 * @param[in] start_function The function bootstrap should call during Start()
 * @param[in] fini_function The function bootstrap should call during Finish()
 * @param[in] allow_overwrites When false, throw an exception if a bootstrap already exists (otherwise overwrite)
 *
 * @throws runtime_exception when allow_overwrites is false and the name already has a bootstrap registered for it
 */
void RegisterComponent(const string &name,
                       vector<string> requires,
                       vector<string> optional,
                       fn_init init_function,
                       fn_start start_function,
                       fn_fini fini_function,
                       bool allow_overwrites) {

  return BSCORE.RegisterComponent(name, requires, optional,
                                  init_function, start_function, fini_function,
                                  allow_overwrites);

}

/**
 * @brief Register a class instance that has the BootstrapInterface
 *
 * @param[in] component The class instance to be registered
 * @param[in] allow_overwrites When false, throw an exception if a bootstrap already exists (otherwise overwrite)
 *
 * @throws runtime_error when allow_overwrites is false and the component is already registered
 */
void RegisterComponent(BootstrapInterface *component, bool allow_overwrites) {

  return BSCORE.RegisterComponent(component, allow_overwrites);
}

/**
 * @brief Get information back about what bootstraps are currently registered
 *
 * @param[out] info_message Info about the bootstraps
 * @return ok True if there aren't any dependency problems
 */
bool CheckDependencies(string *info_message) {
  return BSCORE.CheckDependencies(info_message);
}

/**
 * @brief Lookup a bootstrap component class
 *
 * @param[in] name Name of the component to retrieve
 * @retval Component A pointer to the component
 * @retval nullptr No bootstrap is known for this name
 */
BootstrapInterface * GetComponentPointer(string name) {
  return BSCORE.GetComponentPointer(name);
}

/**
 * @brief Get a list of all the bootstrap names, in the order they will be executed
 *
 * @return names A vector of names, sorted in the order of startup
 */
vector<string> GetStartupOrder() {
  return BSCORE.GetStartupOrder();
}

/**
 * @brief Call for whookie to set our nodeid
 *
 * @param[in] iuo A marker signifying that this is only for internal use
 * @param[in] nodeid The whookie nodeid for this node
 * @note This function is intended for internal use only.
 */
void setNodeID(internal_use_only_t iuo, NodeID nodeid) {
  BSCORE.SetNodeID(nodeid);
}

/**
 * @brief Issue an Init to all known bootstraps, in the correct order
 *
 * @param[in] config The configuration to pass each bootstrap
 * @param[in] last_component A function that registers itself and all other components
 */
void Init(const Configuration &config, fn_register last_component) {

  string last_component_name = last_component(); //Calls registration
  return BSCORE.Init(config);
}

/**
 * @brief Perform a Start on all bootstraps. Must be preceeded by an Init(config)
 *
 * @throws runtime_error if bootstrap not in initialized state
 */
void Start() {
  return BSCORE.Start();
}

/**
 * @brief Perform an Init(config,last_component) and Start() on all bootstraps.
 *
 * @param[in] config The configuration to pass each bootstrap
 * @param[in] last_component A function that registers itself and all other components
 *
 * @throws runtime_error if bootstrap not in uninitialized state
 */
void Start(const Configuration &config, fn_register last_component) {

  string last_component_name = last_component(); //Calls registration
  return BSCORE.Start(config);
}

/**
 * @brief Issue a Finish() in reverse startup order, to shutdown services. Removes all known bootstraps.
 *
 * @throws runtime_error if not in the initialized state
 */
void Finish() {
  BSCORE.Finish(true);
}

/**
 * @brief Issue a Finish() in reverse startup order to shutdown services. Does NOT remove bootstraps (for debugging)
 *
 * @throws runtime_error if not in the initialized state
 */
void FinishSoft() {
  BSCORE.Finish(false);
}

/**
 * @brief Get the current state of bootstrap
 *
 * @retval "uninitialized" Bootstrap singleton hasn't been created yet
 * @retval "created" The Bootstrap module has been created, but an Init/Start has not been invoked
 * @retval "initialized" An Init has been called, but not a Start (or Finish)
 * @retval "started" A Start has been called (but no Finish)
 */
std::string GetState() {
  return BSCORE.GetState();
}

/**
 * @brief Determine whether bootstrap has already been started
 *
 * @retval TRUE Bootstrap has been Started
 * @retval FALSE Bootstrap has not been started
 */
bool IsStarted() {
  return BSCORE.IsStarted();
}

/**
 * @brief Get the Configuration that Bootstrap is using
 *
 * @retval Empty Configuration (when not initialized)
 * @retval Configuration handed to Init (possibly modified)
 */
Configuration GetConfiguration() {
  return BSCORE.GetConfiguration();
}

/**
 * @brief Whookie for dumping info about the bootstraps
 * @param rs ReplyStream to update with info
 */
void dumpInfo(faodel::ReplyStream &rs) {
  return BSCORE.dumpInfo(rs);
}

} // namespace bootstrap
} // namespace faodel
