// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_MESSAGE_HELPERS_HH
#define OPBOX_MESSAGE_HELPERS_HH

#include <cstdint>
#include <stdexcept>

#include "faodel-common/NodeID.hh"
#include "faodel-common/SerializationHelpersCereal.hh"
#include "lunasa/DataObject.hh"

#include "opbox/common/Message.hh"
#include "opbox/net/net.hh"


namespace opbox {


//Create simple request that fits entirely in the stock message_t (no body)
void AllocateStandardMessage(lunasa::DataObject &new_ldo,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t src_mailbox,
                    const uint32_t op_id, const uint16_t user_flags);


//Create simple message that puts a single string in the message_t body
bool AllocateStringMessage(lunasa::DataObject &new_ldo,
                    const faodel::nodeid_t &src_node,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t src_mailbox,
                    const mailbox_t dst_mailbox,
                    const uint32_t op_id, const uint16_t user_flags,
                    const std::string user_string);

//Create simple message that puts a single string in the message_t body
bool AllocateStringRequestMessage(lunasa::DataObject &new_ldo,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t src_mailbox,
                    const uint32_t op_id, const uint16_t user_flags,
                    const std::string user_string);

//Create simple message that puts a single string in the message_t body
bool AllocateStringReplyMessage(lunasa::DataObject &new_ldo,
                    const message_t *request_msg,
                    const uint16_t user_flags,
                    const std::string user_string);


//Extract the string out of a standard string message
std::string UnpackStringMessage(message_t *hdr);


//This template provides a way to pack a Cereal-serializable structure
//into the body of a message. This version asks for all fields in order
//to allow users to set everything (eg, forwarded message)
template<typename T>
bool AllocateCerealMessage(lunasa::DataObject &new_ldo,
                    const faodel::nodeid_t &src_node,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t src_mailbox,
                    const mailbox_t dst_mailbox,
                    const uint32_t op_id, const uint16_t user_flags,
                    const T &cereal_serializable_object){

  //Pack the object
  std::string packed_object = faodel::CerealPack<T>(cereal_serializable_object);

  //if(packed_object.size() >= 64*1024)
  //  throw std::runtime_error("AllocteCerealMessage failed because string >= 64KB");

  //Allocate a new LDO. Because it is handed to us as a pointer, we won't
  //increase/decrease the refcount.
  new_ldo = opbox::net::NewMessage(sizeof(message_t) + packed_object.size());
  auto *msg = new_ldo.GetDataPtr<message_t *>();
  msg->src = src_node;
  msg->dst = dst_node;
  msg->src_mailbox = src_mailbox;
  msg->dst_mailbox = dst_mailbox;
  msg->op_id = op_id;
  msg->user_flags = user_flags;
  msg->body_len =  static_cast<uint32_t>(packed_object.size());

  //Append the packed data to the end of the message
  packed_object.copy(&msg->body[0], packed_object.size());

  return (packed_object.size()  > MESSAGE_BODY_MTU);
}


//This template provides a way to pack a Cereal-serializable structure
//into the body of a message.
template<typename T>
bool AllocateCerealRequestMessage(lunasa::DataObject &new_ldo,
                    const faodel::nodeid_t &dst_node, const mailbox_t src_mailbox,
                    const uint32_t op_id, const uint16_t user_flags,
                    const T &cereal_serializable_object){

  //Pack the object
  std::string packed_object = faodel::CerealPack<T>(cereal_serializable_object);

  //if(packed_object.size() >= 64*1024)
  //  throw std::runtime_error("AllocteCerealRequestMessage failed because string >= 64KB");


  //Allocate a new LDO. Because it is handed to us as a pointer, we won't
  //increase/decrease the refcount.
  new_ldo = opbox::net::NewMessage(sizeof(message_t) + packed_object.size());
  auto *msg = new_ldo.GetDataPtr<message_t *>();
  msg->SetStandardRequest(dst_node, src_mailbox, op_id, user_flags,
                          static_cast<uint32_t>(packed_object.size()));

  //Append the packed data to the end of the message
  packed_object.copy(&msg->body[0], packed_object.size());

  return (packed_object.size()  > MESSAGE_BODY_MTU);
}


//Allocate a message that passes a Cereal-serializable structure in the body
//and uses a request message to populate the message headers.
template<typename T>
bool AllocateCerealReplyMessage(lunasa::DataObject &new_ldo,
                                const message_t *request_msg,
                                const uint16_t user_flags,
                                const T &cereal_serializable_object){

  //Pack the object
  std::string packed_object = faodel::CerealPack<T>(cereal_serializable_object);

  //if(packed_object.size() >= 64*1024)
  //  throw std::runtime_error("AllocteCerealReplyMessage failed because string >= 64KB");

  //Allocate a new LDO. Because it is handed to us as a pointer, we won't
  //increase/decrease the refcount.
  new_ldo = opbox::net::NewMessage(sizeof(message_t) + packed_object.size());
  auto *msg = new_ldo.GetDataPtr<message_t *>();

  msg->SetStandardReply(request_msg, user_flags, static_cast<uint32_t>(packed_object.size()));

  //Append the packed data to the end of the message
  packed_object.copy(&msg->body[0], packed_object.size());

  return (packed_object.size()  > MESSAGE_BODY_MTU);
}

//Unpack a message that sent along a Cereal-packed data structure
template<typename T>
T UnpackCerealMessage(const message_t *hdr){
  std::string s(hdr->body, hdr->body_len);
  return faodel::CerealUnpack<T>(s);
}

} // namespace opbox

#endif // OPBOX_MESSAGE_HELPERS_HH
