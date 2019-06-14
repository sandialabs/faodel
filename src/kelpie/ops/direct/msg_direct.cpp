// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h>

#include "kelpie/ops/direct/msg_direct.hh"

using namespace std;

namespace kelpie {


uint16_t DirectFlags::GetCommand(const opbox::message_t *msg) {
  return (msg->user_flags & CMD_MASK);
}

bool DirectFlags::IsCommand(const opbox::message_t *msg) {
  return ((msg->user_flags & FLAG_IS_COMMAND) == FLAG_IS_COMMAND);
}
bool DirectFlags::IsStatus(const opbox::message_t *msg)  {
  return ((msg->user_flags & FLAG_IS_COMMAND) == 0);
}

bool DirectFlags::CanStall(const opbox::message_t *msg) {
  return ((msg->user_flags & FLAG_CAN_STALL) == FLAG_CAN_STALL);
}
void DirectFlags::CanStall(opbox::message_t *msg, bool can_stall) {
  msg->user_flags = (msg->user_flags & ~FLAG_CAN_STALL) | ((can_stall)?FLAG_CAN_STALL:0);
}
void DirectFlags::Success(opbox::message_t *msg, bool is_success) {
  msg->user_flags = (msg->user_flags & ~FLAG_IS_SUCCESS) | ((is_success)?FLAG_IS_SUCCESS:0);
}
bool DirectFlags::Success(const opbox::message_t *msg) {
  return ((msg->user_flags & FLAG_IS_SUCCESS) == FLAG_IS_SUCCESS);
}


void msg_direct_buffer_t::SetLDO(lunasa::DataObject *ldo_data) {
  meta_plus_data_size = (ldo_data) ? (ldo_data->GetMetaSize()+ldo_data->GetDataSize()) : 0;
  if(meta_plus_data_size){
    //Convert to local nbr
    net::NetBufferLocal  *nbl = nullptr;
    net::GetRdmaPtr(ldo_data, &nbl, &net_buffer_remote);
  } else {
    //no reference
    memset(&net_buffer_remote, 0, sizeof(net::NetBufferRemote));
  }
}

bool
msg_direct_buffer_t::Alloc(lunasa::DataObject &ldo_msg, const uint32_t op_id, const uint16_t command_and_flags, const faodel::nodeid_t dst,
                           const opbox::mailbox_t src_mailbox, const opbox::mailbox_t dst_mailbox, const faodel::bucket_t bucket,
                           const kelpie::Key &key, const kelpie::iom_hash_t iom_hash, const kelpie::pool_behavior_t behavior_flags,
                           lunasa::DataObject *ldo_data) {

  //Allocate the message
  ldo_msg = net::NewMessage(sizeof(msg_direct_buffer_t)+key.size());

  //Get a pointer we can work with
  auto *msg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();

  //Fill in our local nbr reference for the target to use
  msg->SetLDO(ldo_data);

  //Fill in identity info
  msg->k1_size = key.k1_size();
  msg->k2_size = key.k2_size();
  msg->bucket = bucket;
  msg->iom_hash = iom_hash;
  msg->behavior_flags = behavior_flags;

  //Set the header now that we know our key sizes
  msg->hdr.SetStandardRequest(dst, src_mailbox, op_id, command_and_flags);
  msg->hdr.dst_mailbox = dst_mailbox;

  //Fix the header length
  msg->hdr.body_len = (sizeof(msg_direct_buffer_t) - sizeof(opbox::message_t)) +
                      msg->k1_size + msg->k2_size;

  //Pack the key data into the end
  memcpy(&msg->key_data[0],            key.K1().c_str(), msg->k1_size);
  memcpy(&msg->key_data[msg->k1_size], key.K2().c_str(), msg->k2_size);

  return (ldo_msg.GetWireSize() > MESSAGE_MTU);
}


kelpie::Key msg_direct_buffer_t::ExtractKey(){
  kelpie::Key key( std::string(&key_data[0],       k1_size),
                   std::string(&key_data[k1_size], k2_size) );
  if(!key.valid()){
    throw std::invalid_argument("msg_direct_request had an invalid key");
  }
  return key;
}

std::string msg_direct_buffer_t::str() {
  std::stringstream ss;
  kelpie::Key key = ExtractKey();

  ss<<"msg_direct_buffer_t :"
    <<"\n    meta+data_size "<<meta_plus_data_size
    <<"\n    k1_size        "<<k1_size
    <<"\n    k2_size        "<<k2_size
    <<"\n    bucket         "<<bucket.GetHex()
    <<"\n    key            "<<key.str()
    <<"\n";
  return ss.str();

}



std::string msg_direct_status_t::str() {
  std::stringstream ss;

  ss<<"msg_direct_status :"
    <<"\n    user_flag           "<<std::hex<<hdr.user_flags
    <<"\n    remote_rc           "<<remote_rc
    //TODO: row/col info
    <<"\n";
  return ss.str();

}



msg_direct_status_t * msg_direct_status_t::Alloc(
                    lunasa::DataObject &ldo_msg,
                    message_t *incoming_msg_hdr,
                    uint16_t user_flags){

  //Allocate the message
  ldo_msg = net::NewMessage(sizeof(msg_direct_status_t));

  //Get a pointer we can work with
  auto msg = ldo_msg.GetDataPtr<msg_direct_status_t *>();

  //Wipe all fields.. probably just need hdr and target rc
  memset(msg, 0, sizeof(msg_direct_status_t));

  //Populate header
  msg->hdr.SetStandardReply(incoming_msg_hdr,
                    user_flags,
                    sizeof(msg_direct_status_t)-sizeof(message_t));

  return msg;
}





}  // namespace kelpie
