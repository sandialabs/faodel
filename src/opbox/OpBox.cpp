// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "faodel-common/NodeID.hh"
#include "lunasa/DataObject.hh"
#include "opbox/OpBox.hh"

#include "opbox/core/Singleton.hh"


using namespace std;



namespace opbox {
namespace internal {

/**
 * @brief Internal function for processing a new, incoming message
 *
 * @param[in] peer The node where the message came from
 * @param[in] incoming_message A pointer to the actual message
 */
void HandleIncomingMessage(net::peer_ptr_t peer, message_t *incoming_message) {
  Singleton::impl.core->HandleIncomingMessage(peer, incoming_message); }

/**
 * @brief Internal function for updating an Op
 *
 * @param[in] op The op to update
 * @param[in] args A pointer to the update args
 * @retval 0 Success
 * @retval -1 Failure
 *
 * @deprecated User should use trigger op for safety
 */
int UpdateOp(Op *op, OpArgs *args) {
  return Singleton::impl.core->UpdateOp(op, args);
}

/**
 * @brief Instantiate (but don't launch) a new operation at the target
 *
 * @param[in] op_id The hash of the operation name
 * @return A pointer to the new target Op
 */
Op * CreateNewTargetOp(unsigned int op_id) {
  return Singleton::impl.registry.CreateOp(op_id);
}

bool IsUnconfigured(){
  return Singleton::impl.IsUnconfigured();
}

} // namespace internal



/**
 * @brief Register a new operation type for use with OpBox (usually done before start)
 *
 * @param[in] id A unique id for the op (usually a static hash of the name)
 * @param[in] name The name for this op
 * @param[in] func A function for automatically creating an op (usually a new op(op_create_as_target))
 * @note Most Op registrations should use the RegisterOp() template instead
 */
void RegisterOp(int id, std::string name, fn_OpCreate_t func)  {
  return opbox::internal::Singleton::impl.registry.RegisterOp(id, name, func);
}

/**
 * @brief Remove a handler for a specific operation
 *
 * @param[in] id The hash value of the operation name
 * @param[in] ignore_lock_warning When true, ignore locks and remove (necessary for forced shutdowns)
 */
void DeregisterOp(int id, bool ignore_lock_warning)                 {
  return opbox::internal::Singleton::impl.registry.DeregisterOp(id, ignore_lock_warning);
}

/**
 * @brief Take ownership of an op a user has created and begin executing it
 *
 * @param[in] op An operation the user has created
 * @param[out] mailbox The mailbox this message was assigned
 * @note Ownership of the op is handed to OpBox at this point. The op will be
 *       destroyed when it completes, and thus users should not use the
 *       pointer after calling LaunchOp
 */
int LaunchOp(Op *op, mailbox_t *mailbox) {
  return opbox::internal::Singleton::impl.core->LaunchOp(op, mailbox);
}


int TriggerOp(mailbox_t mailbox, shared_ptr<OpArgs> args) {
  return opbox::internal::Singleton::impl.core->TriggerOp(mailbox, args);
}

/**
 * @brief Return a unique identifier that opbox and others can use to reference this rank
 *
 * @return faodel::nodeid_t
 */
faodel::nodeid_t GetMyID() {
  return opbox::net::GetMyID();
}

} // namespace opbox
