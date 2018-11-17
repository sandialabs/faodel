// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_DIRMAN_SINGLETON_HH
#define OPBOX_DIRMAN_SINGLETON_HH

#include "common/Common.hh"
#include "common/LoggingInterface.hh"

#include "opbox/services/dirman/DirectoryInfo.hh"
#include "opbox/services/dirman/common/DirectoryCache.hh"
#include "opbox/services/dirman/common/DirectoryOwnerCache.hh"

#include "opbox/services/dirman/core/DirManCoreUnconfigured.hh"



namespace opbox {
namespace dirman {
namespace internal {

/**
 * @brief An implementation of the DirMan service's Singleton
 *
 * @note this is instantiated in the OpBox singleton
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



} // namespace internal
} // namespace dirman
} // namespace opbox

#endif // OPBOX_DIRMAN_SINGLETON_HH
