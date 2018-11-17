// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPDIRMANCENTRALIZED_HH
#define OPBOX_OPDIRMANCENTRALIZED_HH

#include <future>


#include "opbox/services/dirman/DirectoryInfo.hh"
#include "opbox/ops/Op.hh"

namespace opbox {

/**
 * @brief This Op is used to do all the remote requests in the centralized DirMan
 *
 * The Centralized DirMan service implements a simple client/server protocol
 * for manipulating DirMan data. This Op handles several functions including:
 *    - Hosting a new dirman directory
 *    - Getting info about a dirman directory
 *    - Joinging a dirman directory
 *    - Leaving a dirman directory
 *
 *  All requests result in an updated DirInfo entry being returned to the
 *  caller. This info can be retrieved through a future that is handed
 *  back by GetFuture(). GetFuture() MUST be called before launching the op.
 *
 */
class OpDirManCentralized : public Op {

  enum class State : int {
    start=0,
    snd_wait_for_reply,
    done
  };

public:
  enum class RequestType : uint8_t {
    //Note: bit4 signifies this message packs a dirInfo structure
    Invalid=0,
    HostNewDir    = 0x11,
    GetInfo       = 0x02,
    JoinDir       = 0x03,
    LeaveDir      = 0x04,
    ReturnDirInfo = 0x15
  };


public:
  //An origin can issue any of the above requests, but they involve different data
  OpDirManCentralized(RequestType req_type, faodel::nodeid_t root_id, DirectoryInfo dir_info);
  OpDirManCentralized(RequestType req_type, faodel::nodeid_t root_id, faodel::ResourceURL url);

  //A target starts off the same way no matter what command
  explicit OpDirManCentralized(op_create_as_target_t t);
  ~OpDirManCentralized() override;

  //Means for passing back the result
  std::future<DirectoryInfo> GetFuture();

  //Unique name and id for this op
  const static unsigned int op_id;
  const static std::string  op_name;
  unsigned int getOpID() const override { return op_id; }
  std::string  getOpName() const override { return op_name; }

  WaitingType UpdateOrigin(OpArgs *args) override;
  WaitingType UpdateTarget(OpArgs *args) override;

  std::string GetStateName() const override;


private:
  State state;
  opbox::net::peer_t *peer;
  lunasa::DataObject  ldo_msg;
  RequestType request_type;

  std::promise<DirectoryInfo> di_promise;

  WaitingType updateState(State new_state, WaitingType waiting_condition) {
    state=new_state;
    return waiting_condition;
  }
};

} // namespace opbox

#endif // OPBOX_OPDIRMANCREATE_HH
