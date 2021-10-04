// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_MSG_DIRECT_HH
#define KELPIE_MSG_DIRECT_HH

#include "faodel-common/Common.hh"

#include "opbox/OpBox.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"


namespace kelpie {

// The Direct ops use three kinds of messages:
//  - buffer : for sending a buffer handle, bucket, and key to remote
//  - status : for passing back ack/nacks and info about a k/v
//  - listreply : custom cereal-packed message with info for reply messages
//
// The following messages are sent during different types of
//
// publish
//   origin: sends a BUFFER message with CMD_PUBLISH, bucket, key, and buffer
//   target: does a get rdma transfer (if not already available)
//   target: sends a STATUS message with Success flag set
//
// get bounded (user knows length)
//   origin: sends a BUFFER message with CMD_GET_BOUNDED, bucket, key, and buffer
//   target: does a put rdma transfer (after waiting for it to be published)
//   target: sends a STATUS message with Success flag set if success
//
// get unbounded (user doesn't know length)
//   origin: sends a BUFFER message with CMD_GET_UNBOUNDED, bucket, key (no buffer)
//   target: sends a BUFFER message with CMD_GET_UNBOUNDED with nbr, when available
//   origin: does a get rdma transfer
//   origin: sends a STATUS message with Success flag to notify of completion
//
// meta-colinfo
//   origin: sends a BUFFER message with CMD_GET_COLINFO
//   target: sends a STATUS message with col_info populated
//   origin: relays back to user via callback
//
// meta-rowinfo
//   origin: sends a BUFFER message with CMD_GET_ROWINFO
//   target: sends a STATUS message with row_info populated
//   origin: relays back to user via callback
//
// list: get a list of all objects and their lengths that match a search string
//    origin: sends BUFFER message with CMD_LIST to one or more targets
//    target: sends custom cereal message with all the info packed into a message
//    origin: gathers all responses. Ends when all collected.
//
// drop: specify node should drop all objects matching a search string
//    origin: sends BUFFER message with CMD_DROP to one or more targets
//    target: optional send STATUS message with Ack/Nack info
//    origin: gathers all responses. Ends when all collected.
//
// USER_FLAGS
//   OpDirect uses the message USER_FLAGS to pass along info.
//     0x0080 : Command message type mask (0x0000=status msg, 0x0080=buffer msg)
//     0x00F0 : Command mask (eg, CMD_PUBLISH, CMD_GET_BOUNDED, etc)
//     0x0002 : Request should stall until complete (0x0002=stall, 0x0000=return immediately)
//     0x0001 : Status success mask ( 0x0001=success, 0x0000=failure)


/**
 * @brief A Structure for holding constants that are used in Direct messages
 */
struct DirectFlags {
  static constexpr uint16_t CMD_MASK          = 0x00F0;

  static constexpr uint16_t CMD_COMPUTE       = 0x0080;

  static constexpr uint16_t CMD_PUBLISH       = 0x0090;
  static constexpr uint16_t CMD_GET_BOUNDED   = 0x00A0;
  static constexpr uint16_t CMD_GET_UNBOUNDED = 0x00B0;

  static constexpr uint16_t CMD_GET_COLINFO   = 0x00C0;
  static constexpr uint16_t CMD_GET_ROWINFO   = 0x00D0;
  static constexpr uint16_t CMD_LIST          = 0x00E0;
  static constexpr uint16_t CMD_DROP          = 0x00F0;

  static constexpr uint16_t CMD_STATUS_ACK    = 0x0011;
  static constexpr uint16_t CMD_STATUS_NACK   = 0x0010;

  static constexpr uint16_t FLAG_IS_COMMAND   = 0x0080;
  static constexpr uint16_t FLAG_CAN_STALL    = 0x0002;
  static constexpr uint16_t FLAG_IS_SUCCESS   = 0x0001;

  static uint16_t GetCommand(const opbox::message_t *msg);
  static bool IsCommand(const opbox::message_t *msg);
  static bool IsStatus(const opbox::message_t *msg);
  static bool CanStall(const opbox::message_t *msg);
  static void CanStall(opbox::message_t *msg, bool can_stall);
  static void Success(opbox::message_t *msg, bool is_success);
  static bool Success(const opbox::message_t *msg);
  static bool IsAck(const opbox::message_t *msg);
  static bool IsNack(const opbox::message_t *msg);
};


//We use array[0] notation in structs, so we have to tell compiler it's ok
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"


/**
 * @brief Message format for sending commands in Direct Messages that don't need to pass a remote_buffer_pointer
 * @note Actual commands are stored in the user_flags section of the header
 * @note The string section of the message may hold the key and remote function name/args
 * Key data is manually packed into the key_data section at the end of
 *       the message, so you MUST use the Alloc() function to create a new message
 */
struct msg_direct_simple_t {
  opbox::message_t                   hdr;                         //!< Standard header field

  uint64_t                           meta_plus_data_size;         //!< Used on remote end to allocate LDO for data
  uint16_t                           k1_size;                     //!< Used for serdes of key.k1
  uint16_t                           k2_size;                     //!< Used for serdes of key.k2
  faodel::bucket_t                   bucket;                      //!< Hashed bucket id
  iom_hash_t                         iom_hash;                    //!< Hash of the IOM to use
  pool_behavior_t                    behavior_flags;              //!< Flags specifying actions to take
  uint8_t                            function_name_size;          //!< Length of function name for compute
  uint16_t                           function_args_size;          //!< Length of function args for compute

  char                               string_data[0];              //!< Place where additional packed key data is stored

  //Since key_data is variable prevent user from using an empty ctor, as
  //the Set() operation would overflow. We expect Op to allocate the space
  //in an LDO, so we don't have any ctors here.
  msg_direct_simple_t()=delete;

  uint16_t GetCommand() const         { return DirectFlags::GetCommand(&hdr); }
  bool IsStatus() const               { return DirectFlags::IsStatus(&hdr); }
  void CanStall(bool can_stall)       { return DirectFlags::CanStall(&hdr, can_stall); } //setter
  bool CanStall() const               { return DirectFlags::CanStall(&hdr); } //getter

  kelpie::Key ExtractKey() const;
  void ExtractComputeArgs(kelpie::Key *key, std::string *function_name, std::string *function_args) const;
  std::string str();


  static bool Alloc(lunasa::DataObject &new_ldo,                     //!< New LDO generated for holding this message
                    const uint32_t op_id,                            //!< Which op this is for
                    const uint16_t command_and_flags,                //!< The command/flags users wants to set
                    const faodel::nodeid_t dst,                      //!< Where this message is going
                    const opbox::mailbox_t src_mailbox,              //!< What our mailbox should be
                    const opbox::mailbox_t dst_mailbox,              //!< Destination mailbox (if a response)
                    const faodel::bucket_t bucket,                   //!< Hashed bucket id for message
                    const kelpie::Key &key,                          //!< The key for this request
                    const kelpie::iom_hash_t iom_hash,               //!< Optional IO Module Hash id associated with this op
                    const kelpie::pool_behavior_t behavior_flags,    //!< Behavior settings for this transfer
                    const std::string &function_name="",             //!< String name of function to use (only in Compute ops)
                    const std::string &function_args=""              //!< String of args to pass (only in Compute ops)
  );

  //note: rdma address lookups may make ldo_data non const TODO
};



/**
 * @brief Message format for sending commands in Direct messages involving keys or rdma pointers
 * @note Actual commands are stored in the user_flags section of the header
 * @note Key data is manually packed into the key_data section at the end of
 *       the message, so you MUST use the Alloc() function to create a new message
 */
struct msg_direct_buffer_t {
  opbox::message_t                   hdr;                         //!< Standard header field

  struct net::NetBufferRemote        net_buffer_remote;           //!< RDMA pointers the receiver can use to put/get
  uint64_t                           meta_plus_data_size;         //!< Used on remote end to allocate LDO for data
  uint16_t                           k1_size;                     //!< Used for serdes of key.k1
  uint16_t                           k2_size;                     //!< Used for serdes of key.k2
  faodel::bucket_t                   bucket;                      //!< Hashed bucket id
  iom_hash_t                         iom_hash;                    //!< Hash of the IOM to use
  pool_behavior_t                    behavior_flags;              //!< Flags specifying actions to take

  char                               string_data[0];              //!< Place where additional packed key data is stored

  //Since key_data is variable prevent user from using an empty ctor, as
  //the Set() operation would overflow. We expect Op to allocate the space
  //in an LDO, so we don't have any ctors here.
  msg_direct_buffer_t()=delete;

  uint16_t GetCommand() const         { return DirectFlags::GetCommand(&hdr); }
  bool IsStatus() const               { return DirectFlags::IsStatus(&hdr); }
  void CanStall(bool can_stall)       { return DirectFlags::CanStall(&hdr, can_stall); } //setter
  bool CanStall() const               { return DirectFlags::CanStall(&hdr); } //getter

  void SetLDO(lunasa::DataObject *ldo_data);

  kelpie::Key ExtractKey() const;
  std::string str();


  static bool Alloc(lunasa::DataObject &new_ldo,                     //!< New LDO generated for holding this message
                    const uint32_t op_id,                            //!< Which opbox op this is for
                    const uint16_t command_and_flags,                //!< The command/flags users wants to set
                    const faodel::nodeid_t dst,                      //!< Where this message is going
                    const opbox::mailbox_t src_mailbox,              //!< What our mailbox should be
                    const opbox::mailbox_t dst_mailbox,              //!< Destination mailbox (if a response)
                    const faodel::bucket_t bucket,                   //!< Hashed bucket id for message
                    const kelpie::Key &key,                          //!< The key for this request
                    const kelpie::iom_hash_t iom_hash,               //!< Optional IO Module Hash id associated with this op
                    const kelpie::pool_behavior_t  behavior_flags,   //!< Behavior settings for this transfer
                    lunasa::DataObject *ldo_data                     //!< The ldo we're using for data (or nullptr if none)
  );

  //note: rdma address lookups may make ldo_data non const TODO

};




/**
 * @brief Provide a short status message back to a sender with row/col info
 * @note Call an Alloc function to create an appropriately-sized LDO and
 *       get a cast of it.
 */
struct msg_direct_status_t {
  opbox::message_t                   hdr;                         //!< Standard header field
  int                                remote_rc;                   //!< Return code seen at the other node
  object_info_t                      object_info;                 //!< Statistics about this object's row/column

  bool IsStatus()               { return DirectFlags::IsStatus(&hdr); }
  void Success(bool is_success) { return DirectFlags::Success(&hdr, is_success); }
  bool Success()                { return DirectFlags::Success(&hdr); }

  std::string str();

  msg_direct_status_t()=delete; //Intentionally disable. Call an Alloc instead

  static msg_direct_status_t * Alloc(
                    lunasa::DataObject &new_ldo_ptr,              //!< New LDO generated for holding this message
                    message_t *origin_msg_hdr,                    //!< Original request to reference
                    uint16_t user_flags                           //!< Custom flags to set (overwrites)
                    );
  static msg_direct_status_t * AllocAck(
                    lunasa::DataObject &new_ldo_ptr,              //!< New LDO generated for holding this message
                    message_t *origin_msg_hdr                     //!< Original request to reference
                    ) {
    return Alloc(new_ldo_ptr, origin_msg_hdr, DirectFlags::CMD_STATUS_ACK);
  }

  static msg_direct_status_t * AllocNack(
                    lunasa::DataObject &new_ldo_ptr,              //!< New LDO generated for holding this message
                    message_t *origin_msg_hdr                     //!< Original request to reference
                    ) {
    return Alloc(new_ldo_ptr, origin_msg_hdr, DirectFlags::CMD_STATUS_NACK);
  }


};

#pragma GCC diagnostic pop //For ignoring array[0] kinds of allocation

}  // namespace kelpie

#endif  // KELPIE_MSG_DIRECT_HH
