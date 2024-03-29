// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPBOXCOREDEPRECATEDSTANDARD_HH
#define OPBOX_OPBOXCOREDEPRECATEDSTANDARD_HH

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
 * @brief This class provides the stock implementation of opbox
 *
 * @note the current standard interface may have ordering/timing issues
 * with some ops due to the way events are handled. There can be race
 * conditions where a network event finishes before the op completes
 * an action causing it to have odd errors. We are transitioning
 * to the CoreThreaded, which provides stronger guarantees.
 * 
 * @deprecated The original, standard class is now deprecated due to
 * ordering issues that arise in a threaded environment. Use 'threaded' instead
 */
class OpBoxCoreDeprecatedStandard :
    public OpBoxCoreBase,
    public faodel::LoggingInterface {

public:
  OpBoxCoreDeprecatedStandard();
  ~OpBoxCoreDeprecatedStandard() override;

  //Bootstrap calls are handled by the singleton, which then calls these functions
  void init(const faodel::Configuration &config) override;
  void start() override;
  void finish() override;

  int LaunchOp(opbox::Op *op, mailbox_t *resulting_mailbox) override;
  int TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args) override;

  int HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message) override;
  int UpdateOp(Op *op, OpArgs *args) override;

  int GetNumberOfActiveOps(unsigned int op_id=0) override;

  void HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results);

  std::string GetType() const override { return "standard"; }

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  bool initialized;
  bool running;
  bool shutdown_requested;

  opbox::Op * getActiveOp(mailbox_t mailbox);
  void        addActiveOp(opbox::Op *op);
  void        endActiveOp(mailbox_t mailbox);

  int doAction(mailbox_t my_mailbox, Op *op, OpArgs *args);

  faodel::MutexWrapper *op_mutex;
  std::map<int, opbox::Op *> active_ops;

  OpTimer *op_timer = nullptr; //Debug: collect info about how long each op took

};

} //namespace internal
} //namespace opbox

//extern opbox::OpBox ebox; //Global contained in OpBox.cpp

#endif // OPBOX_OPBOXCOREDEPRECATEDSTANDARD_HH
