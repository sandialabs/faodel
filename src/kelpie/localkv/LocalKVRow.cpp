// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <stdio.h>
#include <string.h>

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
 * @param[in] create_if_missing Create the column if its missing
 * @param[in] trigger_dependency_check After a cell update, see if we need to trigger any callbacks
 * @param[out] col_info (Optional) pass back information about the column, after op
 * @param[in] func Lambda to execute on the column
 * @returns return code of lambda, or KELPIE_ENOENT if
 *
 * NOTE: row_info is filled in the next layer up, at LocalKV::doColOp
 */
rc_t LocalKVRow::doColOp(const Key &key, bool create_if_missing, bool trigger_dependency_check,
                         kv_col_info_t *col_info,  fn_column_op_t func){

  bool previously_existed = true;
  LocalKVCell *cell;
  if(create_if_missing){
    cell = getOrCreateCol(key, &previously_existed);
  } else {
    cell = getCol(key);
    if(cell==nullptr){
      if(col_info){
        memset(col_info, 0, sizeof(kv_col_info_t));
        col_info->availability=Availability::Unavailable;  //just to make sure
      }
      return KELPIE_ENOENT;
    }
  }

  //Cell is valid, do the caller's function on the cell
  rc_t rc = func(*this, *cell, previously_existed);

  //Cell is updated, so stats should be good. See if this is an operation
  //that needs to trigger any dependencies
  if(trigger_dependency_check){
    cell->dispatchCallbacksAndNotifications(this, key);
  }

  //Pass back generic cell info if requested
  if(col_info) cell->getInfo(col_info);

  cell->time_accessed = cell->getTime();
  return rc;

}
/**
 * @brief Return the number of columns stored in this row
 * @note requires ownership of mutex
 * @returns number of columns
 */
int LocalKVRow::getNumCols(){
  return cols.size() + (col_single!=nullptr);
}


/**
 * @brief Search the row and pull out a pointer to a column of data if available
 * @param[in] key Key to look for (key.K2 is only part used)
 * @note requires ownership of the mutex
 * @returns Cell or nullptr
 */
LocalKVCell * LocalKVRow::getCol(const Key &key){

  //Empty column gets put somewhere special
  if(key.K2()=="") return col_single;

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
LocalKVCell * LocalKVRow::getCol(const string colname){

  //Empty column gets put somewhere special
  if(colname=="") return col_single;

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
    if(key.K2()=="") col_single = cell;
    else             cols[key.K2()] = cell;
    existed=false;
  }
  if(previously_existed) *previously_existed = existed;
  return cell;
}


/**
 * @brief Insert a node into the waiting list for a particular cell.
 * @param[in] colname out column name
 * @param[in] requesting_node_id id of the node that really wants data (for forwarding)
 * @returns
   @retval KELPIE_OK This item was added and has not been asked for yet
   @retval KELPIE_EEXIST This item has already been asked for
*/
rc_t LocalKVRow::insertWaitingList(string colname, nodeid_t requesting_node_id){

  set<nodeid_t> node_set;

  map<string, set<nodeid_t>>::iterator it;
  it=col_waiting_list.find(colname);

  if(it!=col_waiting_list.end()){
    //This entry already on the wait list
    node_set=it->second;
    return KELPIE_EEXIST;
  }
  node_set.insert(requesting_node_id);
  col_waiting_list[colname] = node_set;

  return KELPIE_OK;
}


/**
 * @brief Return the list of nodes that have asked for this column before
 * @param[in] colname String name of the column we want
 * @param[out] waiting_list A set containing all of the nodes that are waiting on this entry
 * @note this will clear out the column's waiting list, as it is assumed you're going to handle it
 * @todo check into passing back an empty set
 */
void LocalKVRow::getWaitingList(string colname, set<nodeid_t> &waiting_list){


  map<string, set<nodeid_t>>::iterator it;
  it=col_waiting_list.find(colname);

  if(it!=col_waiting_list.end()){
    //Found something. Pass the list back, then clear out the entry
    waiting_list = it->second;
    col_waiting_list.erase(it);

  }// else {
   // //No members. Be nice and pass back an empty set
   // set<nodeid_t> node_set;
   // *waiting_list = node_set;
  //}

  return;
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

    if(key.K2()=="") col_single=nullptr;
    else             cols.erase(key.K2());
  }
  *is_empty = isDroppable();
  return KELPIE_OK;
}

/**
 * @brief Reports back whether this row is empty and can be deleted
 * @retval TRUE Row is empty and can be dropped
 * @retval FALSE Row has columns, or a list of subscribers waiting for data
 */
bool LocalKVRow::isDroppable(){
  return cols.empty() && col_waiting_list.empty();
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
  } else if (cols.size()>0) {
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
 * @param[out] row_info Summary statistics for the row
 */
void LocalKVRow::getInfo(kv_row_info_t *row_info){

  if(!row_info) return;

  //Sum up all the column bytes
  ssize_t row_bytes=0;
  if(col_single) row_bytes=col_single->ldo.GetUserSize();
  for(auto &name_cellptr : cols)
    row_bytes += name_cellptr.second->ldo.GetUserSize();

  row_info->row_bytes = row_bytes;
  row_info->num_cols_in_row = cols.size() + (col_single!=nullptr);
  row_info->num_row_receiver_nodes = col_waiting_list.size();
  row_info->num_row_dependencies = 0;  //TODO
  row_info->availability = getAvailability();
}

/**
 * @brief Write debug info into a stream stream
 * @param[out] ss String Stream to append info into
 * @param[in] depth How many more levels to drill into, after this one
 * @param[in] indent how many spaces to indent this item
 */
void LocalKVRow::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ')+"[Row] Name='" << rowname << "' Cols: " << cols.size() << endl;
  if(col_single!=nullptr){
    ss << string(indent+1,' ') << "Colname=''";
    if(depth>0)      col_single->sstr(ss, depth-1, indent+1);
    else             ss << endl;
  }
  for(auto &name_cellptr : cols){
    ss << string(indent+1,' ') << "Colname='" << name_cellptr.first << "'";
    if(depth>0)      name_cellptr.second->sstr(ss, depth-1, indent+1);
    else             ss << endl;
  }
}

}  // namespace kelpie
