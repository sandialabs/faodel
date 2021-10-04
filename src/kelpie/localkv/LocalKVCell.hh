// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_LOCALKVCELL_HH
#define KELPIE_LOCALKVCELL_HH

#include <cinttypes>
#include <string>
#include <sstream>
#include <set>
#include <memory>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/InfoInterface.hh"

#include "opbox/ops/Op.hh"

#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"

#include "kelpie/pools/Pool.hh"

#include "lunasa/DataObject.hh"


namespace kelpie {

/**
 *   @brief A class for holding the final references to a block of memory
 */
class LocalKVCell :
    public faodel::InfoInterface {
  
public:

  LocalKVCell();
  LocalKVCell(const LocalKVCell &x) = delete;

  bool operator<( const LocalKVCell &x) const;
  
  size_t getUserSize() const {
    return ldo.GetUserSize();
  }

  Availability              availability;   //!< Where this data resides
  uint32_t                  hold_until;     //!< Hold at least until this point in time
  bool                      drop_requested; //!< User requested a drop, but dependencies prevented

  //Some usage statistics
  uint32_t                  time_posted;    //!< The time this block was stored locally
  uint32_t                  time_accessed;  //!< Last time this node was accessed
  uint32_t                  time_offloaded; //!< The time when this block was offloaded from memory to disk

  lunasa::DataObject        ldo;            //!< The actual data object stored by Kelpie

  //Functions for transferring ownership over to some other entity
  int    evict(const Key &key, const Availability new_availability, lunasa::DataObject *new_owners_ldo);


  //Functions for manipulating the dependencies.
  //void   appendWaitingList(faodel::nodeid_t);
  void   appendWaitingList(opbox::mailbox_t op_mailbox);
  void   appendCallbackList(fn_want_callback_t callback);
  void   dispatchCallbacksAndNotifications(LocalKVRow *row, const Key &key);

  //Functions for getting state info
  bool     isDroppable();
  uint32_t getTime();  //Make a timestamp
  void     getInfo(object_info_t *info);

  void getWaitingInfo(size_t *num_waiting_actions) {
    if(num_waiting_actions) *num_waiting_actions = getNumDependencies();
  }
  size_t getNumDependencies() { return waiting_ops_list.size() + callback_list.size(); }

  //InfoInterface function
  void   sstr(std::stringstream &ss, int depth, int indent) const override;


private:
  //Information about who generated and who wants
  std::set<opbox::mailbox_t>      waiting_ops_list;    //!< Ops that are stalled on this item
  std::vector<fn_want_callback_t> callback_list;       //!< Local callbacks to schedule when item becomes available


};

}  // namespace kelpie

#endif  // KELPIE_LOCALKVCELL_HH
