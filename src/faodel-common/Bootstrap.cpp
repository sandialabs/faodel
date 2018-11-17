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

#include "common/Common.hh"
#include "common/BootstrapInternal.hh"
#include "common/Debug.hh"
#include "common/NodeID.hh"

using namespace std;

namespace faodel {
namespace bootstrap {

//Global but hidden
namespace internal {
Bootstrap *bootstrap = nullptr;

/**
 * @brief A failsafe class that removes bootstrap at exit if they exist
 */
class BootstrapCleanup {
public:
  ~BootstrapCleanup() {
    if(bootstrap!=nullptr) {
      delete bootstrap;
      bootstrap=nullptr;
    }
  }
};

BootstrapCleanup bsc; //Global, triggers delete

} // namespace internal



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
 * @throws BootstrapError when allow_overwrites is false and the name already has a bootstrap registered for it
 */
void RegisterComponent(string name,
                       vector<string> requires,
                       vector<string> optional,
                       fn_init  init_function,
                       fn_start start_function,
                       fn_fini  fini_function,
                       bool allow_overwrites) {

  //First time called, allocate the bootstrap. Note, we're doing
  //this because c++ can't guarantee ctor ordering between files
  if(internal::bootstrap == nullptr) {
    internal::bootstrap = new internal::Bootstrap();
  }

  return internal::bootstrap->RegisterComponent(name, requires, optional,
                                                init_function, start_function, fini_function,
                                                allow_overwrites);
}

/**
 * @brief Register a class instance that has the BootstrapInterface
 *
 * @param[in] component The class instance to be registered
 * @param[in] allow_overwrites When false, throw an exception if a bootstrap already exists (otherwise overwrite)
 *
 * @throws BootstrapError when allow_overwrites is false and the component is already registered
 */
void RegisterComponent(BootstrapInterface *component, bool allow_overwrites) {

  //First time called, allocate the bootstrap. Note, we're doing
  //this because c++ can't guarantee ctor ordering between files
  if(internal::bootstrap == nullptr) {
    internal::bootstrap = new internal::Bootstrap();
  }

  return internal::bootstrap->RegisterComponent(component, allow_overwrites);
}



/**
 * @brief Get information back about what bootstraps are currently registered
 *
 * @param[out] info_message Info about the bootstraps
 * @return ok True if there aren't any dependency problems
 */
bool CheckDependencies(string *info_message) {
  kassert(internal::bootstrap!=nullptr, "CheckDependencies called before any Bootstrap components were registered");
  return internal::bootstrap->CheckDependencies(info_message);
}

/**
 * @brief Lookup a bootstrap component class
 *
 * @param[in] name Name of the component to retrieve
 * @retval Component A pointer to the component
 * @retval nullptr No bootstrap is known for this name
 */
BootstrapInterface * GetComponentPointer(string name) {
  kassert(internal::bootstrap!=nullptr, "GetComponentPointer called before any Bootstrap components were registered");
  return internal::bootstrap->GetComponentPointer(name);
}

/**
 * @brief Get a list of all the bootstrap names, in the order they will be executed
 *
 * @return names A vector of names, sorted in the order of startup
 */
vector<string> GetStartupOrder() {
  kassert(internal::bootstrap!=nullptr, "GetStartupOrder called before any Bootstrap components were registered");
  return internal::bootstrap->GetStartupOrder();
}

void SetNodeID(NodeID nodeid) {
  kassert(internal::bootstrap!=nullptr, "SetNodeID called before any Bootstrap components were registered");
  internal::bootstrap->SetNodeID(nodeid);
}

/**
 * @brief Issue an Init to all known bootstraps, in the correct order
 *
 * @param[in] config The configuration to pass each bootstrap
 * @param[in] last_component A function that registers itself and all other components
 */
void Init(const Configuration &config, fn_register last_component) {

  string last_component_name = last_component(); //Calls registration

  kassert(internal::bootstrap!=nullptr, "Init called before any Bootstrap components were registered");
  return internal::bootstrap->Init(config);
}

/**
 * @brief Perform a Start on all bootstraps. Must be preceeded by an Init(config)
 *
 * @throws BootstrapError if bootstrap not in initialized state
 */
void Start() {
  kassert(internal::bootstrap!=nullptr, "Start called before any Bootstrap components were registered");
  return internal::bootstrap->Start();
}

/**
 * @brief Perform an Init(config,last_component) and Start() on all bootstraps.
 *
 * @param[in] config The configuration to pass each bootstrap
 * @param[in] last_component A function that registers itself and all other components
 *
 * @throws BootstrapError if bootstrap not in uninitialized state
 */
void Start(const Configuration &config, fn_register last_component) {

  string last_component_name = last_component(); //Calls registration

  kassert(internal::bootstrap!=nullptr, "Start(config) called before any Bootstrap components were registered");
  return internal::bootstrap->Start(config);
}

/**
 * @brief Issue a Finish() in reverse startup order, to shutdown services. Removes all known bootstraps.
 *
 * @throws BootstrapError if not in the initialized state
 */
void Finish() {
  if(internal::bootstrap==nullptr) throw BootstrapError("Finish", "No bootstraps were registered");
  delete internal::bootstrap;
  internal::bootstrap = nullptr;
}


/**
 * @brief Issue a Finish() in reverse startup order to shutdown services. Does NOT remove bootstraps (for debugging)
 *
 * @throws BootstrapError if not in the initialized state
 */
void FinishSoft() {
  if(internal::bootstrap==nullptr) throw BootstrapError("FinishSoft", "No bootstraps were registered");
  internal::bootstrap->Finish(false);
}




//Implementations
namespace internal {

Bootstrap::Bootstrap()
  : LoggingInterface("bootstrap"),
    halt_on_shutdown(false),
    status_on_shutdown(false),
    sleep_seconds_before_shutdown(1),
    my_node_id(NODE_UNSPECIFIED),
    state(State::CREATED_BS) {
}

Bootstrap::~Bootstrap() {
  if(state==State::INITIALIZED) { warn("Bootstrap was initialized but never started"); }
  if(state==State::STARTED) Finish(true);
}

void Bootstrap::RegisterComponent(string name,
                             vector<string> requires,
                             vector<string> optional,
                             fn_init  init_function,
                             fn_start start_function,
                             fn_fini  fini_function,
                             bool allow_overwrites ) {

  if(state!=State::CREATED_BS) throw BootstrapError("RegsiterComponent","Register of "+name+" called after init");

  bstrap_t bs;
  bs.name = name;
  bs.requires = requires;
  bs.optional = optional;
  bs.init_function = init_function;
  bs.start_function = start_function;
  bs.fini_function = fini_function;
  bs.optional_component_ptr = nullptr; //User didn't provide

  for(auto &tmp_bs : bstraps) {
    if(tmp_bs.name == name) {
      if(!allow_overwrites) throw BootstrapError("RegsiterComponent","Attempted to register "+name+" multiple times");
      tmp_bs = bs;
      return;
    }
  }

  bstraps.push_back(bs);
}


void Bootstrap::RegisterComponent(BootstrapInterface *component, bool allow_overwrites) {
  string name;
  vector<string> requires;
  vector<string> optional;
  component->GetBootstrapDependencies(name, requires, optional);

  bstrap_t bs;
  bs.name = name;
  bs.requires = requires;
  bs.optional = optional;
  bs.init_function  = [component] (const Configuration &config) { component->Init(config); };
  bs.start_function = [component] () { component->Start(); };
  bs.fini_function  = [component] () { component->Finish(); };
  bs.optional_component_ptr = component;

  for(auto &tmp_bs : bstraps) {
    if(tmp_bs.name == name) {
      if(!allow_overwrites) throw BootstrapError("RegsiterComponent","Attempted to register "+name+" multiple times");
      tmp_bs = bs;
      return;
    }
  }
  bstraps.push_back(bs);
}

void Bootstrap::Init(const Configuration &config) {

  //We need to load any updates from references
  Configuration config2=config;
  config2.AppendFromReferences();

  //Now we can update bootstrap's logging
  ConfigureLogging(config2);

  config2.GetBool(&halt_on_shutdown,              "bootstrap.halt_on_shutdown",   "false");
  config2.GetBool(&status_on_shutdown,            "bootstrap.status_on_shutdown", "false");
  config2.GetUInt(&sleep_seconds_before_shutdown, "bootstrap.sleep_seconds_before_shutdown", "1");


  dbg("Init ("+std::to_string(bstraps.size())+" bootstraps known)");

  if(state!=State::CREATED_BS) throw BootstrapError("Init","Init called multiple times (it can only be called once)");

  string info_message;
  bool ok=CheckDependencies(&info_message); //Sorts them
  if(!ok) throw BootstrapError("Init","Dependency error "+info_message);

  //Execute each one
  dbg("Init'ing all services");
  for(auto &bs : bstraps) {
    dbg("Initing service "+bs.name);
    bs.init_function(config2);
  }
  dbg("Completed Init'ing services. Moving to Initialized state.");
  state=State::INITIALIZED;
}

void Bootstrap::Start() {

  if(state!=State::INITIALIZED)
    throw BootstrapError("Start","Attempted to Start() when not in the Initialized state. Call Init, Start, Finish\n"
                         "(Current State is "+GetState()+")");

  dbg("Starting all services");
  for(auto &bs : bstraps) {
    dbg("Starting service " + bs.name);
    bs.start_function();
  }
  dbg("Completed Starting services. Moving to Started state.");
  state=State::STARTED;
}

void Bootstrap::Start(const Configuration &config) {

  if(state!=State::CREATED_BS)
    throw BootstrapError("Start","Attempted to Start(config) when already initialized and/or started. Call Init, Start, Finish\n"
                         "(Current State is "+GetState()+")");
  Init(config);
  Start();
}

void Bootstrap::dumpStatus() const {

  cout <<"\n Node Url: "<<my_node_id.GetHttpLink()<<endl;

  cout <<"\n"
       <<"          888\n" 
       <<"          888\n" 
       <<"          888\n"
       <<"   .d88b. 888  888\n" 
       <<"  d88""88b888 .88P\n" 
       <<"  888  888888888K\n"  
       <<"  Y88..88P888 \"88b\n" 
       <<"   \"Y88P\" 888  888\n"
       <<"\n";

}

void Bootstrap::Finish(bool clear_list_of_bootstrap_users) {

  if(state==State::CREATED_BS) throw BootstrapError("Finish","Attempted to Finish when not Init state. Currently: "+GetState());

  dbg("Finish ("+std::to_string(bstraps.size())+" bootstraps known)");
  if(halt_on_shutdown) {
    if(status_on_shutdown) dumpStatus();
    KHALT("Bootstrap finish called with Halt on Shutdown activated");
  }

  //Some apps need a few seconds before they shutdown. Here's a nice place to do it 
  if(sleep_seconds_before_shutdown>0) {
    info("Finished. Sleeping for "+std::to_string(sleep_seconds_before_shutdown)+" seconds before shutting down");
    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds_before_shutdown));
  }

  if(state == State::STARTED) {
    reverse(bstraps.begin(), bstraps.end());
    for(auto &bs : bstraps) {
      dbg("Finishing service " + bs.name);
      bs.fini_function();
    }
  }
  state=State::CREATED_BS;

  //Testing may set this to false so it can setup/teardown a stack built from init-time bs's
  if(clear_list_of_bootstrap_users)
    bstraps.clear(); //Get rid of all

  if(status_on_shutdown) {
    dumpStatus();
  }
                 
}

string Bootstrap::GetState() {
  switch(state) {
  case State::CREATED_BS:  return "Created";
  case State::INITIALIZED: return "Initialized";
  case State::STARTED:     return "Started";
  }
  return "Unknown?";
}

BootstrapInterface * Bootstrap::GetComponentPointer(string name) {
  for(auto &bs : bstraps) {
    if(bs.name == name) {
      return bs.optional_component_ptr;
    }
  }
  return nullptr;
}

bool Bootstrap::expandDependencies(map<string, set<string>> &dep_lut, stringstream &emsg) {

  //Step 1: create a list of available dependencies so we know what's available and toss optionals
  set<string> all_mandatory;
  for(auto &bs : bstraps) {
    all_mandatory.insert(bs.name);
  }

  //Step 2: create an initial lut of dependencies. Plug in all required and any available optionals
  for(auto &bs : bstraps) {
    set<string> deps;

    //Verify all requires exist
    for(auto &p : bs.requires) {
      if( all_mandatory.count(p) == 0) {
        emsg <<"Bootstrap error: stage "<<bs.name<<" requires missing component "<<p<<endl;
        return false;
      }
    }
    deps.insert(bs.requires.begin(), bs.requires.end());

    //Add in any optionals
    for(auto &p :bs.optional) {
      if( all_mandatory.count(p) == 1 ) {
        deps.insert(p);
      }
    }

    //Store in lut
    dep_lut[bs.name] = deps;
  }

  return true;
}


/**
 * @brief Examine the dependency list and sort them
 *
 * @param[out] emsg Info about what went wrong
 * @return ok True if dependencies are ok
 */
bool Bootstrap::sortDependencies(stringstream &emsg) {

  //Use a dependency lookup table to figure out full dependencies for each item
  map<string, set<string>> dep_lut;

  bool ok = expandDependencies(dep_lut, emsg);
  if(!ok) return false;


  //Step 3: keep cycling through the list until we have discovered all
  //        ancestors for each item. Ends when we don't have any updates
  bool keep_going=true;
  while(keep_going) {
    keep_going = false; //Assume we don't find any work

    for(auto &workitem_name_set : dep_lut) {
      set<string> workitem_updated = workitem_name_set.second;
      //Look at each of our parents
      for(auto &parent : workitem_name_set.second ) {
        //See who the parent need and add them into our list
        set<string> parent_deps = dep_lut[parent];
        for(auto &d : parent_deps) {
          if(workitem_updated.count(d) == 0) {
            keep_going=true;
            workitem_updated.insert(d);
          }
        }
      }
      //Put updated set back in lut
      dep_lut[workitem_name_set.first] = workitem_updated;
    }
  }

  //Step 4: Create a sorted list
  vector<bstrap_t> sbs;
  for(auto &bs : bstraps) {
    bool found_spot=false;
    for(size_t i=0; (!found_spot) && (i<sbs.size()); i++) {
      string name = sbs[i].name;
      if( dep_lut[sbs[i].name].count(bs.name) ) {
        sbs.insert(sbs.begin()+i, bs);
        found_spot = true;
      }
    }
    if(!found_spot) //Nobody depended on us, add to the back
      sbs.push_back(bs);
  }

  //Replace with the sorted list
  bstraps = sbs;

  return true;
}

/**
 * @brief Examine bootstraps and see if any are missing
 *
 * @param[out] info_message Optional info message describing what went wrong
 * @retval true Dependencies are ok
 * @retval false Something's wrong (see info message)
 */
bool Bootstrap::CheckDependencies(string *info_message) {

  stringstream ss;
  bool ok = sortDependencies(ss);

  if(info_message != nullptr) {
    if(ok) {
      ss<<"Bootstrap order:\n";
      for(auto &bs : bstraps)
        ss<<bs.name<<endl;
    }
    *info_message = ss.str();
  }

  return ok;
}


/**
 * @brief Examine known bootstraps and determine order
 *
 * @retval names Vector of all the components, in order
 */
vector<string> Bootstrap::GetStartupOrder() {

  stringstream ss;
  bool ok = sortDependencies(ss);
  if(!ok) throw BootstrapError("GetStartupOrder","Could not sort dependencies. "+ss.str());

  vector<string> names;
  for(auto &bs : bstraps)
    names.push_back(bs.name);
  return names;
}

} // namespace internal

} // namespace bootstrap
} // namespace faodel
