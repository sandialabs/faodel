// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_MESSAGEHEADER_HH
#define OPBOX_MESSAGEHEADER_HH

#include <stdint.h>


#include "faodel-common/NodeID.hh"
#include "lunasa/DataObject.hh"




namespace opbox {
typedef uint32_t mailbox_t;             //!< A unique, node-specific id for an Op instance
const mailbox_t MAILBOX_UNSPECIFIED(0); //!< Designates a mailbox has not been assigned for an op

//Prototypes
namespace net {
  faodel::nodeid_t GetMyID();
  opbox::mailbox_t GetNextMailbox();
} // end namespace opbox::net


/**
 * @brief Provides a basic header for all OpBox messages. User may place addition info in body
 *
 * This struct provides a basic header structure for messages transmitted in
 * opbox. It provides basic sender/receiver info as well as an ID for
 * specifying which type of registered operation this is. Users can put
 * 16 bits of flag info a user_flags variable.
 *
 * If you need to send additional info in this message, you can use the
 * body section of this message. The idea is that you create a struct of
 * your own and insert a message_t at the front of it. You'll need to
 * update the body_len of the message_t to reflect how much extra space
 * your structure requires. If it's simple data (eg, just a string), you
 * can just put it at body and avoid creating a new struct for every
 * message you make.
 *
 * note: This is intentionally a C way of doing things. The idea is to
 * make messages easy to cast when dealing with raw memory.
 */
struct message_t {

  faodel::nodeid_t  src;          //!< The origin's id (only used by the op)
  faodel::nodeid_t  dst;          //!< The target this message is going to (only used by the op. Can be zero)
  mailbox_t         src_mailbox;  //!< ID to use when communicating back with origin
  mailbox_t         dst_mailbox;  //!< ID to use at the target (usually 0 for most ops)
  uint32_t          op_id;        //!< The id for this type of op (a hash of its name)
  uint16_t          user_flags;   //!< small place for user to put simple flags
  uint16_t          body_len;     //!< Length of this message's body (should be less than MTU - sizeof(message_t))

  //Body follows next. We use the non-compliant zero-length array here to make pointers easier
  char              body[0];      //!< Starting point for any other op-specific data in this message

  std::string GetBodyAsString() const;

  //Tests to see if this message is what we expected
  bool IsExpected(uint32_t expected_op_id);
  bool IsExpected(uint32_t expected_op_id, uint16_t expected_flags);
  bool IsExpected(uint32_t expected_op_id, uint16_t flag_mask, uint16_t expected_flags);

  //Helpers for filling in standard request/reply fields
  void SetStandardRequest(faodel::nodeid_t dst, mailbox_t src_mailbox, uint32_t op_id, uint16_t user_flags=0, uint16_t body_len=0);
  void SetStandardReply(const message_t *hdr, uint16_t user_flags=0, uint16_t body_len=0);


  //Provide InfoInterface w/o inheritance (which would add an extra 8 bytes to structure)
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;
  std::string str(int depth=0, int indent=0) const;

};


const uint32_t MESSAGE_MTU = 2048;                                   //!< Maximum Transfer Unit for underlying network
const uint32_t MESSAGE_BODY_MTU = (MESSAGE_MTU - sizeof(message_t)); //!< Maximum size the body can be for an MTU

} // namespace opbox

#endif // OPBOX_MESSAGEHEADER_HH
