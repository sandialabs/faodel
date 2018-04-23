// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>

#include "common/Debug.hh"

#include "opbox/common/OpRegistry.hh"

using namespace std;

namespace opbox {

OpRegistry::OpRegistry()
  : finalized(false) {

  mutex = faodel::GenerateMutex();

}

/**
 * @brief Signify that work has been completed and that all known ops should be discarded
 */
void OpRegistry::Finish(){
  kassert(finalized, "Finish attempted on OpRegistry when it has not been started");

  mutex->Lock();
  //Get rid of all post ops
  known_ops_post.clear();
  op_names_post.clear();
  mutex->Unlock();

  //Get rid of all pre ops
  known_ops_pre.clear();
  op_names_pre.clear();

  finalized=false;
}

/**
 * @brief Register a function for creating a particular op
 * @param[in] op_id   The unique id for the op (a compile-time hash of its name)
 * @param[in] op_name The unique string name of the op
 * @param[in] func    A function for creating the new op
 * @return void
 *
 * The RegisterOp function is used to register a lambda for creating a new
 * Op associated with a particular ID. A user should define a meaningful
 * name for the op (op_name) and use a compile-time hash of the name to
 * uniquely identify the Op. This function will exit if the given op_id
 * matches a previously registered op (it assumes there is a hash collision).
 *
 * It is expected that most users will use the RegisterOp template to
 * register functions, as id/name fields can be cumbersome. eg, use:
 *     opbox::RegisterOp<OpMyBigStateMachine>();
 *
 * Ops can be registered at any point in time, but there is a performance
 * advantage to registering them before the bootstrap::Start() phase takes
 * place (ie, it avoids a mutex).
 */
void OpRegistry::RegisterOp(int op_id, string op_name, fn_OpCreate_t func){

  //Note: user specifies op_id and op_name because if we create a dummy
  //      op at this point, it's possible something in the ctor won't
  //      be ready.

  //See if in pre-finalized set
  auto search = known_ops_pre.find(op_id);
  if (search != known_ops_pre.end()){
    std::cout <<"Error: duplicate register of same Op function. Could be a hash collision or duplicates. Name is: "<<op_name<<endl;
    std::cout <<" All known ops:\n" << str(1,3) <<endl;
    exit(-1);
  }
  if(!finalized){
    known_ops_pre[op_id] = func;
    op_names_pre[op_id] = op_name;
  } else {
    //We're currently running. Must also search the post section
    mutex->Lock();
    search = known_ops_post.find(op_id);
    if(search != known_ops_post.end()){
      std::cout <<"Error: duplicate register of same Op function. Could be a hash collision or duplicates. Name is: "<<op_name<<endl;
      std::cout <<" All known ops:\n" << str(1,3) <<endl;
      exit(-1);
    }
    known_ops_post[op_id] = func;
    op_names_post[op_id] = op_name;
    mutex->Unlock();
  }
}

/**
 * @brief Remove an Op from service
 * @param[in] op_id                The op id to remove
 * @param[in] ignore_lock_warning  Ignore check to see if attempting to deregister a pre op when finalized
 *
 * TODO: Ops that were registered before bootstrap was started emit a warning
 */
void OpRegistry::DeregisterOp(int op_id, bool ignore_lock_warning){
  auto search1 = known_ops_pre.find(op_id);
  if(search1 != known_ops_pre.end()){
    if(finalized && !ignore_lock_warning){
      //TODO: we should really shut down more gracefully. However, OpRegistry doesn't
      //      reset finalized until it is shut down, which usually happens after the
      //      user shuts down.
      std::cout <<"Warning:: Deregistering pre-finalized op while in finalized state\n";
      exit(0);
    }

    known_ops_pre.erase(search1);
    auto search1b = op_names_pre.find(op_id);
    if(search1b != op_names_pre.end())
      op_names_pre.erase(search1b);
    return;
  }

  mutex->Lock();
  auto search2 = known_ops_post.find(op_id);
  if(search2 != known_ops_post.end()){
    known_ops_post.erase(search2);
    auto search2b = op_names_post.find(op_id);
    if(search2b != op_names_post.end())
      op_names_post.erase(search2b);
    mutex->Unlock();
    return;
  }
  mutex->Unlock();
  cout <<"Error: Deregister op "<<op_id<<" did not find requested op to deregister\n";
}

/**
 * @brief Creates a new Op for the given op id
 * @param[in] op_id    Unique id for op (usually a compile-time hash of the op name)
 * @retval    OP*      A pointer to the newly-create Op
 * @retval    nullptr  The op was not found in the list
 */
Op * OpRegistry::CreateOp(unsigned int op_id){

  //Search 1: do a lockless search on the pre-finalize ops
  auto search1 = known_ops_pre.find(op_id);
  if(search1 != known_ops_pre.end()){
    return search1->second();
  }

  //Search 2: do a locked search on the post-finalize ops
  mutex->Lock();
  auto search2 = known_ops_post.find(op_id);
  if(search2 != known_ops_post.end()){
    Op *res = search2->second();
    mutex->Unlock();
    return res;
  }
  mutex->Unlock();
  return nullptr;

}


/**
 * @brief Process a request from Webhook to get status information
 *
 * @param[in] args    The map of k/v parameters the user sent in this request
 * @param[in] results The stringstream to write results to
 */
void OpRegistry::HandleWebhookStatus(
                    const std::map<std::string,std::string> &args,
                    std::stringstream &results) {

  webhook::ReplyStream rs(args, "OpBox OpRegistry Status", &results);
  webhookInfo(rs);
  rs.Finish();
}

/**
 * @brief Append information about the reqistry to a webhook replystream
 *
 * @param[in] rs The replystream that output should be written to
 */
void OpRegistry::webhookInfo(webhook::ReplyStream &rs) {

  rs.tableBegin("OpRegistry");
  rs.tableTop({"Parameter","Setting"});
  rs.tableRow({"Finalized:", (finalized)?"True":"False"});
  rs.tableRow({"Pre-Finalized Entries:", to_string(op_names_pre.size())});
  rs.tableRow({"Post-Finalized Entries:", to_string(op_names_post.size())});
  rs.tableEnd();

  rs.tableBegin("Operations Registered before OpBox Start()\n");
  rs.tableTop({"Name","Hash(name)"});
  for(auto &id_name : op_names_pre){
    rs.tableRow( { id_name.second, to_string(id_name.first) } );
  }
  rs.tableEnd();


  rs.tableBegin("Operations Registered after OpBox Start()\n");
  rs.tableTop({"Name","Hash(name)"});
  mutex->ReaderLock();
  for(auto &id_name : op_names_post){
    rs.tableRow( { id_name.second, to_string(id_name.first) } );
  }
  rs.tableEnd();
  mutex->Unlock();


}

/**
 * @brief Dump information about a component (and its internal components)
 *
 * @param[in] ss     The stringstream to append this component's information to
 * @param[in] depth  How many layers deeper into the component we should go
 * @param[in] indent How many spaces to indent this component
 */
void OpRegistry::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[OpRegistry]"
     << " PreInitOps: "<< known_ops_pre.size()
     << " PostInitOps: "<< known_ops_post.size()
     << endl;
  if(depth>0){
    indent++;
    for(auto x : known_ops_pre){
      Op *op = x.second();
      ss<< string(indent,' ') << "["<<hex<<x.first<<"] "<<op->getOpName()<<endl;
      delete op;
    }

    mutex->Lock();
    for(auto x : known_ops_post){
      Op *op = x.second();
      ss<< string(indent,' ') << "["<<hex<<x.first<<"] "<<op->getOpName()<<endl;
      delete op;
    }
    mutex->Unlock();
  }
}

} // namespace opbox
