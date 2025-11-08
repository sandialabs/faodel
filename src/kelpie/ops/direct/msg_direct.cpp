// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
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
bool DirectFlags::IsAck(const opbox::message_t *msg) {
  return ((msg->user_flags & CMD_STATUS_ACK)==CMD_STATUS_ACK);
}
bool DirectFlags::IsNack(const opbox::message_t *msg) {
  return ((msg->user_flags & CMD_STATUS_ACK)==CMD_STATUS_NACK);
}

bool
msg_direct_simple_t::Alloc(lunasa::DataObject &ldo_msg, const uint32_t op_id, const uint16_t command_and_flags, const faodel::nodeid_t dst,
                           const opbox::mailbox_t src_mailbox, const opbox::mailbox_t dst_mailbox, const faodel::bucket_t bucket,
                           const kelpie::Key &key, const kelpie::iom_hash_t iom_hash, const kelpie::pool_behavior_t behavior_flags,
                           const std::string &function_name, const std::string &function_args) {

  size_t string_size = key.size() + function_name.size() + function_args.size();

  //Allocate the message
  ldo_msg = net::NewMessage(sizeof(msg_direct_simple_t)+string_size);

  //Get a pointer we can work with
  auto *msg = ldo_msg.GetDataPtr<msg_direct_simple_t *>();

  //Fill in identity info
  msg->k1_size = key.k1_size();
  msg->k2_size = key.k2_size();
  msg->bucket = bucket;
  msg->iom_hash = iom_hash;
  msg->behavior_flags = behavior_flags;
  msg->function_name_size = (function_name.size()) & 0x0FF;
  msg->function_args_size = (function_args.size()) & 0x0FFFF;

  //Set the header now that we know our key sizes
  msg->hdr.SetStandardRequest(dst, src_mailbox, op_id, command_and_flags);
  msg->hdr.dst_mailbox = dst_mailbox;

  //Fix the header length
  msg->hdr.body_len = (sizeof(msg_direct_simple_t) - sizeof(opbox::message_t) + string_size);

  //Append all our string data at the end
  char *ptr = &msg->string_data[0];
  memcpy(ptr, key.K1().c_str(), msg->k1_size); ptr += msg->k1_size;
  memcpy(ptr, key.K2().c_str(), msg->k2_size); ptr += msg->k2_size;
  memcpy(ptr, function_name.c_str(), msg->function_name_size); ptr += msg->function_name_size;
  memcpy(ptr, function_args.c_str(), msg->function_args_size);

  return (ldo_msg.GetWireSize() > MESSAGE_MTU);
}

kelpie::Key msg_direct_simple_t::ExtractKey() const {
  kelpie::Key key( std::string(&string_data[0],       static_cast<size_t>(k1_size)),
                   std::string(&string_data[k1_size], static_cast<size_t>(k2_size)) );
  if(!key.valid()){
    throw std::invalid_argument("msg_direct_request had an invalid key");
  }
  return key;
}

/**
 * @brief Parse this message and extract the Key, Function Name, and Function Args
 * @param[out] key Kelpie Key
 * @param[out] function_name The string value of the function
 * @param[out] function_args The string arguments to pass to the compute function
 */
void msg_direct_simple_t::ExtractComputeArgs(kelpie::Key *key,
                                             string *function_name, string *function_args)  const {
  string s1,s2;
  const char *ptr = &string_data[0];
  s1 = std::string( ptr, static_cast<size_t>(k1_size)); ptr+=k1_size;
  s2 = std::string( ptr, static_cast<size_t>(k2_size)); ptr+=k2_size;
  *key = kelpie::Key( s1, s2);
  if(!key->valid()) {
    throw std::invalid_argument("msg_direct_request had an invalid key");
  }

  *function_name = std::string( ptr, static_cast<size_t>(function_name_size)); ptr+=function_name_size;
  *function_args = std::string( ptr, static_cast<size_t>(function_args_size));

}

std::string msg_direct_simple_t::str() {

  kelpie::Key key;
  string function_name, function_args;
  ExtractComputeArgs(&key, &function_name, &function_args);

  std::stringstream ss;
  ss<<"msg_direct_simple_t :"
    <<"\n    meta+data_size "<<meta_plus_data_size
    <<"\n    k1_size        "<<k1_size
    <<"\n    k2_size        "<<k2_size
    <<"\n    bucket         "<<bucket.GetHex()
    <<"\n    key            "<<key.str()
    <<"\n    function_name  "<<function_name
    <<"\n    function_args  "<<function_args
    <<"\n";
  return ss.str();

}



/**
 * @brief Allocate a kelpie direct communication message in a situation where a buffer pointer must be sent to the remote side
 * @param[out] ldo_msg  A new buffer returned to the user, populated with all the info supplied to this cal
 * @param[in] op_id Which opbox Op this is for (eg OpKelpieList.op_id. which is a hash of "OpKelpieList" )
 * @param[in] command_and_flags DirectFlags that specify what this message is for
 * @param[in] dst The node id for where this message is going
 * @param[in] src_mailbox What our mailbox is for this message
 * @param[in] dst_mailbox  The mailbox to use at the destination if this is a reply (0 if new message)
 * @param[in] bucket Hashed bucket id for this key
 * @param[in] key The Kelpie Key for this request
 * @param[in] iom_hash Optional I/O Module hash id for this op
 * @param[in] behavior_flags Any behavior flags needed for this op
 * @param[in] ldo_data A local buffer to share with remote node, Nullptr for none
 * @return
 */
bool
msg_direct_buffer_t::Alloc(lunasa::DataObject &ldo_msg, const uint32_t op_id, const uint16_t command_and_flags, const faodel::nodeid_t dst,
                           const opbox::mailbox_t src_mailbox, const opbox::mailbox_t dst_mailbox, const faodel::bucket_t bucket,
                           const kelpie::Key &key, const kelpie::iom_hash_t iom_hash, const kelpie::pool_behavior_t behavior_flags,
                           lunasa::DataObject *ldo_data) {

  size_t string_size = key.size();

  //Allocate the message
  ldo_msg = net::NewMessage(sizeof(msg_direct_buffer_t)+string_size);

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
  msg->hdr.body_len = (sizeof(msg_direct_buffer_t) - sizeof(opbox::message_t) + string_size);

  //Append the key to the string section
  char *ptr = &msg->string_data[0];
  memcpy(ptr, key.K1().c_str(), msg->k1_size); ptr += msg->k1_size;
  memcpy(ptr, key.K2().c_str(), msg->k2_size); ptr += msg->k2_size;

  return (ldo_msg.GetWireSize() > MESSAGE_MTU);
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

kelpie::Key msg_direct_buffer_t::ExtractKey() const {
  kelpie::Key key( std::string(&string_data[0],       static_cast<size_t>(k1_size)),
                   std::string(&string_data[k1_size], static_cast<size_t>(k2_size)) );
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
                    uint16_t user_flags) {

  //Allocate the message
  ldo_msg = net::NewMessage(sizeof(msg_direct_status_t));

  //Get a pointer we can work with
  auto msg = ldo_msg.GetDataPtr<msg_direct_status_t *>();

  //Wipe all fields.. probably just need hdr and target rc
  memset((void *)msg, 0, sizeof(msg_direct_status_t));

  //Populate header
  msg->hdr.SetStandardReply(incoming_msg_hdr,
                    user_flags,
                    sizeof(msg_direct_status_t)-sizeof(message_t));

  return msg;
}





}  // namespace kelpie
