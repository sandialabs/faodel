// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_SINGLETON_HH
#define OPBOX_SINGLETON_HH

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"

#include "opbox/common/Types.hh"

#include "opbox/OpBox.hh"
#include "opbox/common/OpRegistry.hh"
#include "opbox/core/OpBoxCoreBase.hh"

#include "opbox/core/OpBoxCoreUnconfigured.hh"



namespace opbox {
namespace internal {


/**
 * @brief The actual OpBox singleton, which manages bootstraps
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

  void whookieInfoRegistry(faodel::ReplyStream &rs) { return registry.whookieInfo(rs); }

  OpRegistry registry;
  OpBoxCoreBase *core;


private:
  OpBoxCoreUnconfigured unconfigured;

};


/**
 * @brief A static placeholder for opbox's singleton
 */
class Singleton {
public:
  static opbox::internal::SingletonImpl         impl;

};

} // namespace internal
} // namespace opbox

#endif // OPBOX_OPBOXSINGLETON_HH
