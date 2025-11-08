// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>

#include <whookie/Server.hh>

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "faodel-common/BootstrapImplementation.hh"

using namespace std;

//Implementations
namespace faodel {
namespace bootstrap {
namespace internal {

Bootstrap::Bootstrap()
        : LoggingInterface("bootstrap"),
          halt_on_shutdown(false),
          status_on_shutdown(false),
          mpisyncstop_enabled(false),
          sleep_seconds_before_shutdown(0),
          my_node_id(NODE_UNSPECIFIED),
          num_init_callers(0),
          state(State::UNINITIALIZED) {
  state_mutex = GenerateMutexByTypeID(MutexWrapperTypeID::PTHREADS_LOCK);
}

Bootstrap::~Bootstrap() {

  state_mutex->Lock();
  num_init_callers=1; //ensure we actually shutdown
  if(state == State::INITIALIZED) { warn("Bootstrap was initialized but never started"); }
  if(state == State::STARTED) finish_(true);
  delete state_mutex;
}

/**
 * @brief Register a new component for Bootstrap to manage in setup/teardown
 * @param[in] name A string name for the component
 * @param[in] requires A vector of other components that come before this item
 * @param[in] optional A vector of any optional components that come before this item
 * @param[in] init_function The function to call when Bootstrap handed an Init()
 * @param[in] start_function The function to call when Bootstrap handed a Start
 * @param[in] fini_function The function to call when Bootstrap handed a Finish()
 * @param[in] allow_overwrites Overwrite any previous settings for a registered op
 * @throw runtime_exception If bootstrap is not in unintialized state or component exists and not allow_overwrites
 */
void Bootstrap::RegisterComponent(string name,
                                  vector <string> requires,
                                  vector <string> optional,
                                  fn_init init_function,
                                  fn_start start_function,
                                  fn_fini fini_function,
                                  bool allow_overwrites) {

  state_mutex->Lock();

  //dbg("RegisteringComponent "+name);

  //See if someone is trying to register a component after bootstrap has already started. This is ok
  //if the component was registered the first time around (eg, when users hide a bootstrap init of
  //kelpie in their ctors). We have to fail though if they didn't register this component.
  if(state != State::UNINITIALIZED) {
    bool already_exists = HasComponent(name);
    state_mutex->Unlock();
    if(!already_exists) {
      throw std::runtime_error("Bootstrap RegisterComponent: Register of " + name + " called after init");
    }
    return;
  }

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
      if(!allow_overwrites) {
        state_mutex->Unlock();
        throw std::runtime_error("Bootstrap RegisterComponent Attempted to register " + name + " multiple times");
      }
      tmp_bs = bs;
      return;
    }
  }

  bstraps.push_back(bs);
  state_mutex->Unlock();
}

/**
 * @brief Register a component that has implements BootstrapInterface
 * @param[in] component A component that implements the interface
 * @param[in] allow_overwrites Overwrite any previous settings for a registered op
 * @throw runtime_error If bootstrap is not in unintialized state or component exists and not allow_overwrites
 */
void Bootstrap::RegisterComponent(BootstrapInterface *component, bool allow_overwrites) {
  string name;
  vector <string> requires;
  vector <string> optional;
  component->GetBootstrapDependencies(name, requires, optional);

  state_mutex->Lock();

  //dbg("RegisteringComponent "+name);

  //See if someone is trying to register a component after bootstrap has already started. This is ok
  //if the component was registered the first time around (eg, when users hide a bootstrap init of
  //kelpie in their ctors). We have to fail though if they didn't register this component.
  if(state != State::UNINITIALIZED) {
    bool already_exists = HasComponent(name);
    state_mutex->Unlock();
    if(!already_exists) {
      throw std::runtime_error("Bootstrap RegisterComponent: Register of " + name + " called after init");
    }
    return;
  }

  //Normal registration
  bstrap_t bs;
  bs.name = name;
  bs.requires = requires;
  bs.optional = optional;
  bs.init_function = [component](Configuration *config) { component->InitAndModifyConfiguration(config); };
  bs.start_function = [component]() { component->Start(); };
  bs.fini_function = [component]() { component->Finish(); };
  bs.optional_component_ptr = component;

  for(auto &tmp_bs : bstraps) {
    if(tmp_bs.name == name) {
      if(!allow_overwrites) {
        state_mutex->Unlock();
        throw std::runtime_error("Bootstrap RegisterComponent: Attempted to register " + name + " multiple times");
      }
      tmp_bs = bs;
      state_mutex->Unlock();
      return;
    }
  }
  bstraps.push_back(bs);

  state_mutex->Unlock();

}

/**
 * @brief Initialize all the pre-registered bootstrap components with a given config
 * @param config The configuration to pass each component
 * @retval TRUE We initialized using this call (nobody else initialized before us)
 * @retval FALSE Someone else had already initialized, so we used those settings
 * @note The Configuration may be modified in Init to fill in runtime values
 */
bool Bootstrap::Init(const Configuration &config) {

  bool exit_on_errors; //When true, we catch exceptions and then call exit

  state_mutex->Lock();

  //Only proceed if this is the first initialization. Otherwise use previous initialization
  int current_count = num_init_callers++;
  if(current_count != 0) {
    warn("Multiple bootstrap Init's called. Using existing initialization.");
    state_mutex->Unlock();
    return false;
  }


  //We need to load any updates from references
  configuration = config;
  configuration.AppendFromReferences();

  //Now we can update bootstrap's logging
  ConfigureLogging(configuration);

  bool mpisyncstart_enabled=false;
  configuration.GetBool(&show_config_at_init,           "bootstrap.show_config",         "false");
  configuration.GetBool(&exit_on_errors,                "bootstrap.exit_on_errors",      "true");
  configuration.GetBool(&halt_on_shutdown,              "bootstrap.halt_on_shutdown",    "false");
  configuration.GetBool(&status_on_shutdown,            "bootstrap.status_on_shutdown",  "false");
  configuration.GetUInt(&sleep_seconds_before_shutdown, "bootstrap.sleep_seconds_before_shutdown", "0");
  configuration.GetBool(&mpisyncstart_enabled,          "mpisyncstart.enable",           "false");
  configuration.GetBool(&mpisyncstop_enabled,           "mpisyncstop.enable",            "false");

  dbg("Init (" + std::to_string(bstraps.size()) + " bootstraps known)");

  #ifndef Faodel_ENABLE_MPI_SUPPORT
    if(mpisyncstart_enabled) {
      warn("The mpisyncstart option was enabled, but Faodel was not built with MPI support. Ignoring.");
    }
    if(mpisyncstop_enabled) {
      warn("The mpisyncstop option was enabled, but Faodel was not built with MPI support. Ignoring.");
    }
  #else
    //cout<<"MPISYNCSTART enabled: "<<mpisyncstart_enabled<<endl;
    if((mpisyncstart_enabled) && (!HasComponent("mpisyncstart"))) {
      error("Configuration has mpisyncstart.enable, but mpisyncstart bootstrap not registered. Ignoring.");
    }
  #endif


  string info_message;
  bool ok = CheckDependencies(&info_message); //Sorts bootstraps
  if(!ok) {
    state_mutex->Unlock();
    throw std::runtime_error("Bootstrap Init: Dependency error " + info_message);
  }

  //Execute each bootstrap
  try {
    for(auto &bs : bstraps) {
      dbg("Initializing service " + bs.name);
      bs.init_function(&configuration);
    }
  } catch(const runtime_error &e) {
    state_mutex->Unlock();
    if(exit_on_errors) {
      cerr <<"Bootstrap Init Error: "<<string(e.what())<<endl;
      exit(-1);
    } else {
      throw(e);
    }
    throw;
  }

  if(show_config_at_init) {
    cout <<"Faodel Configuration after Bootstrap::Init is:\n" << configuration.str();
  }

  //Note: we can't install bootstrap whookie here because of build order issues. Do it in whookie

  dbg("Completed Initializing services. Moved to 'initialized' state.");
  state = State::INITIALIZED;
  state_mutex->Unlock();
  return true;
}

/**
 * @brief Step through the initialized bootstrap components and then call Start
 * @throw runtime_error if bootstrap not in Initialized state
 */
void Bootstrap::Start() {

  state_mutex->Lock();
  if(state == State::STARTED) {
    dbg("Already in started state. Continuing.");
    state_mutex->Unlock();
    return;
  }

  if(state != State::INITIALIZED) {
    state_mutex->Unlock();
    throw std::runtime_error(
            "Bootstrap Start: Attempted to Start() when not in the Initialized state. Call Init, Start, Finish\n"
            "(Current State is " + GetState() + ")");
  }

  dbg("Starting all services");
  for(auto &bs : bstraps) {
    dbg("Starting service " + bs.name);
    bs.start_function();
  }
  dbg("Completed Starting services. Moved to 'started' state.");
  state = State::STARTED;
  state_mutex->Unlock();
}

/**
 * @brief Perform Init on all components, then perform Start on all components
 * @param[in] config The configuration to pass all components
 * @note If bootstrap is already started, it will reuse it. This may cause the config passed in to be ignored
 */
void Bootstrap::Start(const Configuration &config) {
  bool we_initialized = Init(config);
  if(we_initialized) Start();
}

/**
 * @brief Get the number of times bootstrap Init was called
 * @return Current count
 * Sometimes different applications embed bootstrap start in their code to enable servces
 * to start automatically. This function tells you how many times Init was called.
 */
int Bootstrap::GetNumberOfUsers() const {
  state_mutex->Lock();
  int count = num_init_callers;
  state_mutex->Unlock();
  return count;
}

/**
 * @brief Determine if a bootstrap component (eg kelpie) has been registered yet
 * @param component_name Name of the component to locate
 * @retval TRUE This component is known
 * @retval FALSE This component has not been registered yet
 */
bool Bootstrap::HasComponent(const string &component_name) const {
  for(auto bs : bstraps) {
    if(bs.name == component_name) return true;
  }
  return false;
}

/**
 * @brief Dump out a text message about the node's status
 * @note Currenly only supports OK
 */
void Bootstrap::dumpStatus() const {

  cout << "\n Node Url: " << my_node_id.GetHttpLink() << endl;

  cout << "\n"
       << "          888\n"
       << "          888\n"
       << "          888\n"
       << "   .d88b. 888  888\n"
       << "  d88""88b888 .88P\n"
       << "  888  888888888K\n"
       << "  Y88..88P888 \"88b\n"
       << "   \"Y88P\" 888  888\n"
       << "\n";

}
void Bootstrap::dumpInfo(ReplyStream &rs) const {
  rs.tableBegin("Bootstrap Settings");
  rs.tableTop({"Parameter", "Value"});
  rs.tableRow({"Current State", GetState() });
  rs.tableRow({"MPISyncStop Enabled on Shutdown", std::to_string(mpisyncstop_enabled)});
  rs.tableRow({"Status on Shutdown", std::to_string(status_on_shutdown)});
  rs.tableRow({"Halt on Shutdown", std::to_string(halt_on_shutdown) });
  rs.tableRow({"Sleep Seconds Before Shutdown", std::to_string(sleep_seconds_before_shutdown)});
  rs.tableEnd();

  rs.mkList(faodel::bootstrap::GetStartupOrder(), "Bootstrap Startup Order" );

  rs.Finish();
}

/**
 * @brief Shutdown all bootstraps in reverse order
 * @param[in] clear_list_of_bootstrap_users When true, erase all known bootstraps for a clean restart
 * @throw runtime_error If bootstrap not in UNINITIALIZED state
 */
void Bootstrap::Finish(bool clear_list_of_bootstrap_users) {

  state_mutex->Lock();
  finish_(clear_list_of_bootstrap_users);
  state_mutex->Unlock();

}

void Bootstrap::finish_(bool clear_list_of_bootstrap_users) {

  if(state == State::UNINITIALIZED) {
    state_mutex->Unlock();
    throw std::runtime_error("Bootstrap Finish: Attempted to Finish when not Init state. Currently: " + GetState());
  }

  //dbg("Num init callers is "+std::to_string(num_init_callers)+" state is "+GetState());
  //Check number of initializers
  num_init_callers--;

  if(num_init_callers>0) {
    dbg("Received finish, but other entities started bootstrap. Waiting for their finish.");
    return;
  }

  #ifdef Faodel_ENABLE_MPI_SUPPORT
    //Halt until everyone has had a chance to shutdown. This should catch any late senders
    if(mpisyncstop_enabled) {
      dbg("Performing mpisyncstop");
      MPI_Barrier(MPI_COMM_WORLD);
    }
  #endif


  dbg("Finish (" + std::to_string(bstraps.size()) + " bootstraps known)");
  if(halt_on_shutdown) {
    if(status_on_shutdown) dumpStatus();
    F_HALT("Bootstrap finish called with Halt on Shutdown activated");
  }

  //Some apps need a few seconds before they shutdown. Here's a nice place to do it
  if(sleep_seconds_before_shutdown > 0) {
    info("Finished. Sleeping for " + std::to_string(sleep_seconds_before_shutdown) + " seconds before shutting down");
    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds_before_shutdown));
  }

  if(state == State::STARTED) {
    reverse(bstraps.begin(), bstraps.end());
    for(auto &bs : bstraps) {
      dbg("Finishing service " + bs.name);
      bs.fini_function();
    }
  }
  state = State::UNINITIALIZED;

  //Testing may set this to false so it can setup/teardown a stack built from init-time bs's
  if(clear_list_of_bootstrap_users)
    bstraps.clear(); //Get rid of all

  if(status_on_shutdown) {
    dumpStatus();
  }

}

/**
 * @brief Get a text representation of current state
 * @retval UNINITIALIZED Init() has not been called yet
 * @retval INITIALIZED Init() has been called, but not Start
 * @retval STARTED Start() has been called, but not Finish()
 */
string Bootstrap::GetState() const {
  switch(state) {
    case State::UNINITIALIZED:
      return "uninitialized";
    case State::INITIALIZED:
      return "initialized";
    case State::STARTED:
      return "started";
  }
  return "Unknown?";
}

/**
 * @brief Retrieve a pointer to the underlying, registered component
 * @param name The name of the component
 * @retval component Component was found
 * @retval nullptr Component not found
 */
BootstrapInterface *Bootstrap::GetComponentPointer(string name) {
  for(auto &bs : bstraps) {
    if(bs.name == name) {
      return bs.optional_component_ptr;
    }
  }
  return nullptr;
}

/**
 * @brief Expand the list of dependencies and convert them to a lookup table
 * @param dep_lut The returned dependency lookup table
 * @param emsg Any error message
 * @retval TRUE All resolved ok
 * @retval FALSE At least one problem with dependencies.
 */
bool Bootstrap::expandDependencies(map <string, set<string>> &dep_lut, stringstream &emsg) {

  //Step 1: create a list of available dependencies so we know what's available and toss optionals
  set <string> all_mandatory;
  for(auto &bs : bstraps) {
    all_mandatory.insert(bs.name);
  }

  //Step 2: create an initial lut of dependencies. Plug in all required and any available optionals
  for(auto &bs : bstraps) {
    set <string> deps;

    //Verify all requires exist
    for(auto &p : bs.requires) {
      if(all_mandatory.count(p) == 0) {
        emsg << "Bootstrap error: stage " << bs.name << " requires missing component " << p << endl;
        return false;
      }
    }
    deps.insert(bs.requires.begin(), bs.requires.end());

    //Add in any optionals
    for(auto &p :bs.optional) {
      if(all_mandatory.count(p) == 1) {
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
  map <string, set<string>> dep_lut;

  bool ok = expandDependencies(dep_lut, emsg);
  if(!ok) return false;


  //Step 3: keep cycling through the list until we have discovered all
  //        ancestors for each item. Ends when we don't have any updates
  bool keep_going = true;
  while(keep_going) {
    keep_going = false; //Assume we don't find any work

    for(auto &workitem_name_set : dep_lut) {
      set <string> workitem_updated = workitem_name_set.second;
      //Look at each of our parents
      for(auto &parent : workitem_name_set.second) {
        //See who the parent need and add them into our list
        set <string> parent_deps = dep_lut[parent];
        for(auto &d : parent_deps) {
          if(workitem_updated.count(d) == 0) {
            keep_going = true;
            workitem_updated.insert(d);
          }
        }
      }
      //Put updated set back in lut
      dep_lut[workitem_name_set.first] = workitem_updated;
    }
  }

  //Step 4: Create a sorted list
  vector <bstrap_t> sbs;
  for(auto &bs : bstraps) {
    bool found_spot = false;
    for(size_t i = 0; (!found_spot) && (i < sbs.size()); i++) {
      string name = sbs[i].name;
      if(dep_lut[sbs[i].name].count(bs.name)) {
        sbs.insert(sbs.begin() + i, bs);
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
      ss << "Bootstrap order:\n";
      for(auto &bs : bstraps)
        ss << bs.name << endl;
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
vector <string> Bootstrap::GetStartupOrder() {

  stringstream ss;
  bool ok = sortDependencies(ss);
  if(!ok) throw std::runtime_error("Bootstrap GetStartupOrder: Could not sort dependencies. " + ss.str());

  vector <string> names;
  for(auto &bs : bstraps)
    names.push_back(bs.name);
  return names;
}


} // namespace internal
} // namespace bootstrap
} // namespace faodel
