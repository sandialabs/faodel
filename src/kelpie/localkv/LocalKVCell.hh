// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_LOCALKVCELL_HH
#define KELPIE_LOCALKVCELL_HH

#include <inttypes.h>
#include <string>
#include <sstream>
#include <set>
#include <memory>

#include "common/FaodelTypes.hh"
#include "common/InfoInterface.hh"

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
  
  size_t getUserSize() {
    return ldo.GetUserSize();
  }
  
  int  evict(const Key &key, std::string fileio_tbd);
  
  Availability              availability;   //!< Where this data resides
  uint32_t                  hold_until;     //!< Hold at least until this point in time

  //Some usage statistics
  uint32_t                  time_posted;    //!< The time this block was stored locally
  uint32_t                  time_accessed;  //!< Last time this node was accessed
  uint32_t                  time_offloaded; //!< The time when this block was offloaded from memory to disk

  lunasa::DataObject        ldo;            //!< The actual data object stored by Kelpie

  //Functions for transferring ownership over to some other entity
  int    evict(const Key &key, const Availability new_availability, lunasa::DataObject *new_owners_ldo);


  //Functions for manipulating the dependencies.
  void   appendWaitingList(faodel::nodeid_t);
  void   appendWaitingList(opbox::mailbox_t op_mailbox);
  void   appendCallbackList(fn_want_callback_t callback);
  void   dispatchCallbacksAndNotifications(LocalKVRow *row, const Key &key);

  //Functions for getting state info
  bool     isDroppable();
  uint32_t getTime();  //Make a timestamp
  void     getInfo(kv_col_info_t *col_info);

  //InfoInterface function
  void   sstr(std::stringstream &ss, int depth=0, int indent=0) const;


private:
  //Information about who generated and who wants
  std::set<faodel::nodeid_t>      waiting_nodes_list;  //!< Nodes that want a copy of this when it becomes available
  std::set<opbox::mailbox_t>      waiting_ops_list;    //!< Ops that are stalled on this item
  std::vector<fn_want_callback_t> callback_list;       //!< Local callbacks to schedule when item becomes available


};

}  // namespace kelpie

#endif  // KELPIE_LOCALKVCELL_HH
