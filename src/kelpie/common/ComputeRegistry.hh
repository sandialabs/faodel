// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_COMPUTEREGISTRY_HH
#define KELPIE_COMPUTEREGISTRY_HH

#include <memory>
#include <map>
#include <string>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/LoggingInterface.hh"

#include "kelpie/common/Types.hh"

namespace kelpie {
namespace internal {

/**
 * @brief An internal registry for (tracking existing/creating new) pools
 *
 * The ComputeRegistry stores functions that are available at the node for users
 */
class ComputeRegistry :
    public faodel::LoggingInterface {

 public:
  ComputeRegistry();
  ~ComputeRegistry();

  void init(const faodel::Configuration &config);
  void start();
  void finish();

  void RegisterComputeFunction(const std::string &compute_function_name, fn_compute_t function_pointer);

  rc_t doCompute(const std::string &compute_function_name,
                  const std::string &args,
                  faodel::bucket_t bucket, const Key &key,
                  std::map<Key, lunasa::DataObject> ldos,
                  lunasa::DataObject *ext_ldo);


  //Built-in UDF 'pick' selects the 'first', 'last', 'smallest', 'largest' DataObject to return
  static rc_t fn_pick(faodel::bucket_t, const Key &key, const std::string &args,
                      std::map<Key, lunasa::DataObject> ldos, lunasa::DataObject *ext_ldo);


private:
  bool started;
  int default_compute_logging_level;
  std::map<std::string, fn_compute_t> compute_fns;

  void HandleWhookieStatus(const std::map<std::string, std::string> &args, std::stringstream &results);
};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_COMPUTEREGISTRY_HH
