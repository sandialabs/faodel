// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstring>

#include "kelpie/localkv/LocalKVCell.hh"
#include "kelpie/localkv/LocalKVRow.hh"


using namespace std;
using namespace faodel;

namespace kelpie {

LocalKVRow::LocalKVRow(const std::string &rowname, MutexWrapperTypeID mutex_type)
  : col_single(nullptr) {

  this->rowname = rowname;
  this->row_lock = GenerateMutexByTypeID(mutex_type);
}

LocalKVRow::~LocalKVRow(){
  //Cells must all be individually released
  for(auto &name_cellptr : cols){
    delete name_cellptr.second;
  }
  if(col_single) delete col_single;
  delete row_lock;
}


/**
 * @brief Do a lambda on a desired column. Create if it doesn't exist
 * @param[in] key Key to look for (key.K2 is only part used)
 * @param[in] flags Specific actions to take during lambda (eg create row if missing)
 * @param[out] info Optional pass back information about the column, after op
 * @param[in] func Lambda to execute on the column
 * @returns return code of lambda, or KELPIE_ENOENT if
 *
 * NOTE: row_info is filled in the next layer up, at LocalKV::doColOp
 */
rc_t LocalKVRow::doColOp(const Key &key,
                         lkv::lambda_flags_t flags,
                         object_info_t *info,  fn_column_op_t func){

  bool previously_existed = true;
  LocalKVCell *cell;
  if( flags & lkv::LambdaFlags::CREATE_IF_MISSING ) {
    cell = getOrCreateCol(key, &previously_existed);
  } else {
    cell = getCol(key);
    if(cell==nullptr){
      if(info) info->Wipe();
      return KELPIE_ENOENT;
    }
  }

  //Cell is valid, do the caller's function on the cell
  rc_t rc = func(*this, *cell, previously_existed);

  //Cell is updated, so stats should be good. See if this is an operation
  //that needs to trigger any dependencies
  if( flags & lkv::LambdaFlags::TRIGGER_DEPENDENCIES){
    cell->dispatchCallbacksAndNotifications(this, key);
  }

  //Pass back generic cell info if requested
  if(info) cell->getInfo(info);

  cell->time_accessed = cell->getTime();
  return rc;

}
/**
 * @brief Return the number of columns stored in this row
 * @note requires ownership of mutex
 * @returns number of columns
 */
size_t LocalKVRow::getNumCols() {
  return cols.size() + (col_single!=nullptr);
}


/**
 * @brief Search the row and pull out a pointer to a column of data if available
 * @param[in] key Key to look for (key.K2 is only part used)
 * @note requires ownership of the mutex
 * @returns Cell or nullptr
 */
LocalKVCell * LocalKVRow::getCol(const Key &key) {

  //Empty column gets put somewhere special
  if(key.K2().empty()) return col_single;

  //All other columns go in map
  map<string, LocalKVCell *>::iterator it;
  it=cols.find(key.K2());
  if(it==cols.end()) return nullptr;
  return it->second;
}

/**
 * @brief Search the row and pull out a pointer to a column of data if available
 * @param[in] colname Column to look for
 * @note requires ownership of the mutex
 * @returns Cell or nullptr
 */
LocalKVCell * LocalKVRow::getCol(const string &colname){

  //Empty column gets put somewhere special
  if(colname.empty()) return col_single;

  //All other columns go in map
  map<string, LocalKVCell *>::iterator it;
  it=cols.find(colname);
  if(it==cols.end()) return nullptr;
  return it->second;
}


string LocalKVRow::getFirstColumnName(){
  if(col_single) return "";
  auto itr = cols.begin();
  return itr->first;
}

size_t LocalKVRow::getFirstColumnUserSize() {
  if(col_single) return col_single->getUserSize();
  auto itr = cols.begin();
  return itr->second->getUserSize();
}

/**
 * @brief Search the row for a particular column. If it doesn't exist, create it
 * @param[in] key Key to look for (key.K2 is only part used)
 * @param[out] previously_existed True if this item already existed
 * @note requires ownership of the mutex
 * @returns Cell or nullptr
 */
LocalKVCell * LocalKVRow::getOrCreateCol(const Key &key, bool *previously_existed){
  bool existed=true;
  LocalKVCell *cell = getCol(key);

  if(cell==nullptr){
    //TODO: verify we have a write lock here
    //Cell previously unknown. Create and add it

    cell = new LocalKVCell();
    if(key.K2().empty()) col_single = cell;
    else                 cols[key.K2()] = cell;
    existed=false;
  }
  if(previously_existed) *previously_existed = existed;
  return cell;
}


/**
 * @brief Perform a search for columns in this row that match a search string
 * @param[in] search_string The column to search for. Will prefix match if string ends in '*'
 * @param[out] names The column names that matched the search
 * @param[out] capacities The LDO user capacities for the matching column(s)
 * @return The number of matches found
 */
int LocalKVRow::getActiveColumnNamesCapacities(const string &search_string, vector<string> *names, vector<size_t> *capacities) {

  bool col_wildcard = StringEndsWith(search_string, "*");

  if(!col_wildcard) {
    //Find exact match
    LocalKVCell *cell = getCol(search_string);
    if(cell==nullptr) return 0;
    if(names) names->push_back(search_string);
    if(capacities) capacities->push_back(cell->getUserSize());
    return 1;

  } else {
    //Do a wildcard search
    int num_found=0;

    //Adjust the search name so it doesn't end in *
    string prefix=search_string;
    prefix.erase(prefix.size()-1);

    //Check no-name column first
    if(prefix.empty() && (col_single)) {
      if(names) names->push_back("");
      if(capacities) capacities->push_back(col_single->getUserSize());
    }

    //Walk through all entries and see if we have a hit
    for(auto name_colptr = cols.lower_bound(prefix); name_colptr!=cols.end(); ++name_colptr) {
      if(name_colptr->second->getUserSize()) { //Only entries with data
        if(StringBeginsWith(name_colptr->first, prefix)) {
          if(names) names->push_back(name_colptr->first);
          if(capacities) capacities->push_back(name_colptr->second->getUserSize());
          num_found++;
        }
      }
    }
    return num_found;
  }
}

/**
 * @brief Remove columns that match a search string
 * @param[in] search_string The column pattern we want. Does prefix match if string ends in '*'
 * @return The number of columns that the drop command matched
 */
int LocalKVRow::dropColumns(const std::string &search_string) {

  bool col_wildcard = StringEndsWith(search_string, "*");

  if(!col_wildcard) {
    //Find exact match
    LocalKVCell *cell = getCol(search_string);
    if(cell == nullptr) return 0;
    if(cell->isDroppable()) {
      delete cell;
      if(search_string.empty()) col_single = nullptr;
      else cols.erase(search_string);

    } else {
      //Cell has things waiting on it. Mark as drop.
      cell->drop_requested = true;
    }
    return 1;

  } else {

    //Do a wildcard search
    int num_found=0;

    //Adjust the search name so it doesn't end in *
    string prefix=search_string;
    prefix.erase(prefix.size()-1);

    //Check no-name column first
    if(prefix.empty() && (col_single)) {
      num_found++;
      delete col_single;
      col_single = nullptr;
    }

    //Walk through all entries and see if we have a hit
    vector<string> delete_names;
    for(auto name_colptr = cols.lower_bound(prefix); name_colptr!=cols.end(); ++name_colptr) {
      if(StringBeginsWith(name_colptr->first, prefix)) {
        num_found++;
        if(name_colptr->second->isDroppable()) {
          delete name_colptr->second;
          delete_names.push_back(name_colptr->first);
        } else {
          name_colptr->second->drop_requested = true;
        }
      }
    }

    //Remove all the items from the map
    for(auto &name : delete_names)
      cols.erase(name);

    return num_found;
  }
}

/**
 * @brief Drops a particular key from the table
 * @param[in] key The key to drop
 * @param[in] drop_options TODO extra options to use for dropping
 * @param[out] dropped_bytes Optional counter on the number of bytes that were dropped
 * @param[out] is_empty Specifies whether this row can be dropped after the op
 * @retval KELPIE_OK Always returns ok
 */
rc_t LocalKVRow::drop(const Key &key, int drop_options, size_t *dropped_bytes, bool *is_empty){

  LocalKVCell *cell = getCol(key);
  if(cell!=nullptr){
    //Found it. Delete it.
    if(dropped_bytes) *dropped_bytes += cell->getUserSize();

    delete cell;

    if(key.K2().empty()) col_single=nullptr;
    else                 cols.erase(key.K2());
  }
  *is_empty = isDroppable();
  return KELPIE_OK;
}

/**
 * @brief Reports back whether this row CAN be dropped (ie, is it free of dependencies)
 * @retval TRUE Row does not have any dependencies and can be dropped
 * @retval FALSE Row has columns with dependencies that prevent dropping
 */
bool LocalKVRow::isDroppable() {
  if((col_single) && (!col_single->isDroppable())) return false;
  for(auto &name_col : cols) {
    if(!name_col.second->isDroppable()) return false;
  }
  return true;
}

/**
 * @brief Determine if this row is empty (ie, no cells in single or cols)
 * @retval TRUE Row doesn't have cells or waiting dependencies
 * @retval FALSE Row has at least one cell in it (possibly with a dependency)
 */
bool LocalKVRow::isEmpty() {
  return (col_single==nullptr) && (cols.empty());
}


/**
 * @brief Get an overall Availability estimate for all items in this row
 * @retval Availability An availability value if all cells in row are the same
 * @retval MixedConditions Designates that cells had different availabilities
 */
Availability LocalKVRow::getAvailability() {
  Availability a;

  //Set the initial value
  if(col_single) {
    a=col_single->availability;
  } else if (!cols.empty()) {
    a=cols.begin()->second->availability;
  } else {
    return Availability::Unavailable;
  }

  //Scan through the column list
  for(auto &name_cellptr : cols) {
    if(name_cellptr.second->availability != a) {
      return Availability::MixedConditions;
    }
  }
  return a;
}


/**
 * @brief Generate summary information for all the cells in this row
 * @param[in] key Optional info that can be used to filter columns via a wildcard
 * @param[out] info Updates the ROW portion of info
 */
void LocalKVRow::getInfo(const Key &key, object_info_t *info){

  if(!info) return;
  string prefix_match;
  if(key.IsColWildcard()) {
    prefix_match=key.K2();
    prefix_match.erase( prefix_match.size()-1); //remove the trailing '*'
  }

  if(prefix_match.empty()) {
    //No '*' provided so we match everything in row

    //Sum up all the column bytes
    size_t row_bytes = 0;
    if(col_single) row_bytes = col_single->getUserSize();
    for(auto &name_cellptr : cols)
      row_bytes += name_cellptr.second->getUserSize();

    info->row_user_bytes = row_bytes;
    info->row_num_columns = getNumCols();
    return;

  } else {
    //User gave us a wildcard. This automatically filters out the noname column. Only look at matching cols.

    //Col Wildcard. Compute based on matches
    info->row_user_bytes = 0;
    info->row_num_columns = 0;
    for(auto &name_cellptr : cols) {
      if( StringBeginsWith(name_cellptr.first, prefix_match) ) {
        info->row_user_bytes += name_cellptr.second->getUserSize();
        info->row_num_columns++;
      }
    }
  }
}

/**
 * @brief Write debug info into a stream stream
 * @param[out] ss String Stream to append info into
 * @param[in] depth How many more levels to drill into, after this one
 * @param[in] indent how many spaces to indent this item
 */
void LocalKVRow::sstr(stringstream &ss, int depth, int indent) const {

  //int num_cols = cols.size() + (col_single!=nullptr);

  ss << string(indent,' ')+"[Row] '" << rowname;
  if(col_single!=nullptr) {
    ss << string(indent,' ') << "[Col] ''";
    if(depth>0)      col_single->sstr(ss, depth-1, indent+1);
    else             ss << endl;
  }
  for(auto &name_cellptr : cols){
    ss << string(indent,' ') << "[Col] '" << name_cellptr.first << "'";
    if(depth>0)      name_cellptr.second->sstr(ss, depth-1, indent+1);
    else             ss << endl;
  }
}

}  // namespace kelpie
