// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <chrono>
#include <thread>
#include <stdexcept>

#include "faodel-common/Debug.hh"

#include "webhook/WebHook.hh"
#include "webhook/Server.hh"

#include "opbox/OpBox.hh"
#include "opbox/core/OpBoxCoreStandard.hh"
#include "opbox/core/OpBoxCoreUnconfigured.hh"

#include "opbox/core/Singleton.hh" //for registry webhook


using namespace std;
using namespace opbox;

namespace opbox {
namespace internal {

OpBoxCoreStandard::OpBoxCoreStandard()
  : LoggingInterface("opbox","Standard"),
    initialized(false),
    running(false),
    shutdown_requested(false) {

  op_mutex = faodel::GenerateMutex("pthreads","rwlock");

}
OpBoxCoreStandard::~OpBoxCoreStandard(){

  dbg("standard dtor");
  if (running){
    // cleanup: user or bootstrap should have called finish by now,
    // but maybe not.
    finish();
  }

  if(initialized){
    shutdown_requested=true;
    //th_progress.join();

    op_mutex->WriterLock();
    for(auto op_ptr : active_ops){
      delete op_ptr.second;
    }
    op_mutex->Unlock();
  }
  delete op_mutex;

  dbg("standard dtor done");
}


/**
 * @brief Internal initialize (called by OpBoxCoreUnconfigured)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void OpBoxCoreStandard::init(const faodel::Configuration &config) {


  ConfigureLogging(config);
  dbg("private Init");
  kassert(!initialized, "Attempted to initialize OpBoxCoreStandard more than once");

  opbox::net::Init(config);

  dbg("Done with opbox::net::Init()");
  opbox::net::RegisterRecvCallback(opbox::internal::HandleIncomingMessage);

  webhook::Server::updateHook("/opbox", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWebhookStatus(args, results);
  });

  //Start thread
  //th_progress = thread(&OpBoxCoreStandard::eventLoop, this);
  initialized=true;
}


/**
 * @brief Internal start function (called by OpBoxUnconfigured during bootstrap Initialize)
 *
 */
void OpBoxCoreStandard::start() {
  dbg("private Start");
  kassert(initialized, "Attempted to start OpBoxCoreStandard before initialization");
  opbox::net::Start();
  running=true;
}


/**
 * @brief Internal finish function (called by OpBoxUnconfigured during bootstrap Finish)
 *
 */
void OpBoxCoreStandard::finish() {
  dbg("private finish");
  kassert(initialized && running, "Attempted to finish OpBoxCoreStandard that is not started");

  webhook::Server::deregisterHook("/opbox");

  opbox::net::Finish();

  if(initialized){
    dbg("deleting all");
    shutdown_requested=true;
    //th_progress.join();
    op_mutex->WriterLock();
    for(auto op_ptr : active_ops){
      delete op_ptr.second;
      active_ops.erase(op_ptr.first);
    }
    op_mutex->Unlock();
    initialized=false;
  }

  running=false;
}


/**
 * @brief Internal function for issuing an op update and dealing with op status
 *
 * @param[in] my_mailbox The local mailbox id (MAILBOX_UNSPECIFIED when not assigned)
 * @param[in] op Pointer to the op that will be updated
 * @param[in] args Arguments that will be handed to the update
 * @retval 0 Success
 * @retval -1 Failure
 */
int OpBoxCoreStandard::doUpdate(mailbox_t my_mailbox, Op *op, OpArgs *args){

  WaitingType rc = op->Update(args);
  op->touch();

  switch(rc){
  case WaitingType::done_and_destroy:
    if(my_mailbox!=0){
      endActiveOp(my_mailbox); //Handles delete of op
    }
    break;

  case WaitingType::error:
    error("Error in op handling");
    exit(-1);
    return -1;
    break;

  default:
    //Op is doing something. Keep it in the active queue (or add it if new).
    if(my_mailbox==0){
      addActiveOp(op); //Sets the mailbox if needed
    }
  }

  return args->result;
}


/**
 * @brief Examine an incoming message and pass it to either a new or existing Op
 *
 * @param peer Handle to the node that sent this message
 * @param incoming_message Pointer to the message (memory owned by net)
 * @throws runtime_error if op is unknown or no longer active
 */
int OpBoxCoreStandard::HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message){

  Op *op;
  OpArgs args(peer, incoming_message);
  mailbox_t my_mailbox = incoming_message->dst_mailbox;

  dbg("Incoming message for mailbox "+to_string(my_mailbox));

  //See if this is an unexpected message
  if(my_mailbox == 0){
    //New communication. Spin up a new op to andle it.
    op = opbox::internal::CreateNewTargetOp(incoming_message->op_id);
  } else {
    //Message is for an existing op. Find it.
    op = getActiveOp(my_mailbox);
  }

  //Verify op was known. If not, throw an error
  if(op==nullptr){
    stringstream ss;
    ss<<"Error: Incoming message for";
    if(my_mailbox==0) {
      ss<<" new Op "
        << std::hex<<incoming_message->op_id
        << " failed because Op was not known. Known Ops:\n";

      opbox::internal::Singleton::impl.registry.sstr(ss, 100, 4);

    } else{
      ss<<" existing Op (mailbox "<<my_mailbox<<") failed because Op not active.\n";
    }
    throw std::runtime_error(ss.str());
  }

  //Call the update and deal with storing the op
  return doUpdate(my_mailbox, op, &args);

}


/**
 * @brief Allow user to trigger an update of an op and pass in user data
 *
 * @param[in] op The op to update
 * @param[in] args User data to be sent to the op
 * @retval 0 Success
 * @retval -1 Failure
 */
int OpBoxCoreStandard::UpdateOp(Op *op, OpArgs *args){

  args->result = 0;
  int rc =  doUpdate(op->GetAssignedMailbox(), op, args);
  return rc;
}


/**
 * @brief User mechanism for launching a new op. Op ownership is transferred to OpBox
 *
 * @param[in] op A user-created op (ownership transferred to opbox)
 * @param[out] resulting_mailbox The mailbox generated for this op
 */
int OpBoxCoreStandard::LaunchOp(Op *op, mailbox_t *resulting_mailbox){
  kassert(initialized && running, "Attempted to StartOp when OpBoxCoreStandard that is not running");
  kassert(op!=nullptr, "Tried starting a null op");

  dbg("LaunchOp "+op->getOpName());

  //TODO: Can this be reordered so registration happens after update?
  addActiveOp(op);

  OpArgs args(UpdateType::start);
  args.result = 0;

  WaitingType rc = op->Update(&args);
  if(rc == WaitingType::done_and_destroy){
    dbg("LaunchOp update completed w/ done+destroy");
    endActiveOp(op->GetAssignedMailbox());
    if(resulting_mailbox) *resulting_mailbox = MAILBOX_UNSPECIFIED;

    args.result = 0;

    return 0;

  } else {
    dbg("LaunchOp update dispatched with more work to do");
    op->touch();
    if(resulting_mailbox) *resulting_mailbox = op->GetAssignedMailbox();

    args.result = 0;

    return 0;
  }
}


/**
 * @brief Allow user to trigger an update of an op and pass in user data
 *
 * @param[in] mailbox The mailbox of the op that needs updating
 * @param[in] args User data to be sent to the op
 * @retval 0 Success
 * @retval -1 Failure
 */
int OpBoxCoreStandard::TriggerOp(mailbox_t mailbox, shared_ptr<OpArgs> args){

  args->result = 0;
  Op *op = getActiveOp(mailbox);
  if(op==nullptr){
    args->result = -1;
    if(mailbox==MAILBOX_UNSPECIFIED) {
        args->result = -1;
        return -1;
    }
    //throw std::runtime_error("Error: TriggerOp failed because mailbox is no longer active.\n");
    args->result = -1;
    return -1; //Not found
  }

  return doUpdate(mailbox, op, args.get());

}


/**
 * @brief Internal retrieval of a pointer to an existing op based on its mailbox id
 *
 * @param[in] mailbox An identifier for the op we want to retrieve
 * @retval op A pointer to the op we want
 * @retval nullptr The op was not found
 */
Op * OpBoxCoreStandard::getActiveOp(mailbox_t mailbox){

  if(mailbox==0) return nullptr;
  Op *op;
  op_mutex->ReaderLock();
  auto it = active_ops.find(mailbox);
  op = (it!=active_ops.end()) ? it->second : nullptr;
  op_mutex->Unlock();

  return op;
}


/**
 * @brief Internal function for storing a user-created op in our list of active ops
 *
 * @param[in] op The op in question (may trigger the assignment of a mailbox id)
 */
void OpBoxCoreStandard::addActiveOp(Op *op){
  kassert(op!=nullptr, "Null active op");
  mailbox_t mailbox = op->GetAssignedMailbox();
  kassert(mailbox!=0, "Op had a zero-value mailbox");
  op_mutex->WriterLock();
  active_ops[mailbox] = op;
  op_mutex->Unlock();
}


/**
 * @brief Internal function for removing an op that is no longer needed
 *
 * @param[in] mailbox An identifier for the op we want to delete
 */
void OpBoxCoreStandard::endActiveOp(mailbox_t mailbox){
  kassert(mailbox!=0, "Op had a zero-value mailbox");
  Op *op;
  op_mutex->WriterLock();
  auto it = active_ops.find(mailbox);
  if(it!=active_ops.end()){
    op = it->second;
    active_ops.erase(mailbox);
    delete op;
  }
  op_mutex->Unlock();
}


/**
 * @brief HandleWebhookStatus Process a request from Webhook to get status information
 *
 * @param[in] args    The map of k/v parameters the user sent in this request
 * @param[in] results The stringstream to write results to
 */
void OpBoxCoreStandard::HandleWebhookStatus(
                    const std::map<std::string,std::string> &args,
                    std::stringstream &results) {

    faodel::ReplyStream rs(args, "OpBox Status", &results);

    vector<pair<string,string>> stats;
    stats.push_back(pair<string,string>("Core Type", GetType()));
    rs.mkTable(stats, "OpBox Status");
    opbox::internal::Singleton::impl.webhookInfoRegistry(rs);
    rs.Finish();
}


/**
 * @brief InfoInterface: dump information about a component (and its internal components)
 *
 * @param[in] ss     The stringstream to append this component's information to
 * @param[in] depth  How many layers deeper into the component we should go
 * @param[in] indent How many spaces to indent this component
 */
void OpBoxCoreStandard::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[OpBoxCore] "
     << " Type: " << GetType()
     << " ActiveOps: "<<active_ops.size()
     << endl;

}


} // namespace internal
} // namespace opbox

