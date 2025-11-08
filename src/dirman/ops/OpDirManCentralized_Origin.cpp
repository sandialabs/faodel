// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>

#include "faodel-common/Debug.hh"

#include "opbox/ops/OpHelpers.hh"

#include "dirman/DirMan.hh"
#include "dirman/ops/OpDirManCentralized.hh"

#include "dirman/ops/msg_dirman.hh"

using namespace std;
using namespace faodel;
using namespace dirman;

namespace dirman {

/**
 * @brief Mechanism for updating the Origin's state machine
 * @param[in,out] args Allows passing of incoming and outgoing arguments
 * @return WaitingType Tells opbox what to do next
 */
WaitingType OpDirManCentralized::UpdateOrigin(OpArgs *args) {


  switch (state) {

    // Sender START: send message off to root.
    // ldo was created and set during ctor. We only need to be notified
    // if there were problems sending the message, as we will expect a
    // reply from the Target.
    case State::start:
      //cout <<"OpDirManCentralized Sender:  Starting to send initial message\n";
      opbox::net::SendMsg(peer, std::move(ldo_msg), UnsuccessfulOnlyCallback(this));
      return updateState(State::snd_wait_for_reply, WaitingType::waiting_on_cq);

      // Sender WAIT_FOR_REPLY: wait for server to get back to us
    case State::snd_wait_for_reply: //wait for reply

      //Parse the incoming message
    {

      auto *msg = args->ExpectMessageOrDie<message_t *>();
      auto req_type = static_cast<RequestType>(msg->user_flags);

      switch (req_type) {
        case RequestType::Invalid: {
          //Invalid can happen. this isn't a reply message
          cerr << "Invalid OpDirManCentalized response at origin\n";
          F_HALT("InvalidMessage")
        }
          break;

        case RequestType::ReturnDirInfo: {
          DirectoryInfo dir_info = msg_dirman::ExtractDirInfo(msg);
          di_promise.set_value(dir_info);
          return updateState(State::done, WaitingType::done_and_destroy);
        }

        default: {
          cerr << "Unexpected message type returned to origin in OpDirManCentralized\n";
          exit(-1);
        }
      }
    }
      //Should not get here
      break;

      // Sender DONE: everything is complete, wait to be destroyed
    case State::done:
      return updateState(State::done, WaitingType::done_and_destroy);

    default: F_TODO("Unhandled state in OpDirManCentralized origin Update");
  }
  //Shouldn't get here
  return updateState(State::done, WaitingType::error);
}

} // namespace dirman