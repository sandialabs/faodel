// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPBOXCORETHREADED_HH
#define OPBOX_OPBOXCORETHREADED_HH

#include <vector>
#include <thread>

#include "faodel-common/LoggingInterface.hh"

#include "lunasa/DataObject.hh"
#include "opbox/ops/Op.hh"
#include "opbox/core/OpBoxCoreBase.hh"
#include "opbox/core/OpTimer.hh"

namespace opbox {
namespace internal {

/**
 * @brief A threaded version of the opbox core with stronger guarantees
 *
 * This core is designed to provide stronger guarantees about how events
 * are passed to Ops.
 *
 * @note It is expected that this will become the new standard core in
 * later releases.
 */
class OpBoxCoreThreaded :
    public OpBoxCoreBase,
    public faodel::LoggingInterface {

public:
  OpBoxCoreThreaded();
  ~OpBoxCoreThreaded() override;

  //Boostrap calls are handled by the singleton, which then calls these functions
  void init(const faodel::Configuration &config) override;
  void start() override;
  void finish() override;

  int LaunchOp(opbox::Op *op, mailbox_t *resulting_mailbox) override;
  int TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args) override;

  int HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message) override;
  int UpdateOp(Op *op, OpArgs *args) override;

  int GetNumberOfActiveOps(unsigned int op_id=0) override;

  void HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieActiveOps(const std::map<std::string,std::string> &args, std::stringstream &results);
  std::string GetType() const override { return "threaded"; }

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  bool initialized;
  bool running;
  bool shutdown_requested;

  int doAction(faodel::internal_use_only_t iuo, mailbox_t mailbox, Op *op, OpArgs *args);

  opbox::Op * getActiveOp(mailbox_t mailbox);
  void        addActiveOp(opbox::Op *op);
  void        endActiveOp(mailbox_t mailbox);


  faodel::MutexWrapper *op_mutex;
  std::map<int, opbox::Op *> active_ops;

  OpTimer *op_timer = nullptr; //Debug: collect info about how long each op took

};

} //namespace internal
} //namespace opbox

//extern opbox::OpBox ebox; //Global contained in OpBox.cpp

#endif // OPBOX_OPBOXCORETHREADED_HH
