// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_POOLREGISTRY_HH
#define KELPIE_POOLREGISTRY_HH

#include <memory>
#include <map>
#include <string>

#include "common/FaodelTypes.hh"
#include "common/ResourceURL.hh"

#include "kelpie/common/Types.hh"

namespace kelpie {
namespace internal {

/**
 * @brief An internal registry for (tracking existing/creating new) pools
 *
 * The PoolRegistry stores two types of information relating to pools. First,
 * it maintains a list of Pool constructors that are used to generate new
 * pool instances. Users may register new pool constructors before start
 * time via the RegisterPoolConstructor() function. Second, the registry
 * maintains a list of pools in the system that this application instance
 * has connected to. New connections can be established via Connect().
 */
class PoolRegistry {

 public:
  PoolRegistry();
  ~PoolRegistry();

  void init(faodel::bucket_t default_bucket);
  void start();
  void finish();

  Pool Connect(const faodel::ResourceURL &pool_url);
  void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t function_pointer);

 private:
  faodel::MutexWrapper *mutex;
  faodel::bucket_t default_bucket;
  std::map<std::string, fn_PoolCreate_t> pool_create_fns;  // ctors for pools
  std::map<std::string, std::shared_ptr<PoolBase>> known_pools;

  std::string makeKnownPoolKey(const faodel::ResourceURL &url) const {
    return url.bucket.GetHex()+url.path+"/"+url.name;
  }
  void HandleWebhookStatus(const std::map<std::string, std::string> &args, std::stringstream &results);
};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_POOLREGISTRY_HH
