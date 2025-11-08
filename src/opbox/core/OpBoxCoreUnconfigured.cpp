// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "opbox/core/OpBoxCoreBase.hh"
#include "opbox/core/OpBoxCoreUnconfigured.hh"
#include "opbox/core/OpBoxCoreDeprecatedStandard.hh"

#include "faodel-common/NodeID.hh"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

using namespace std;

namespace opbox {
namespace internal {


OpBoxCoreUnconfigured::OpBoxCoreUnconfigured()
  : LoggingInterface("opbox","Unconfigured") {
}
OpBoxCoreUnconfigured::~OpBoxCoreUnconfigured() = default;

//The unconfigured core should not be asked to do the internal start, init, or
//finish calls because it is never the real module. Init should pick a real
//core to create and then direct start/finish/init to it.
void OpBoxCoreUnconfigured::start()                      { Panic("start"); }
void OpBoxCoreUnconfigured::finish()                     { Panic("finish"); }
void OpBoxCoreUnconfigured::init(
                    const faodel::Configuration &config) { Panic("init"); }
int  OpBoxCoreUnconfigured::LaunchOp(
                    Op *op, mailbox_t *mailbox)          { Panic("LaunchOp"); return -1; }
int  OpBoxCoreUnconfigured::HandleIncomingMessage(
                    opbox::net::peer_ptr_t peer,
                    message_t *incoming_message)         { Panic("HandleIncomingMessage"); return -1; }
int  OpBoxCoreUnconfigured::UpdateOp(
                    Op *op, OpArgs *args)                { Panic("UpdateOp"); return -1; }
int  OpBoxCoreUnconfigured::GetNumberOfActiveOps(unsigned int op_id) { return 0;} //For simple queries
int  OpBoxCoreUnconfigured::TriggerOp(
                    mailbox_t mailbox,
                    shared_ptr<OpArgs> args)             { Panic("TriggerOp"); return -1; }


/**
 * @brief Issue an error message. User is attempting to call a function when opbox is in an unconfigured state
 *
 * @param[in] fname Which function the user requested
 */
void OpBoxCoreUnconfigured::Panic(const string fname) const {
  error("Attempted to use OpBoxCoreUnconfigure "+fname+"() before calling opbox::Init().\n"
         + "       OpBox must be initialized by hand or by faodel::Bootstrap before use.");
  exit(-1);
}

/**
 * @brief InfoInterface: dump information about a component (and its internal components)
 *
 * @param[in] ss     The stringstream to append this component's information to
 * @param[in] depth  How many layers deeper into the component we should go
 * @param[in] indent How many spaces to indent this component
 */
void OpBoxCoreUnconfigured::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[OpBox] "
     << "CurrentType: Unconfigured"
     << endl;
}

} // namespace internal
} // namespace opbox

