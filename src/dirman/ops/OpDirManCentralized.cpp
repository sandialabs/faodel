// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>

#include "faodel-common/Debug.hh"

#include "dirman/DirMan.hh"
#include "dirman/ops/OpDirManCentralized.hh"

#include "dirman/ops/msg_dirman.hh"


using namespace std;

namespace dirman {

//Static names/ids for this op
const unsigned int OpDirManCentralized::op_id = const_hash("OpDirManCentralized");
const string OpDirManCentralized::op_name = "OpDirManCentralized";


/**
 * @brief Create the origin side of a HostNewDir operation
 * @param[in] req_type A host_new_dir constant expression to specify which operation is happening
 * @param[in] root_id The root node where this message should be transmitted
 * @param[in] dir_info The DirectoryInfo structure (which can include a list of participating nodes) to register
 */
OpDirManCentralized::OpDirManCentralized(RequestType req_type, faodel::nodeid_t root_id, faodel::DirectoryInfo dir_info)
        : Op(true), state(State::start), ldo_msg(), request_type(RequestType::HostNewDir) {

  F_ASSERT(req_type == RequestType::HostNewDir, "Only supports hostnewdir now");

  int rc = opbox::net::Connect(&peer, root_id); //Retrieve the root's peer ptr
  F_ASSERT((rc == 0), "Connect failed?");

  msg_dirman::AllocateRequest(ldo_msg,
                              RequestType::HostNewDir,
                              root_id, GetAssignedMailbox(), dir_info);

  //Work picks up again in origin's state machine
}


/**
 * @brief Create the origin side of a GetInfo operation
 * @param[in] req_type A constant expression to specify which operation is happening
 * @param[in] root_id The root node where this message should be transmitted
 * @param[in] url The name of the resource that we're looking up
 */
OpDirManCentralized::OpDirManCentralized(RequestType req_type, faodel::nodeid_t root_id, faodel::ResourceURL url)
        : Op(true), state(State::start), ldo_msg(), request_type(req_type) {

  F_ASSERT((req_type == RequestType::GetInfo) ||
           (req_type == RequestType::JoinDir) ||
           (req_type == RequestType::LeaveDir) ||
           (req_type == RequestType::DropDir) ||
           (req_type == RequestType::ReturnDirInfo), "Request type not handled");

  int rc = opbox::net::Connect(&peer, root_id); //Retrieve the root's peer ptr
  if(rc!=0) {
    throw std::runtime_error("DirMan could not connect to server "+root_id.GetHex()+" - "+root_id.GetHttpLink());
  }

  msg_dirman::AllocateRequest(ldo_msg,
                              req_type,
                              root_id, GetAssignedMailbox(), url);

  //Work picks up again in origin's state machine
}

/**
 * @brief Create the target side of a new DirMan message. Allocates space for a reply message
 * @param t The op_create_as_target_t is used to signify that this Op is a target, not the initiator
 * @return OpDirManCentralized - The target state machine, ready to be triggered for handling a new message
 */
OpDirManCentralized::OpDirManCentralized(op_create_as_target_t t)
        : Op(t), state(State::start), ldo_msg() {
  //No work to do - done in target's state machine
}

/**
 * @brief Close out an OpDirCreate message, releasing resources held during its lifetime
 * @return void
 */
OpDirManCentralized::~OpDirManCentralized() = default;

/**
 * @brief Get a future for handing back the resulting DirInfo
 * @return future
 * @note This function must be executed **before** launching the Op
 */
future<faodel::DirectoryInfo> OpDirManCentralized::GetFuture() {
  return di_promise.get_future();
}

/**
 * @brief Get a string label for the current state
 * @return string label
 */
string OpDirManCentralized::GetStateName() const {
  switch (state) {
    case State::start:
      return "Start";
    case State::snd_wait_for_reply:
      return "Sender-WaitForReply";
    case State::done:
      return "Done";
  }
  F_FAIL();
}

} // namespace dirman