// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <stdexcept>

#include "faodel-common/Debug.hh"
#include "kelpie/common/ComputeRegistry.hh"
#include "whookie/Server.hh"

using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {


ComputeRegistry::ComputeRegistry()
  : LoggingInterface("kelpie.compute_registry"),
    started(false),
    default_compute_logging_level(0) {
}

ComputeRegistry::~ComputeRegistry() {
}

void ComputeRegistry::init(const faodel::Configuration &config) {

  ConfigureLogging(config); //Set registry's logging level
  default_compute_logging_level = LoggingInterface::GetLoggingLevelFromConfiguration(config, "kelpie.compute");

  RegisterComputeFunction("pick", fn_pick);


  whookie::Server::updateHook("/kelpie/compute_registry", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWhookieStatus(args, results);
    });

}
void ComputeRegistry::start(){
  started=true; //Prevent anyone from registering new pool types after this point
}

void ComputeRegistry::finish() {
  dbg("Finishing");
  whookie::Server::deregisterHook("/kelpie/compute_registry");

  compute_fns.clear();
  //NOTE: This may need a mutex+clear for safety
  started=false;
}

void ComputeRegistry::RegisterComputeFunction(const std::string &compute_function_name, fn_compute_t function_pointer){

  F_ASSERT(!started, "Attempted to register compute function after bootstrap Start().");

  dbg("Registering compute function "+compute_function_name);

  auto name_fn = compute_fns.find(compute_function_name);
  F_ASSERT(name_fn == compute_fns.end(), "Attempting to overwrite existing compute function for "+compute_function_name);
  compute_fns[compute_function_name]=function_pointer;
}

rc_t ComputeRegistry::doCompute( const std::string &compute_function_name, const std::string &args,
                                  faodel::bucket_t bucket, const Key &key,
                                  std::map<Key, lunasa::DataObject> ldos,
                                  lunasa::DataObject *ext_ldo) {

  auto name_fn = compute_fns.find(compute_function_name);
  if(name_fn == compute_fns.end()) return KELPIE_EINVAL;
  auto fn = name_fn->second;
  return fn(bucket, key, args, ldos, ext_ldo);

}

void ComputeRegistry::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {
    faodel::ReplyStream rs(args, "Kelpie Compute Function Registry", &results);

    vector<pair<string,string>> stats;

    //Just a one-column table with function names for now.. Maybe include pointers later?
    vector<vector<string>> compute_names;
    compute_names.push_back({"Registered Compute Function Names"});
    for(auto &name_fn : compute_fns){
      vector<string> row;
      row.push_back(name_fn.first);
      compute_names.push_back(row);
    }
    rs.mkTable(compute_names, "Compute Functions");
    rs.Finish();
}

/**
 * @brief Compute Function for selecting an output object from a list of keys based on a user constraint
 * @param[in] key The key that was used in the search (can have wildcard in column)
 * @param[in] args Additional pick command: first, last, largest, smallest
 * @param[in] ldos A map of the key/blobs that matched the key search
 * @param[out] ext_ldo The ldo to return to the user.
 * @retval KELPIE_OK Was able to perform the specified pick
 * @retval KELPIE_ENOENT Search didn't find any hits
 * @retval KELPIE_EINVAL Pick argument didn't match anything we knew about
 */
rc_t ComputeRegistry::fn_pick(faodel::bucket_t, const Key &key, const string &args,
                                      map<Key, lunasa::DataObject> ldos,
                                      lunasa::DataObject *ext_ldo) {
  int i;
  string choices[]={ "first", "last", "largest", "smallest" };
  if(args.empty()) {
    i=0;
  } else {
    for(i = 0; i < 4; i++) {
      if(args == choices[i]) break;
    }
  }
  if(i==4) return KELPIE_EINVAL;
  if(ldos.empty()) return KELPIE_ENOENT;
  if(!ext_ldo) return KELPIE_OK;

  switch(i) {
    case 0: *ext_ldo = ldos.begin()->second; break;
    case 1: *ext_ldo = ldos.rbegin()->second; break;
    case 2: {
        uint32_t largest_size = 0;
        for(auto &name_ldo : ldos) {
          if(name_ldo.second.GetUserSize() > largest_size) {
           *ext_ldo = name_ldo.second;
           largest_size = ext_ldo->GetUserSize();
          }
        }
        break;
      }
    case 3: {
        uint32_t smallest_size=ldos.begin()->second.GetUserSize() + 1;
        for(auto &name_ldo : ldos) {
          if(name_ldo.second.GetUserSize() < smallest_size) {
            *ext_ldo = name_ldo.second;
            smallest_size = ext_ldo->GetUserSize();
          }
        }
        break;
      }
    default:  ;
  }

  return KELPIE_OK;
}


}  //namespace internal
}  //namespace kelpie

