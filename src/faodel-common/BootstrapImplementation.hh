// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_BOOTSTRAPINTERNAL_HH
#define FAODEL_COMMON_BOOTSTRAPINTERNAL_HH

#include <string>
#include <map>
#include <vector>

#include "faodel-common/Bootstrap.hh"

#include "faodel-common/Configuration.hh"
#include "faodel-common/LoggingInterface.hh"

namespace faodel {
namespace bootstrap {
namespace internal {


//Holds info on each component
typedef struct {
  std::string name;
  std::vector<std::string> requires;
  std::vector<std::string> optional;
  fn_init  init_function;
  fn_start start_function;
  fn_fini  fini_function;
  BootstrapInterface *optional_component_ptr;
} bstrap_t;


/**
 * @brief A class for registering how FAODEL components are started/stopped
 */
class Bootstrap
  : public LoggingInterface {
public:
  Bootstrap();
  ~Bootstrap() override;

  void SetNodeID(NodeID nodeid) { my_node_id = nodeid; }

  void RegisterComponent(std::string name,
                         std::vector<std::string> requires,
                         std::vector<std::string> optional,
                         fn_init  init_function,
                         fn_start start_function,
                         fn_fini  fini_function,
                         bool allow_overwrites);

  void RegisterComponent(BootstrapInterface *component, bool allow_overwrites);
  BootstrapInterface * GetComponentPointer(std::string name);

  bool CheckDependencies(std::string *info_message=nullptr);
  std::vector<std::string> GetStartupOrder();

  bool Init(const Configuration &config);
  void Start();
  void Start(const Configuration &config); //Init and Start
  void Finish(bool clear_list_of_bootstrap_users);

  std::string GetState() const;
  bool IsStarted() const { return state==State::STARTED; }
  Configuration GetConfiguration() const { return configuration; }

  int GetNumberOfUsers() const;
  bool HasComponent(const std::string &component_name) const;

  void dumpStatus() const;
  void dumpInfo(faodel::ReplyStream &rs) const;

private:

  void finish_(bool clear_list_of_bootstrap_users);

  Configuration configuration;
  bool show_config_at_init;
  bool halt_on_shutdown;
  bool status_on_shutdown;
  bool mpisyncstop_enabled;
  uint64_t sleep_seconds_before_shutdown;
  nodeid_t my_node_id;
  bool expandDependencies(std::map<std::string, std::set<std::string>> &dep_lut,
                          std::stringstream &emsg);

  bool sortDependencies(std::stringstream &emsg);

  std::vector<bstrap_t> bstraps;

  faodel::MutexWrapper *state_mutex; //!< Protects num_init_callers and State
  int num_init_callers; //!< Number of entities that have called init (or start)

  enum class State { UNINITIALIZED, INITIALIZED, STARTED };
  State state;

};



} // namespace internal
} // namespace bootstrap
} // namespace faodel

#endif // FAODEL_COMMON_BOOTSTRAPINTERNAL_HH
