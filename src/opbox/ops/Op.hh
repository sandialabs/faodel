// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OP_HH
#define OPBOX_OP_HH

#include <atomic>
#include <chrono>
#include <iostream>

#include "opbox/common/Types.hh"
#include "opbox/common/Message.hh"
#include "opbox/common/OpArgs.hh"
#include "opbox/net/net.hh"

namespace opbox {


/**
 * @class Op
 * @brief A state machine class for sequencing communication between nodes
 * @details OpBox uses an **Op** to express how one or more nodes coordinate
 * the transfer of information at runtime. An Op is a user-defined **state
 * machine** that reacts to different runtime events (eg, arrival of a new
 * message, completion of an RDMA transfer, or a user-defined trigger). An
 * Op uses the following terminology:
 *
 * - **Origin** vs. **Target**: The node that originally starts an Op is the
 *   origin, while all other instances in the system are targets.
 *
 * - **Mailbox**: OpBox can assign a node-unique ID for an Op in order to
 *   allow other nodes in the system to reference it. Using the identifier
 *   MAILBOX_UNSPECIFIED (which is 0) in a message's mailbox fields means
 *   this message is for a new Op, or the sender does not expect a response.
 *
 * - **Op Name/ID**: The registration process requires each Op be labeled with
 *   a unique name and ID value. The preferred approach is to define a
 *   static name for the op, and then use a const expression hash for the ID.
 *
 */

class Op {
public:

  struct op_create_as_target_t {};                              //!< Marker used to denote Target Op
  static constexpr op_create_as_target_t op_create_as_target{}; //!< Marker used to denote Target Op

  Op() = delete; ///< Empty ctor intentionally disabled to force explicit initialization

  /**
   * @brief Ctor for generating a new op at the node of origin
   * @details It is expected that a user will create an op and then hand
   * ownership of it over to opbox for executing its state machine
   *
   * @param[in] auto_create_mailbox Generate a uniqie mailbox for op during construction
   */
  explicit Op(bool auto_create_mailbox)
    : is_origin(true) {

    mailbox = (auto_create_mailbox) ? opbox::net::GetNextMailbox() //Bypasses op's UNSPECIFIED check
                                    : MAILBOX_UNSPECIFIED;

    ts_created = ts_lastaccessed = getMSTimeStamp();
  }

  /**
   * @brief Internal OpBox Ctor for creating a new target Op
   * @details This ctor is used by OpBox to create a new target Op. When a new
   * message arrives and has a MAILBOX_UNSPECIFIED destination mailbox, OpBox will
   * use the OpRegistry to locate the approprate Op generation function and then
   * call this Ctor. Users are not expected to call this Ctor.
   *
   * @param[in] t A constexpr token to signify this is a target
   */
  explicit Op(op_create_as_target_t t)
    : is_origin(false),
      mailbox(MAILBOX_UNSPECIFIED) {
    ts_created = ts_lastaccessed = getMSTimeStamp();
  }

  virtual ~Op() = default;

  /**
   * @brief OpBox calls this function whenever there is a new event that needs processing
   * @details Whenever OpBox receives an event for an Op, it embeds the event in an OpArgs
   * and then calls Update. In most Ops, state machines are encoded in separate origin
   * and target components to make the code more readable. Therefore the default behavior
   * is to for Update to route the Update to either UpdateOrigin or UpdateTarget, based
   * on whether the Op was created as the origin or not. However, users may override these
   * functions and build their own Update infrastructure.
   *
   * @param[in] args Information about the event being passed to the Op
   * @return WaitingType
   */
  virtual WaitingType Update(OpArgs *args) {
    return (is_origin) ? UpdateOrigin(args)
                       : UpdateTarget(args);
  }                                                    ///< Function called by OpBox when new event happens

  virtual WaitingType UpdateOrigin(OpArgs *args)=0;    ///< Update the origin side of op (called by Update)
  virtual WaitingType UpdateTarget(OpArgs *args)=0;    ///< Update the target side of op (called by Update)
  virtual std::string GetStateName() const = 0;        ///< Returns a printable name for current state
  virtual unsigned int getOpID() const = 0;            ///< Returns a unique id for this type of Op
  virtual std::string getOpName() const = 0;           ///< Returns a unique string id for this type of Op
 
  bool isOrigin() const { return is_origin; }          ///< Returns whether this is an origin op or not
  int GetSecondsSinceCreated() const;                  ///< Report how many seconds since this was created
  int GetSecondsSinceAccessed() const;                 ///< Report how many seconds since this was accessed
  mailbox_t GetAssignedMailbox();                      ///< Generate a node-unique id for this instance
  void touch();                                        ///< Updates the last accessed timestep


protected:
  bool is_origin;        //!< True when this node created the op, false if this node is the target
  mailbox_t mailbox;     //!< A unique identifier for this op
  int ts_created;        //!< Millisecond timestamp of When the op was created
  int ts_lastaccessed;   //!< Millisecond timestamp of last time this op was touched

  int getMSTimeStamp() const;  ///< Generate a millisecond timestamp for this op

};

}

#endif // OPBOX_OP_HH
