// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPBOX_HH
#define OPBOX_OPBOX_HH

#include <memory>

#include "common/NodeID.hh"

#include "opbox/ops/Op.hh"
#include "opbox/net/peer.hh"

namespace opbox {

std::string bootstrap();

faodel::nodeid_t GetMyID(); //Get our ID

void RegisterOp(int op_id, std::string op_name, fn_OpCreate_t func);
void DeregisterOp(int op_id, bool ignore_lock_warning);
int LaunchOp(opbox::Op *op, mailbox_t *mailbox_id=nullptr); //Launch a new operation
int TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args);


/**
 * @brief Template for registering an Op with Opbox. Automatically extracts info from Op.
 * @note: The class to be registered must inherit from Op and define both op_id and op_name
 */
template <class T> void RegisterOp() {
  RegisterOp(T::op_id, T::op_name, []{ return new T(Op::op_create_as_target); } );
}

/**
 * @brief Template for de-registering an Op with Opbox.
 * @note: The class to be registered must inherit from Op and define op_id
 */
template <class T> void DeregisterOp(bool ignore_lock_warning=false) {
  DeregisterOp(T::op_id, ignore_lock_warning);
}



namespace internal {

void HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message);
int UpdateOp(opbox::Op *op, OpArgs *args);
Op * CreateNewTargetOp(unsigned int op_id);
bool IsUnconfigured();

} // namespace internal


} // namespace opbox

#endif // OPBOX_OPBOX_HH
