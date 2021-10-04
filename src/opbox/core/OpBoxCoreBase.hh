// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPBOXCOREBASE_HH
#define OPBOX_OPBOXCOREBASE_HH

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"


namespace opbox {
namespace internal {

/**
 * @brief Base class for OpBoxCores
 *
 * In order to allow developers to try new features out, OpBox implements
 * core functionality in an OpBoxCore. This is the base class for those
 * cores.
 */
class OpBoxCoreBase :
    faodel::InfoInterface {

public:
  OpBoxCoreBase();

  ~OpBoxCoreBase() override;

  //Bootstrap calls are handled by the singleton, which then calls these functions
  virtual void init(const faodel::Configuration &config)=0;
  virtual void start()=0;
  virtual void finish()=0;

  virtual int LaunchOp(Op *op, mailbox_t *resulting_mailbox)=0;
  virtual int TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args) = 0;

  virtual int HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message) = 0;
  virtual int UpdateOp(Op *op, OpArgs *args)=0;

  virtual int GetNumberOfActiveOps(unsigned int op_id=0) = 0;



  //virtual int doTriggerOp(faodel::internal_use_only_t iuo, mailbox_t mailbox, Op *op,  OpArgs *args) = 0;

  virtual std::string GetType() const = 0;


protected:

};

} // namespace internal
} // namespace opbox


#endif // OPBOX_OPBOXCOREBASE_HH
