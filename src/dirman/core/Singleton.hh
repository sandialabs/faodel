// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef DIRMAN_SINGLETON_HH
#define DIRMAN_SINGLETON_HH

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/DirectoryInfo.hh"

#include "dirman/common/DirectoryCache.hh"
#include "dirman/common/DirectoryOwnerCache.hh"
#include "dirman/core/DirManCoreUnconfigured.hh"



namespace dirman {
namespace internal {

/**
 * @brief An implementation of the DirMan service's Singleton
 *
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


  DirManCoreBase *core;


private:
  DirManCoreUnconfigured unconfigured;
  bool dirman_service_none;

};

/**
 * @brief A static placeholder for opbox's singleton
 */
class Singleton {
public:
  static dirman::internal::SingletonImpl impl;
};

} // namespace internal
} // namespace dirman

#endif // DIRMAN_SINGLETON_HH
