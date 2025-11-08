// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPBOXCOREUNCONFIGURED_HH
#define OPBOX_OPBOXCOREUNCONFIGURED_HH

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"

#include "opbox/common/Types.hh"

#include "opbox/OpBox.hh"
#include "opbox/common/OpRegistry.hh"
#include "opbox/core/OpBoxCoreBase.hh"

namespace opbox {
namespace internal {

/**
 * @brief A dummy OpBoxCore used to detect operations on an uninitialized system
 *
 * This core issues a panic message on every function. It is intended to be
 * used to catch scenarios when an application starts up or terminates
 * incorrectly.
 */
class OpBoxCoreUnconfigured :
    public OpBoxCoreBase,
    public faodel::LoggingInterface {

public:
  OpBoxCoreUnconfigured();
  ~OpBoxCoreUnconfigured() override;

  //OpBoxCoreBase API
  void start() override;
  void finish() override;
  void init(const faodel::Configuration &config) override;

  int LaunchOp(Op *op, mailbox_t *mailbox) override;
  int TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args) override;
  int GetNumberOfActiveOps(unsigned int op_id=0) override;

  int HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message) override;
  int UpdateOp(Op *op, OpArgs *args) override;
  
  std::string GetType() const override { return "unconfigured"; };


  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  void Panic(const std::string fname) const;

};
} //namespace internal
} //namespace opbox

#endif // OPBOX_OPBOXCOREUNCONFIGURED_HH
