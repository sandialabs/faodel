// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <chrono>
#include <thread>
#include <stdexcept>

#include "faodel-common/Debug.hh"
#include "faodel-services/BackBurner.hh"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

#include "opbox/OpBox.hh"
#include "opbox/core/OpBoxCoreThreaded.hh"

#include "opbox/core/Singleton.hh" //for registry whookie

#if Faodel_ENABLE_DEBUG_TIMERS
#define OP_TIMER(a,b) if(op_timer) op_timer->Mark(a,b);
#define OP_TIMER_DISPATCHED(mbox) if(op_timer) op_timer->MarkDispatched(mbox);
#else
#define OP_TIMER(a,b)
#define OP_TIMER_DISPATCHED(mbox)
#endif


using namespace std;
using namespace opbox;

namespace opbox {
namespace internal {

OpBoxCoreThreaded::OpBoxCoreThreaded()
  : LoggingInterface("opbox","Threaded"),
    initialized(false),
    running(false),
    shutdown_requested(false) {

  op_mutex = faodel::GenerateMutex("pthreads","rwlock");

}
OpBoxCoreThreaded::~OpBoxCoreThreaded(){

  dbg("OpBoxCoreThreaded dtor");
  if (running){
    // cleanup: user or bootstrap should have called finish by now,
    // but maybe not.
    finish();
  }

  if(initialized) {
    shutdown_requested=true;

    op_mutex->WriterLock();
    for(auto op_ptr : active_ops) {
      delete op_ptr.second;
    }
    op_mutex->Unlock();
  }
  delete op_mutex;

  dbg("OpBoxCoreThreaded dtor done");
}


/**
 * @brief Internal initialize (called by OpBoxCoreUnconfigured)
 *
 * @param[in] config Info the user has passed to us for configuring the unit
 */
void OpBoxCoreThreaded::init(const faodel::Configuration &config) {


  ConfigureLogging(config);
  F_ASSERT(!initialized, "Attempted to initialize OpBoxCoreThreaded more than once");

  opbox::net::Init(config);

  #if Faodel_ENABLE_DEBUG_TIMERS
    //Only consider timers if the user compiles it in and asks for them
    bool enable_timers;
    config.GetBool(&enable_timers, "opbox.enable_timers", "false");
    if(enable_timers) op_timer = new OpTimer();
  #endif


  opbox::net::RegisterRecvCallback(opbox::internal::HandleIncomingMessage);

  whookie::Server::updateHook("/opbox", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieStatus(args, results);
  });
  whookie::Server::updateHook("/opbox/ops", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieActiveOps(args, results);
  });


  //Start thread
  //th_progress = thread(&OpBoxCoreThreaded::eventLoop, this);
  initialized=true;
}


/**
 * @brief Internal start function (called by OpBoxUnconfigured during bootstrap Initialize)
 *
 */
void OpBoxCoreThreaded::start() {
  dbg("private Start");
  F_ASSERT(initialized, "Attempted to start OpBoxCoreThreaded before initialization");
  opbox::net::Start();
  running=true;
}


/**
 * @brief Internal finish function (called by OpBoxUnconfigured during bootstrap Finish)
 *
 */
void OpBoxCoreThreaded::finish() {
  dbg("private finish");
  F_ASSERT(initialized && running, "Attempted to finish OpBoxCoreThreaded that is not started");

  whookie::Server::deregisterHook("/opbox");
  whookie::Server::deregisterHook("/opbox/ops");

  opbox::net::Finish();

  if(initialized) {
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
  if(op_timer) {
    op_timer->Dump();
    delete op_timer;
  }

  running=false;
}


/**
 * @brief Internal function for issuing an op update and dealing with op status
 * 
 * @param[in] iuo This is an internal function that should run on a specific backburner thread
 * @param[in] mailbox The local mailbox id (MAILBOX_UNSPECIFIED when not assigned)
 * @param[in] op Pointer to the op that will be updated
 * @param[in] args Arguments that will be handed to the update
 * @retval 0 Success
 * @retval -1 Failure
 */
int OpBoxCoreThreaded::doAction(faodel::internal_use_only_t iuo, mailbox_t mailbox, Op *op, OpArgs *args){

  dbg("doAction enter mailbox "+to_string(mailbox));
  // SanityCheckArgs(args);

  //Verify this op is still active
  if((mailbox!=0) && (getActiveOp(mailbox) != op)) {
    dbg("Op not found");
    args->result = -1;
    return args->result;
  }

  WaitingType rc = op->Update(args);
  op->touch();

  OP_TIMER(op,OpTimerEvent::ActionComplete);

  switch(rc){
  case WaitingType::done_and_destroy:
    if(mailbox!=0){
      endActiveOp(mailbox); //Handles delete of op
    }
    break;

  case WaitingType::error:
    error("Error in op handling");
    exit(-1);
    return -1;
    break;

  default:
//    //Op is doing something. Keep it in the active queue (or add it if new).
//    if(mailbox==0){
//      addActiveOp(op); //Sets the mailbox if needed
//    }
      break;
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
int OpBoxCoreThreaded::HandleIncomingMessage(opbox::net::peer_ptr_t peer, message_t *incoming_message){

  dbg("HandleIncomingMessage enter");

  Op *op;
  //OpArgs *args = new OpArgs(peer, incoming_message, true);
  mailbox_t my_mailbox = incoming_message->dst_mailbox;

  dbg("Incoming message for mailbox "+to_string(my_mailbox));

  //See if this is an unexpected message
  if(my_mailbox == 0){
    dbg("Creating new TargetOp. OpID is "+std::to_string(incoming_message->op_id));


    //New communication. Spin up a new op to andle it.
    op = opbox::internal::CreateNewTargetOp(incoming_message->op_id);

    if(op==nullptr) {
      F_HALT("Incoming message asked for an opid that was not known");
    }

    addActiveOp(op); //Sets the mailbox if needed
    my_mailbox = op->GetAssignedMailbox();
  } else {
    //Message is for an existing op. Find it.
    op = getActiveOp(my_mailbox);
    if(op==nullptr){
      cout <<"ERROR: Incoming message asked for op that is not active. Mailbox "<<to_string(my_mailbox)<<endl;
    }
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
    cerr <<ss.str();
    //return -1;
    F_HALT("Op Lookup fail "+ss.str());
    throw std::runtime_error(ss.str());
  }

  OP_TIMER(op,OpTimerEvent::Incoming);


  //Note: This has to be a shared pointer for safety. Also has
  //      to be handed as a copy instead of reference to lambda?
  auto args = make_shared<OpArgs>(peer, incoming_message, true);

  faodel::backburner::AddWork(op->GetAssignedMailbox(),
                              [this, my_mailbox, op, args]() mutable {
                                  return doAction(faodel::internal_use_only,
                                                  my_mailbox,
                                                  op,
                                                  args.get());
                              });

  OP_TIMER_DISPATCHED(my_mailbox);

  return 0;
}


int OpBoxCoreThreaded::UpdateOp(Op *op, OpArgs *args){

  //SanityCheckArgs(args);

  OP_TIMER(op, OpTimerEvent::Update);

  mailbox_t my_mailbox = op->GetAssignedMailbox();
  args->result = 0;


  faodel::backburner::AddWork(op->GetAssignedMailbox(),
                              [this, op, args, my_mailbox]() {
                                  doAction(faodel::internal_use_only,
                                           my_mailbox,
                                           op,
                                           args);
                                  //TODO: Check returns
                                  return 0;
                              });
  OP_TIMER_DISPATCHED(my_mailbox);

  return 0;
}

/**
 * @brief User mechanism for launching a new op. Op ownership is transferred to OpBox
 * @param[in] op A user-created op (ownership transferred to opbox)
 * @param[out] resulting_mailbox The mailbox generated for this op
*/
int OpBoxCoreThreaded::LaunchOp(Op *op, mailbox_t *resulting_mailbox){

  dbg("LaunchOp enter");

  F_ASSERT(initialized && running, "Attempted to StartOp when OpBoxCoreThreaded that is not running");
  F_ASSERT(op != nullptr, "Tried starting a null op");

  dbg("LaunchOp "+op->getOpName() +" state "+op->GetStateName());

  addActiveOp(op);

  mailbox_t my_mailbox = op->GetAssignedMailbox();
  if(resulting_mailbox) *resulting_mailbox = my_mailbox;

  OP_TIMER(op,OpTimerEvent::Launch);

  faodel::backburner::AddWork(op->GetAssignedMailbox(), //Put on the right thread
                              [this, op, my_mailbox]() {
                                  OpArgs args(UpdateType::start);
                                  int rc = doAction(faodel::internal_use_only,
                                                    my_mailbox,
                                                    op,
                                                    &args);
                                  if(rc != 0) args.result = rc;
                                  //TODO: Should we block here?

                                  return 0;
                              });
  OP_TIMER_DISPATCHED(my_mailbox);

  return 0;
}

/**
 * @brief Allow user to trigger an update of an op and pass in user data
 *
 * @param[in] mailbox The mailbox of the op that needs updating
 * @param[in] args User data to be sent to the op
 * @retval 0 Success
 * @retval -1 Failure
 */
int OpBoxCoreThreaded::TriggerOp(mailbox_t mailbox, std::shared_ptr<OpArgs> args) {
  dbg("TriggerOp enter mailbox "+to_string(mailbox));
  SanityCheckArgs(args);

  args->result = 0;

  Op *op = getActiveOp(mailbox);
  if(op==nullptr){
    if(mailbox==MAILBOX_UNSPECIFIED) {
        args->result = -1;
        return -1;
    }
    //throw std::runtime_error("Error: TriggerOp failed because mailbox is no longer active.\n");
    args->result = -1;
    return -1; //Not found
  }

  OP_TIMER(op,OpTimerEvent::Trigger);

  faodel::backburner::AddWork(mailbox,
                              [this, mailbox, op, args]() {
                                  doAction(faodel::internal_use_only,
                                           mailbox,
                                           op,
                                           args.get());
                                  //TODO: should this block?
                                  return 0;
                              });
  OP_TIMER_DISPATCHED(mailbox);

  return 0;
}

/**
 * @brief Internal retrieval of a pointer to an existing op based on its mailbox id
 *
 * @param[in] mailbox An identifier for the op we want to retrieve
 * @retval op A pointer to the op we want
 * @retval nullptr The op was not found
 */
Op * OpBoxCoreThreaded::getActiveOp(mailbox_t mailbox){

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
void OpBoxCoreThreaded::addActiveOp(Op *op){
  F_ASSERT(op != nullptr, "Null active op");
  mailbox_t mailbox = op->GetAssignedMailbox();
  F_ASSERT(mailbox != 0, "Op had a zero-value mailbox");
  op_mutex->WriterLock();
  active_ops[mailbox] = op;
  op_mutex->Unlock();
}


/**
 * @brief Internal function for removing an op that is no longer needed
 *
 * @param[in] mailbox An identifier for the op we want to delete
 */
void OpBoxCoreThreaded::endActiveOp(mailbox_t mailbox){
  F_ASSERT(mailbox != 0, "Op had a zero-value mailbox");
  dbg("EndActiveOp for mailbox "+to_string(mailbox));
  Op *op;
  op_mutex->WriterLock();
  auto it = active_ops.find(mailbox);
  if(it!=active_ops.end()){
    op = it->second;
    dbg("  EndActiveOp op is "+op->getOpName()+" state is "+ op->GetStateName());
    active_ops.erase(mailbox);
    delete op;
  }
  op_mutex->Unlock();
}

/**
 * @brief Count the number of active Ops that ObBox has (useful for shutdown)
 * @param[in] op_id Optional op_id field to search on. If set, only include ops that match
 * @retval COUNT the number of ops that are active
 */
int OpBoxCoreThreaded::GetNumberOfActiveOps(unsigned int op_id) {
  if(op_id==0) return active_ops.size();
  int count=0;
  op_mutex->ReaderLock();
  for(auto &mbox_opptr : active_ops){
    if (mbox_opptr.second->getOpID() == op_id) count++;
  }
  op_mutex->Unlock();
  return count;
}


/**
 * @brief HandleWhookieStatus Process a request from Whookie to get status information
 *
 * @param[in] args    The map of k/v parameters the user sent in this request
 * @param[in] results The stringstream to write results to
 */
void OpBoxCoreThreaded::HandleWhookieStatus(
                    const std::map<std::string,std::string> &args,
                    std::stringstream &results) {

    faodel::ReplyStream rs(args, "OpBox Status", &results);

    rs.tableBegin("OpBox Status");
    rs.tableTop({"Parameter", "Setting"});
    rs.tableRow({"Core Type", GetType()});
    rs.tableRow({"State", ((shutdown_requested) ? "Shutdown Requested" : ((running) ? "Running" : ((initialized) ? "Initialized" : "Uninitialized")))});
    rs.tableRow({"Active Ops", to_string(active_ops.size())});
    rs.tableEnd();

    rs.mkText(html::mkLink("Current Active Ops", "/opbox/ops"));
    
    opbox::internal::Singleton::impl.whookieInfoRegistry(rs);
    rs.Finish();
}

/**
 * @brief HandleWhookieActiveOps Process a request from Whookie to get Active Op information
 *
 * @param[in] args    The map of k/v parameters the user sent in this request
 * @param[in] results The stringstream to write results to
 */
void OpBoxCoreThreaded::HandleWhookieActiveOps(
                    const std::map<std::string,std::string> &args,
                    std::stringstream &results) {

  faodel::ReplyStream rs(args, "OpBox Active Ops", &results);
  if(!running) return;

  rs.tableBegin("OpBox Active Ops");
  rs.tableTop({"ID", "Name", "State", "Alive(s)","LastEvent(s)"});
  op_mutex->ReaderLock();
  
  for(auto &id_op : active_ops) {
    rs.tableRow({
          to_string(id_op.first),
          id_op.second->getOpName(),
          id_op.second->GetStateName(),
          to_string(id_op.second->GetSecondsSinceCreated()),
          to_string(id_op.second->GetSecondsSinceAccessed())});
  }
  op_mutex->Unlock();
  rs.Finish();
}

/**
 * @brief InfoInterface: dump information about a component (and its internal components)
 *
 * @param[in] ss     The stringstream to append this component's information to
 * @param[in] depth  How many layers deeper into the component we should go
 * @param[in] indent How many spaces to indent this component
 */
void OpBoxCoreThreaded::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[OpBoxCore] "
     << " Type: " << GetType()
     << " ActiveOps: "<<active_ops.size()
     << endl;
}

} // namespace internal
} // namespace opbox
