// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <map>

#include "faodel-common/Debug.hh"

#include "dirman/DirMan.hh"
#include "dirman/ops/OpDirManCentralized.hh"

#include "dirman/ops/msg_dirman.hh"

using namespace std;
using namespace faodel;
using namespace dirman;

namespace dirman {

/**
 * @brief Mechanism for updating the Target's state machine
 * @param[in,out] args Allows passing of incoming and outgoing arguments
 * @return WaitingType Tells opbox what to do next
 */
WaitingType OpDirManCentralized::UpdateTarget(OpArgs *args) {

  DirectoryInfo result_dir_info;
  DirectoryInfo incoming_dir_info;
  faodel::ResourceURL url;

  switch (state) {

    //Start: Examine the message and send a response
    case State::start: {
      auto *msg = args->ExpectMessageOrDie<message_t *>(&peer);
      auto req_type = static_cast<RequestType>(msg->user_flags);

      if (req_type == RequestType::HostNewDir) {
        //Only instance of a msg_dirman
        incoming_dir_info = msg_dirman::ExtractDirInfo(msg);
        dirman::HostNewDir(incoming_dir_info);
        dirman::GetLocalDirectoryInfo(incoming_dir_info.url, &result_dir_info);

      } else {
        //Everyone else sends a msg_dirman
        url = msg_dirman::ExtractURL(msg);

        switch (req_type) {
          case RequestType::GetInfo:
            dirman::GetLocalDirectoryInfo(url, &result_dir_info);
            break;
          case RequestType::JoinDir:
            dirman::JoinDirWithName(url, "", &result_dir_info);
            break;
          case RequestType::LeaveDir:
            dirman::LeaveDir(url, &result_dir_info);
            break;
          case RequestType::DropDir:
            dirman::DropDir(url);
            break;
          default: //Unknown?
            throw std::invalid_argument("Unknown case condition");
        }
      }

      //Always send a reply
      msg_dirman::AllocateReply(ldo_msg,
                                RequestType::ReturnDirInfo,
                                msg,
                                result_dir_info);

      opbox::net::SendMsg(peer, std::move(ldo_msg));
      return updateState(State::done, WaitingType::done_and_destroy);
    }

      //DONE
    case State::done:
      return updateState(State::done, WaitingType::done_and_destroy);

    default: KTODO("Unhandled state in OpDirManCentralized receiver Update");
  }
  return WaitingType::error;
}

} // namespace dirman
