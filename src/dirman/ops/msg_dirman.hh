// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_MSG_DIRMAN_HH
#define OPBOX_MSG_DIRMAN_HH

#include "opbox/services/dirman/ops/OpDirManCentralized.hh"

namespace opbox {

/**
 * @brief A message data structure used by dirman ops
 *
 * In addition to the standard opbox message header, a dirman message contains
 * is a URL string or a boost-packed DirInfo. Flags are stored in the hdr_flags
 * of the message header.
 *
 * A user is expected to call the Allocate(Request/Reply) functions to generate
 * an LDO for storing a new message.
 */
typedef struct msg_dirman {
  opbox::message_t hdr;
  //Body: The body section following hdr contains either:
  //        - a url string
  //        - a boost-packed DirInfo
  //      bit[4] of hdr_flags specifies which type. Make sure your op id's agree

  msg_dirman()=delete;

  bool hasDirInfo();
  bool hasURL();
  DirectoryInfo ExtractDirInfo() { return msg_dirman::ExtractDirInfo(&hdr); }


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
                    const DirectoryInfo &dir_info);

  static bool AllocateReply(
                    lunasa::DataObject &new_ldo,
                    const OpDirManCentralized::RequestType &req_type,
                    const message_t *request_msg,
                    const DirectoryInfo &dir_info);

  static DirectoryInfo ExtractDirInfo(message_t *hdr);



} msg_dirman_t;

} // namespace opbox


#endif // OPBOX_MSG_DIRMAN_HH
