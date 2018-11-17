// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

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

  //IOM Management
  virtual void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) = 0;
  internal::IomBase * FindIOM(std::string iom_name) { return FindIOM(faodel::hash32(iom_name)); }
  virtual internal::IomBase * FindIOM(iom_hash_t iom_hash) = 0;

protected:

  faodel::bucket_t default_bucket;

};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_KELPIECOREBASE_HH
