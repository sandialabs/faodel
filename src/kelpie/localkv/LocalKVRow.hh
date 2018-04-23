// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LOCALKVROW_HH
#define LOCALKVROW_HH

#include <map>
#include <set>
#include <memory>
#include <functional>

#include "common/FaodelTypes.hh"
#include "common/InfoInterface.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKVCell.hh"

#include "lunasa/DataObject.hh"

namespace kelpie {

  /**
   *  The LocalKVRow holds multiple cells with the same row key and provides a way
   *  to access these items in a thread safe way. The LocalKVRow uses a single mutex for
   *  all members of the row and uses a map to store each item (by column name). The LocalKVRow
   *  also provides a waitlist structure that allows actions to be queued up until a particular
   *  item or set of items arrive.
   *  @note caller must lock/unlock the row while using it to prevent cols from changing during work
   *  @brief Container for multiple cells that share the same row key
   */
class LocalKVRow : public faodel::InfoInterface {


public:
  LocalKVRow(const std::string &rowname, faodel::MutexWrapperTypeID mutex_type);
  ~LocalKVRow();

  int getNumCols();
  void getInfo(kv_row_info_t *row_info);
  Availability getAvailability();

  void lock(){ row_lock->Lock(); }     //!< Locks this row
  void unlock(){ row_lock->Unlock(); } //!< Unlocks this row

  rc_t doColOp(const Key &key, bool create_if_missing, bool trigger_dependency_check,  kv_col_info_t *col_info, fn_column_op_t func);

  rc_t drop(const Key &key, int drop_options, size_t *dropped_bytes, bool *is_empty);
  bool isDroppable();


  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;


  //The following would normally be private, but are exposed so doOp can see them
  std::string rowname;                                                  //!< The row name (first string in key)
  LocalKVCell *col_single;                                              //!< When user hands "" for column part of key
  std::map<std::string, LocalKVCell *> cols;                            //!< Map for locating our cell, based on the column key
  std::map<std::string, std::set<faodel::nodeid_t> > col_waiting_list;  //!< All the nodes that are waiting on this item to arrive

  rc_t insertWaitingList(std::string colname, faodel::nodeid_t requesting_node_id);
  void getWaitingList(std::string colname, std::set<faodel::nodeid_t> &waiting_list);

  LocalKVCell * getCol(const Key &key);
  LocalKVCell * getCol(const std::string colname);
  std::string   getFirstColumnName();

private:
  LocalKVCell * getOrCreateCol(const Key &key, bool *previously_existed);
  faodel::MutexWrapper *row_lock;                                       //!< A lock that is used every time this row is accessed

};

}  // namespace kelpie

#endif
