// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <stdexcept>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "opbox/common/MessageHelpers.hh"
#include "dirman/ops/msg_dirman.hh"

using namespace std;
using namespace faodel;

namespace dirman {

/**
 * @brief Determine if this message has a DirInfo embedded in it
 */
bool msg_dirman::hasDirInfo() {
  return ((hdr.op_id == OpDirManCentralized::op_id) && (hdr.user_flags & 0x10));
}

/**
 * @brief Determine if this message has a URL embedded in it
 */
bool msg_dirman::hasURL() {
  return ((hdr.op_id == OpDirManCentralized::op_id) && !(hdr.user_flags & 0x10));
}

/**
 * @brief Allocate a new LDO and set it as a new Request messge
 *
 * @param[out] new_ldo The resulting ldo that is allocated for this message
 * @param[in] req_type The type of request this is
 * @param[in] dst_node Which node this goes to
 * @param[in] src_mailbox The sender's mailbox
 * @param[in] url The URL we are requesting
 * @retval True This fit in an MTU-sized allocation
 * @retval False This was larger than an MTU
 */
bool msg_dirman::AllocateRequest(lunasa::DataObject &new_ldo,
                                 const OpDirManCentralized::RequestType &req_type,
                                 const faodel::nodeid_t &dst_node,
                                 const mailbox_t &src_mailbox,
                                 const faodel::ResourceURL &url) {

  return AllocateStringRequestMessage(new_ldo,
                                      dst_node, src_mailbox,
                                      OpDirManCentralized::op_id,
                                      static_cast<uint16_t>(req_type),
                                      url.GetFullURL());
}

/**
 * @brief Extract the URL from a message
 * @param[in] hdr A pointer to an incoming message
 * @return url The URL in the message
 * @throws runtime_error if message does not have a url
 */
faodel::ResourceURL msg_dirman::ExtractURL(message_t *hdr) {

  if (!(reinterpret_cast<msg_dirman_t *>(hdr))->hasURL()) {
    throw std::runtime_error("ExtractURL called on a message that didn't contain a URL");
  }
  string s = UnpackStringMessage(hdr);
  return faodel::ResourceURL(s);
}

/**
 * @brief Extract a DirInfo from a message
 * @param[in] hdr A pointer to an incoming message
 * @return DirInfo A DirInfo extracted from the message
 * @throws runtime_error if message does not have a url
 */
DirectoryInfo msg_dirman::ExtractDirInfo(message_t *hdr) {
  if (!(reinterpret_cast<msg_dirman_t *>(hdr))->hasDirInfo()) {
    throw std::runtime_error("ExtractURL called on a message that didn't contain a URL");
  }
  return UnpackCerealMessage<DirectoryInfo>(hdr);
}

/**
 * @brief Allocate a new LDO and fill it with a DirInfo Request
 * @param[out] new_ldo The resulting ldo that is allocated for this message
 * @param[in] req_type The type of request this is
 * @param[in] dst_node Which node this goes to
 * @param[in] src_mailbox The sender's mailbox
 * @param[in] dir_info The DirectoryInfo the user wants to send
 * @retval True This fit in an MTU-sized allocation
 * @retval False This was larger than an MTU
 */
bool msg_dirman::AllocateRequest(lunasa::DataObject &new_ldo,
                                 const OpDirManCentralized::RequestType &req_type,
                                 const faodel::nodeid_t &dst_node,
                                 const mailbox_t &src_mailbox,
                                 const DirectoryInfo &dir_info) {


  return AllocateCerealRequestMessage<DirectoryInfo>(
          new_ldo,
          dst_node, src_mailbox,
          OpDirManCentralized::op_id,
          static_cast<uint16_t>(req_type),
          dir_info);
}

/**
 * @brief Allocate a new LDO and fill it with a reply that includes directory info
 * @param[out] new_ldo The resulting ldo that is allocated for this message
 * @param[in] req_type The type of request this is
 * @param[in] request_msg The message we're responding to (retrieve sender info)
 * @param[in] dir_info The DirectoryInfo the user wants to send
 * @retval True This fit in an MTU-sized allocation
 * @retval False This was larger than an MTU
 */
bool msg_dirman::AllocateReply(lunasa::DataObject &new_ldo,
                               const OpDirManCentralized::RequestType &req_type,
                               const message_t *request_msg,
                               const DirectoryInfo &dir_info) {

  return AllocateCerealReplyMessage<DirectoryInfo>(
          new_ldo,
          request_msg,
          static_cast<uint16_t>(req_type),
          dir_info);
}


} // namespace dirman