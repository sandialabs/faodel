// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_SINGLETON_HH
#define KELPIE_SINGLETON_HH

#include <memory>

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/core/KelpieCoreBase.hh"
#include "kelpie/core/KelpieCoreUnconfigured.hh"

#include "kelpie/pools/PoolRegistry.hh"
#include "kelpie/ioms/IomRegistry.hh"

namespace kelpie {

class UnconfiguredPool; //Forward reference

namespace internal {

/**
 * @brief Singleton implementation. Contains the active KelpieCore.
 *
 * This implementation of the singleton holds the current KelpieCore
 * as well ass a KelpieCoreUnconfigured for error handling. This
 * class should only be created by the Singleton.
 */
class SingletonImpl
  : public faodel::bootstrap::BootstrapInterface,
    public faodel::LoggingInterface {

public:

  SingletonImpl();
  ~SingletonImpl() override;

  bool IsUnconfigured();

  //Bootstrap API
  void Init(const faodel::Configuration &config) override;
  void Start() override;
  void Finish() override;
  void GetBootstrapDependencies(std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const override;

  KelpieCoreBase *core;
  std::shared_ptr<UnconfiguredPool> unconfigured_pool;  

private:
  KelpieCoreUnconfigured unconfigured;


};


/**
 * @brief A singleton that holds that the static SingletonImpl for the node
 *
 * This class has a single static member and provides a way for internal
 * applications to get at the SingletonImpl, which contains the KelpieCore.
 */
class Singleton {
public:
  static kelpie::internal::SingletonImpl         impl;
};


}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_SINGLETON_HH
