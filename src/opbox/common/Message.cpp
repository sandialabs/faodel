// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>

#include "opbox/net/net.hh"
#include "opbox/common/Message.hh"
#include "opbox/common/OpArgs.hh"

using namespace std;

namespace opbox {

/**
 * @brief Pull the body section of the message out and return it as a string
 *
 * @return string version of the body
 */
string message_t::GetBodyAsString() const {
  return string(body, body_len);
}

/**
 * @brief Compare this message's op_id to an expected op identifier
 * @param[in] expected_op_id What this message's op_id should be
 */
bool message_t::IsExpected(uint32_t expected_op_id) {
  return (op_id==expected_op_id);
}

/**
 * @brief Determine if this message matches both the op identifier and user flags
 *
 * @param[in] expected_op_id What the message's op_id should be
 * @param[in] expected_flags What the message's user_flags should be
 * @return True when both fields match
 */
bool message_t::IsExpected(uint32_t expected_op_id, uint16_t expected_flags) {
  return ((op_id==expected_op_id) && (user_flags==expected_flags));
}

/**
 * @brief Determine if this message matches both the op identifier and specific flags
 *
 * @param[in] expected_op_id What the message's op_id should be
 * @param[in] flag_mask Which bits in the user flags to check
 * @param[in] expected_flags The value those masked flags should be set to
 * @return True when this message is a match
 */
bool message_t::IsExpected(uint32_t expected_op_id, uint16_t flag_mask, uint16_t expected_flags) {
  return ((op_id==expected_op_id) && ((user_flags & flag_mask)==expected_flags));
}


/**
 * @brief Fill in the standard fields for an outgoing request message
 *
 * @param[in] dst_node    The nodeid where this message will be going
 * @param[in] src_mailbox The origin's mailbox that the target should reply to
 * @param[in] op_id       The uniqie identifier for this type of op
 * @param[in] user_flags  Any 16b flags a user may want to pass in the message header
 * @param[in] body_len    The length of the data immediately following this header
 */
void message_t::SetStandardRequest(faodel::nodeid_t dst_node, mailbox_t src_mailbox,
                                 uint32_t op_id, uint16_t user_flags,
                                 uint32_t body_len){
  this->src = opbox::net::GetMyID();
  this->dst = dst_node;
  this->src_mailbox = src_mailbox;
  this->dst_mailbox = MAILBOX_UNSPECIFIED;
  this->op_id = op_id;
  this->user_flags = user_flags;
  this->body_len = body_len;
}

/**
 * @brief Build a reply message based on the data provided in a request messgae
 *
 * @param[out] hdr        Pointr to the incoming request message this message is replying to
 * @param[in]  user_flags Any 16b flags a user may want to pass in the message header
 * @param[in]  body_len   The length of the data immediately following this header
 */
void message_t::SetStandardReply(const message_t *hdr, uint16_t user_flags,
                               uint32_t body_len) {
  this->src = opbox::net::GetMyID();
  this->dst = hdr->src;
  this->src_mailbox = MAILBOX_UNSPECIFIED;
  this->dst_mailbox = hdr->src_mailbox;
  this->op_id = hdr->op_id;
  this->user_flags = user_flags;
  this->body_len = body_len;
}

/**
 * @brief Dump information about a component (and its internal components)
 *
 * @param[in] ss     The stringstream to append this component's information to
 * @param[in] depth  How many layers deeper into the component we should go
 * @param[in] indent How many spaces to indent this component
 */
void message_t::sstr(std::stringstream &ss, int depth, int indent) const {
  //Note: This code is here because inheriting from InfoInterface would add
  //      an extra 8 bytes to the message and mess up the packing.
  if(depth == 1){
    //One liner
    ss << string(indent,' ') <<"[msg] src "<<this->src.GetHex()
       << " dst "   <<this->dst.GetHex()
       << " smb "   <<this->src_mailbox
       << " dmb "   <<this->dst_mailbox
       << " opid "  <<this->op_id
       << " uflg "  <<this->user_flags
       << " blen "  <<this->body_len<<endl;

  } else {
    ss << string(indent,' ')  <<"[msg] "<<endl
       << string(indent+1,' ')<<"src:        "<<this->src.GetHex()<<endl
       << string(indent+1,' ')<<"dst:        "<<this->dst.GetHex()<<endl
       << string(indent+1,' ')<<"src_mbox:   "<<this->src_mailbox<<endl
       << string(indent+1,' ')<<"dst_mbox:   "<<this->dst_mailbox<<endl
       << string(indent+1,' ')<<"op_id:      "<<this->op_id<<endl
       << string(indent+1,' ')<<"user_flags: "<<this->user_flags<<endl
       << string(indent+1,' ')<<"body_len:   "<<this->body_len<<endl;
  }
}

/**
 * @brief Retrieve information about a component (and its internal components)
 *
 * @param[in] depth    How many layers deeper into the component we should go
 * @param[in] indent   How many layers deeper into the component we should go
 * @return std::string information about this component (and its components)
 */
std::string message_t::str(int depth, int indent) const {
  //Note: This code is here because inheriting from InfoInterface would add
  //      an extra 8 bytes to the message and mess up the packing.
  stringstream ss;
  sstr(ss, depth,indent);
  return ss.str();
}


} // namespace opbox
