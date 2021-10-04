// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_BOOTSTRAP_HH
#define FAODEL_COMMON_BOOTSTRAP_HH

#include <sstream>
#include <vector>
#include <set>
#include <functional>

#include "faodel-common/NodeID.hh"
#include "faodel-common/ReplyStream.hh"

//Forward References
namespace faodel { class Configuration; }
namespace faodel { namespace bootstrap { class BootstrapInterface; } }


namespace faodel {
namespace bootstrap {

using fn_init     = std::function<void (faodel::Configuration *config)>;
using fn_start    = std::function<void ()>;
using fn_fini     = std::function<void ()> ;
using fn_register = std::function<std::string ()>;


void RegisterComponent(const std::string &name,
                       std::vector<std::string> requires,
                       std::vector<std::string> optional,
                       fn_init init_function,
                       fn_start start_function,
                       fn_fini fini_function,
                       bool allow_overwrites = false);

void RegisterComponent(BootstrapInterface *component, bool allow_overwrites=false);
BootstrapInterface * GetComponentPointer(std::string name);


bool CheckDependencies(std::string *info_message=nullptr);
std::vector<std::string> GetStartupOrder();

void Init(const Configuration &config, fn_register last_component);
void Start();
void Start(const Configuration &config, fn_register last_component);
void Finish();     //Stop services, deleting all knowledge of bootstraps
void FinishSoft(); //Stop services, but don't clear out registration list

void setNodeID(internal_use_only_t iuo, NodeID nodeid);
std::string GetState();
bool IsStarted();
int GetNumberOfUsers();

Configuration GetConfiguration();

void dumpInfo(faodel::ReplyStream &rs);



} // namespace bootstrap
} // namespace faodel


#endif // FAODEL_COMMON_BOOTSTRAP_HH
