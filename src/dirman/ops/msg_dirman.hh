// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef DIRMAN_MSG_DIRMAN_HH
#define DIRMAN_MSG_DIRMAN_HH

#include "dirman/ops/OpDirManCentralized.hh"

namespace dirman {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

/**
 * @brief A message data structure used by dirman ops
 *
 * In addition to the standard opbox message header, a dirman message contains
 * is a URL string or a cereal-packed DirInfo. Flags are stored in the hdr_flags
 * of the message header.
 *
 * A user is expected to call the Allocate(Request/Reply) functions to generate
 * an LDO for storing a new message.
 */
typedef struct msg_dirman {
  opbox::message_t hdr;

  //Body: The body section following hdr contains either:
  //        - a url string
  //        - a cereal-packed DirInfo
  //      bit[4] of hdr_flags specifies which type. Make sure your op id's agree

  msg_dirman()=delete;

  bool hasDirInfo();
  bool hasURL();
  faodel::DirectoryInfo ExtractDirInfo() { return msg_dirman::ExtractDirInfo(&hdr); }


  //For URL messages. They're only used in requests, so no need to reply
  static bool AllocateRequest(
                    lunasa::DataObject &new_ldo,
                    const OpDirManCentralized::RequestType &req_type,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t &src_mailbox,
                    const faodel::ResourceURL &url);

  static faodel::ResourceURL ExtractURL(message_t *hdr);


  //For DirInfo messages
  static bool AllocateRequest(
                    lunasa::DataObject &new_ldo,
                    const OpDirManCentralized::RequestType &req_type,
                    const faodel::nodeid_t &dst_node,
                    const mailbox_t &src_mailbox,
                    const faodel::DirectoryInfo &dir_info);

  static bool AllocateReply(
                    lunasa::DataObject &new_ldo,
                    const OpDirManCentralized::RequestType &req_type,
                    const message_t *request_msg,
                    const faodel::DirectoryInfo &dir_info);

  static faodel::DirectoryInfo ExtractDirInfo(message_t *hdr);



} msg_dirman_t;

#pragma GCC diagnostic pop

} // namespace dirman


#endif // DIRMAN_MSG_DIRMAN_HH
