// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_KELPIECOREBASE_HH
#define KELPIE_KELPIECOREBASE_HH

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/InfoInterface.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/Configuration.hh"

#include "kelpie/common/Types.hh"

#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/pools/PoolRegistry.hh"
#include "kelpie/ioms/IomRegistry.hh"
#include "kelpie/common/ComputeRegistry.hh"

namespace kelpie {
namespace internal {

/**
 * @brief Internal base class for a container that holds Kelpie's components
 *
 * In order to allow Kelpie developers to test out new functionality, Kelpie
 * uses a pluggable system for different implementations. A KelpieCore is
 * a class that contains all the components a particular implementation might
 * need.
 */
class KelpieCoreBase :
    public faodel::InfoInterface,
    public faodel::LoggingInterface {
public:
  KelpieCoreBase();

  ~KelpieCoreBase() override;

  virtual void init(const faodel::Configuration &config) = 0;
  virtual void start()=0;
  virtual void finish()=0;

  virtual std::string GetType() const = 0;
  virtual void getLKV(LocalKV **localkv_ptr) = 0;

  //Pool Management
  virtual void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) = 0;
  virtual Pool Connect(const faodel::ResourceURL &resource_url) = 0;
  virtual std::vector<std::string> GetRegisteredPoolTypes() const = 0;

  //Pool Server
  virtual int JoinServerPool(const faodel::ResourceURL &url, const std::string &optional_node_name) = 0;

  //IO Module
  IomRegistry iom_registry;
  ComputeRegistry compute_registry;

protected:

  faodel::bucket_t default_bucket;

};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_KELPIECOREBASE_HH
