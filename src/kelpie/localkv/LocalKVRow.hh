// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LOCALKVROW_HH
#define LOCALKVROW_HH

#include <map>
#include <set>
#include <memory>
#include <functional>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/InfoInterface.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKVTypes.hh"
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
  ~LocalKVRow() override;

  size_t getNumCols();
  void getInfo(const Key &key, object_info_t *row_info);
  Availability getAvailability();

  void lock(){ row_lock->Lock(); }     //!< Locks this row
  void unlock(){ row_lock->Unlock(); } //!< Unlocks this row

  rc_t doColOp(const Key &key, kelpie::lkv::lambda_flags_t flags, object_info_t *info, fn_column_op_t func);

  rc_t drop(const Key &key, int drop_options, size_t *dropped_bytes, bool *is_empty);
  bool isDroppable();
  bool isEmpty();


  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;


  //The following would normally be private, but are exposed so doOp can see them
  std::string rowname;                                                  //!< The row name (first string in key)
  LocalKVCell *col_single;                                              //!< When user hands "" for column part of key
  std::map<std::string, LocalKVCell *> cols;                            //!< Map for locating our cell, based on the column key


  LocalKVCell * getCol(const Key &key);
  LocalKVCell * getCol(const std::string &colname);
  std::string   getFirstColumnName();
  size_t        getFirstColumnUserSize();
  int           getActiveColumnNamesCapacities(const std::string &search_string, std::vector<std::string> *names, std::vector<size_t> *capacities);
  int           dropColumns(const std::string &search_string);

private:
  LocalKVCell * getOrCreateCol(const Key &key, bool *previously_existed);
  faodel::MutexWrapper *row_lock;                                       //!< A lock that is used every time this row is accessed

};

}  // namespace kelpie

#endif
