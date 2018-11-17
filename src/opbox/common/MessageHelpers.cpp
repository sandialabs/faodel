// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <stdexcept>

#include "opbox/net/net.hh"
#include "opbox/common/Message.hh"

using namespace std;

namespace opbox {


/**
 * @brief Create a new ldo for a message and set the standard header fields
 *
 * @param[out] new_ldo     A new ldo that will store the outgoing message allocation
 * @param[in] dst_node     The nodeid where this message will be going
 * @param[in] src_mailbox  The origin's mailbox that the target should reply to
 * @param[in] op_id        The uniqie identifier for this type of op
 * @param[in] user_flags   Any 16b flags a user may want to pass in the message header
 */
void AllocateStandardMessage(lunasa::DataObject &new_ldo,
                                   const faodel::nodeid_t &dst_node,
                                   const mailbox_t src_mailbox,
                                   const uint32_t op_id,
                                   const uint16_t user_flags){

  new_ldo = opbox::net::NewMessage(sizeof(message_t));
  auto *msg = new_ldo.GetDataPtr<message_t *>();
  msg->SetStandardRequest(dst_node, src_mailbox, op_id, user_flags, 0);
}

/**
 * @brief Create a new ldo for a message that has a single-string body
 *
 * @param[out] new_ldo      A new ldo that will store the outgoing message allocation
 * @param[in]  dst_node     The nodeid where this message will be going
 * @param[in]  src_mailbox  The origin's mailbox that the target should reply to
 * @param[in]  op_id        The uniqie identifier for this type of op
 * @param[in]  user_flags   Any 16b flags a user may want to pass in the message header
 * @param[in]  string       The (possibly binary) string that makes up the body of the message
 */
bool AllocateStringMessage(lunasa::DataObject &new_ldo,
                                   const faodel::nodeid_t &src_node,
                                   const faodel::nodeid_t &dst_node,
                                   const mailbox_t src_mailbox,
                                   const mailbox_t dst_mailbox,
                                   const uint32_t op_id, const uint16_t user_flags,
                                   const string user_string){

  if(user_string.size() >= 64*1024)
    throw std::runtime_error("AllocteStringMessage failed because string >= 64KB");

  new_ldo = opbox::net::NewMessage(sizeof(message_t) + user_string.size());

  auto *msg = new_ldo.GetDataPtr<message_t *>();
  msg->src = src_node;
  msg->dst = dst_node;
  msg->src_mailbox = src_mailbox;
  msg->dst_mailbox = dst_mailbox;
  msg->op_id = op_id;
  msg->user_flags = user_flags;
  msg->body_len =  static_cast<uint16_t>(user_string.size());

  //Append the packed data to the end of the message
  user_string.copy(msg->body, user_string.size());

  return (user_string.size()  > MESSAGE_BODY_MTU);
}


/**
 * @brief Create a new ldo for a message that has a single-string body
 *
 * @param[out] new_ldo      A new ldo that will store the outgoing message allocation
 * @param[in]  dst_node     The nodeid where this message will be going
 * @param[in]  src_mailbox  The origin's mailbox that the target should reply to
 * @param[in]  op_id        The uniqie identifier for this type of op
 * @param[in]  user_flags   Any 16b flags a user may want to pass in the message header
 * @param[in]  string       The (possibly binary) string that makes up the body of the message
 */
bool AllocateStringRequestMessage(lunasa::DataObject &new_ldo,
                                   const faodel::nodeid_t &dst_node,
                                   const mailbox_t src_mailbox,
                                   const uint32_t op_id,
                                   const uint16_t user_flags,
                                   const string user_string){

  if(user_string.size() >= 64*1024)
    throw std::runtime_error("AllocteStringRequestMessage failed because string >= 64KB");


  new_ldo = opbox::net::NewMessage(sizeof(message_t) + user_string.size());

  auto *msg = new_ldo.GetDataPtr<message_t *>();
  msg->SetStandardRequest(dst_node, src_mailbox, op_id, user_flags, user_string.size());
  user_string.copy(msg->body, user_string.size());

  return (user_string.size() > MESSAGE_BODY_MTU);
}

/**
 * @brief Create a new ldo for a reply message that has a single-string body
 *
 * @param[out] new_ldo      A new ldo that will store the outgoing message allocation
 * @param[in]  dst_node     The nodeid where this message will be going
 * @param[in]  src_mailbox  The origin's mailbox that the target should reply to
 * @param[in]  op_id        The uniqie identifier for this type of op
 * @param[in]  user_flags   Any 16b flags a user may want to pass in the message header
 * @param[in]  string       The (possibly binary) string that makes up the body of the message
 */
bool AllocateStringReplyMessage(lunasa::DataObject &new_ldo,
                                   const message_t *request_msg,
                                   const uint16_t user_flags,
                                   const string user_string){

  if(user_string.size() >= 64*1024)
    throw std::runtime_error("AllocteStringReplyMessage failed because string >= 64KB");

  new_ldo = opbox::net::NewMessage(sizeof(message_t) + user_string.size());

  auto *msg = new_ldo.GetDataPtr<message_t *>();

  msg->SetStandardReply(request_msg, user_flags, static_cast<uint16_t>(user_string.size()));
  user_string.copy(msg->body, user_string.size());

  return (user_string.size() > MESSAGE_BODY_MTU);
}


/**
 * @brief Extract the string that resides in the body of a standard string message
 *
 * @param[in] hdr A pointer to the incoming message
 * @return std::string The (possibly binary) string that was stored in the body
 */
std::string UnpackStringMessage(message_t *hdr){
  std::string s(hdr->body, hdr->body_len);
  return s;
}

} // namespace opbox
