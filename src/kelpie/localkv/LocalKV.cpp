// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <stdio.h>
#include <string.h>
#include <iostream>

#include "common/Debug.hh"
#include "kelpie/localkv/LocalKV.hh"

using namespace std;
using namespace faodel;

namespace kelpie {

LocalKV::LocalKV()
  : LoggingInterface("kelpie.lkv"),
    row_mutex_type_id(faodel::MutexWrapperTypeID::DEFAULT),
    configured(false), table_mutex(nullptr) {

  webhook::Server::updateHook("/kelpie/lkv", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWebhookStatus(args, results);
    });
  webhook::Server::updateHook("/kelpie/lkv/row", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWebhookRow(args, results);
    });
  webhook::Server::updateHook("/kelpie/lkv/cell", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWebhookCell(args, results);
    });
}
LocalKV::~LocalKV(){

  webhook::Server::deregisterHook("/kelpie/lkv/cell");
  webhook::Server::deregisterHook("/kelpie/lkv/row");
  webhook::Server::deregisterHook("/kelpie/lkv");

  if(configured){
    wipeAll(faodel::internal_use_only);
    delete table_mutex;
  }
}

/**
 * @brief Do a one-time configure of the localkv before it is used.
 * @param[in] config The Kelpie Configuration object, which stores the list of settings
 * @retval KELPIE_OK LocalKV was configured successfully
 * @retval KELPIE_EEXIST The LocalKV was already configured and this action was ignored.
 */
rc_t LocalKV::Init(const Configuration &config){

  if(configured) {
    cerr << "Error: Attempted to reconfigure the LocalKV. Configuration can only take place once.\n";
    return KELPIE_EEXIST;
  }

  //Enable our configuration
  ConfigureLogging(config);


  //Set our max capacity
  config.GetInt(&max_capacity, "kelpie.lkv.max_capacity","1M");


  //Create our mutex
  table_mutex = config.GenerateComponentMutex("kelpie.lkv", "rwlock");
  configured=true;



  webhook::Server::updateHook("/kelpie/lkv", [this] (const map<string,string> &args, stringstream &results) {
        return HandleWebhookStatus(args, results);
    });
  return KELPIE_OK;
}

/**
 * @brief Put a lunasa data object reference into the LocalKV
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] new_ldo The Lunasa Data Object that the lkv will use to reference an object
 * @param[in] key The key used to reference this data
 * @param[out] row_info Information about the row, after update
 * @param[out] col_info Information about the column, after update
 * @retval KELPIE_OK Success with no triggers
 * @retval KELPIE_TODO Success with triggers that need to be handled
 * @retval KELPIE_EEXIST Entry already exists and therefore was not written
 * @note This put stores a reference (via LDO) to the user's data.
 * As such the user should not modify the LDO's data until it has been evicted.
 */
rc_t LocalKV::put(bucket_t bucket, const Key &key,
                    lunasa::DataObject const &new_ldo,
                    kv_row_info_t *row_info,
                    kv_col_info_t *col_info){

  kassert(key.valid(), "Put given invalid key");

  dbg("Put "+bucket.GetHex()+"|"+key.str()+" length "+to_string(new_ldo.GetTotalSize()));



  rc_t rc = doColOp(bucket, key, true, true, row_info, col_info, //Pub creates if missing and triggers deps
                    [&new_ldo, key] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      if(col.availability==Availability::InLocalMemory){
                        //Existing version? ignore?
                        return KELPIE_EEXIST;
                      }
                      //Fill in the data
                      col.availability = Availability::InLocalMemory;
                      col.ldo = new_ldo;  //todo: deep copy?
                      col.time_posted = col.getTime();

                      return KELPIE_OK;
                    });

  return rc;
}

/**
 * @brief Get a Lunasa Data Object back for a desired key, if available. If not, do nothing.
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[out] ext_ldo A new (reference counted) ldo that provides access to the data stored in the lkv
 * @param[out] row_info Information about the row this ldo is/should live in
 * @param[out] col_info Information about the column this ldo is/should live in
 * @retval KELPIE_OK Succes
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_WAITING Data not here, but request has already been made
 * @retval KELPIE_TODO Needed load from disk
 * @retval KELPIE_EINVAL Invalid input (eg, requesting_node_id was UNSPECIFIED)
 * @note This version passes back an ldo reference to the data. Modifying the data could have side effects.
 * @todo Put some logic here to disable wait lists?
 */
rc_t LocalKV::get(bucket_t bucket, const Key &key,
                    lunasa::DataObject *ext_ldo,
                    kv_row_info_t *row_info,
                    kv_col_info_t *col_info) {

  kassert(key.valid(), "get given invalid key");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key, false, false, //<--don't create an entry if it doesn't exist, don't trigger deps
                    row_info, col_info,
                    [ext_ldo] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      if(col.availability == Availability::InLocalMemory){
                        if(ext_ldo)    *ext_ldo = col.ldo;
                        return KELPIE_OK;
                      }

                      return KELPIE_ENOENT;
                    });
  return rc;
}

#if 0 //Do we need this anymore
/**
 * @brief Get a Lunasa Data Object back for a desired key, if available
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] requesting_node_id The node that wants the data, in cases where a node should be put on the waiting list
 * @param[out] ext_ldo A new (reference counted) ldo that provides access to the data stored in the lkv
 * @param[out] row_info Information about the row this ldo is/should live in
 * @param[out] col_info Information about the column this ldo is/should live in
 * @retval KELPIE_OK Succes
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_WAITING Data not here, but request has already been made
 * @retval KELPIE_TODO Needed load from disk
 * @retval KELPIE_EINVAL Invalid input (eg, requesting_node_id was UNSPECIFIED)
 * @note This version passes back an ldo reference to the data. Modifying the data could have side effects.
 * @todo Put some logic here to disable wait lists?
 */
rc_t LocalKV::get(bucket_t bucket, const Key &key,
                    nodeid_t requesting_node_id_if_missing,
                    lunasa::DataObject *ext_ldo,
                    kv_row_info_t *row_info,
                    kv_col_info_t *col_info) {

  kassert(key.valid(), "get given invalid key");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key, true, false, //<--Creates a node dependency if missing, but don't trigger dep check
                    row_info, col_info,
                    [ext_ldo, &requesting_node_id_if_missing] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {


                      if(col.availability == Availability::InMemory){
                        if(ext_ldo)    *ext_ldo = col.ldo;
                        return KELPIE_OK;
                      }
                      //Add to the wait list. TODO: send back waiting?
                      col.appendWaitingList(requesting_node_id_if_missing);
                      return KELPIE_ENOENT;

                      //TODO: check local for not here?
                    });
  return rc;
}
#endif

/**
 * @brief Get a Lunasa Data Object back for a desired key. Leave mailbox dependency if not available
 * @param[in]  bucket The user id that is marked as the bucket of this data
 * @param[in]  key The key used to reference this data
 * @param[in]  mailbox_if_missing An op mailbox to wakeup if the object isn't available
 * @param[out] ext_ldo A new (reference counted) ldo that provides access to the data stored in the lkv
 * @param[out] col_info Information about the column this ldo is/should live in
 * @param[out] row_info Information about the row this ldo is/should live in
 * @retval KELPIE_OK Succes
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_WAITING Data not here, but request has already been made
 * @retval KELPIE_TODO Needed load from disk
 * @retval KELPIE_EINVAL Invalid input (eg, requesting_node_id was UNSPECIFIED)
 * @note This version passes back an ldo reference to the data. Modifying the data could have side effects.
 * @todo Put some logic here to disable wait lists?
 */
rc_t LocalKV::get(bucket_t bucket, const Key &key,
                    opbox::mailbox_t mailbox_if_missing,
                    lunasa::DataObject *ext_ldo,
                    kv_row_info_t *row_info,
                    kv_col_info_t *col_info) {

  kassert(key.valid(), "get given invalid key");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key, true, false, //<--Creates a mailbox dependency if missing, but don't trigger dep check
                    row_info, col_info,
                    [ext_ldo, mailbox_if_missing] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      if(col.availability == Availability::InLocalMemory){
                        if(ext_ldo)    *ext_ldo = col.ldo;
                        return KELPIE_OK;
                      }
                      //Add to the wait list. TODO: send back waiting?
                      col.appendWaitingList(mailbox_if_missing);
                      return KELPIE_ENOENT;

                      //TODO: check local for not here?
                    });
  return rc;
}



/**
 * @brief Get data and copy it to a user's pointer
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] mem_ptr A raw pointer to where the data should be copied
 * @param[in] max_size The maximum amount of data to copy
 * @param[out] copied_size The amount of data actually copied out
 * @param[out] row_info Optional information about the row
 * @param[out] col_info Optional information about the column
 * @remark This only copies from the user's DATA section of an LDO (not the meta)
 * @retval KELPIE_OK Success
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_TODO Needed load from disk
 * @note This version copies data directly into user's memory.
 * @todo Fix the return codes
 */
rc_t LocalKV::getData(bucket_t bucket, const Key &key,
                    void *mem_ptr, size_t max_size,
                    size_t *copied_size,
                    kv_row_info_t *row_info,
                    kv_col_info_t *col_info){

  //No locks here. All handled by getRef

  kassert(key.valid(), "getData given invalid key");

  lunasa::DataObject ldo_ref;
  size_t tmp_copied_size;

  tmp_copied_size = 0;

  rc_t rc = get(bucket, key, &ldo_ref, row_info, col_info);
  if(rc==KELPIE_OK){
    //tmp_copied_size = (max_size<ldo_ref.capacity()) ? max_size : ldo_ref.capacity();
    tmp_copied_size = (max_size<ldo_ref.GetDataSize()) ? max_size : ldo_ref.GetDataSize();
    memcpy(mem_ptr, ldo_ref.GetDataPtr(), tmp_copied_size);
  }

  if(copied_size) *copied_size = tmp_copied_size;

  return rc;
}

/**
 * @brief Process a want request, which provides a means of leaving a callback if an item is not available
 *
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] requesting_node_id_if_missing The node that is requesting this info (if a remote node is requesting)
 * @param[in] caller_will_fetch_if_missing For designating that a network request will be made to retrieve if not here
 * @param[in] callback The function to execute when this item becomes available
 * @return rc_t
 */
rc_t LocalKV::want(bucket_t bucket, const Key &key,
                    nodeid_t requesting_node_id_if_missing,
                    bool caller_will_fetch_if_missing,
                    fn_want_callback_t callback){

  kassert(key.valid(), "want given invalid key");

  dbg("Want "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key,
                    true,  false,    //Creates entry if missing, but does not dispatch callbacks
                    nullptr, nullptr,
                    [callback,key,requesting_node_id_if_missing,caller_will_fetch_if_missing] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      //Is available
                      if(col.availability==Availability::InLocalMemory){

                        if(callback!=nullptr){
                          //Pass info about the row/col back
                          kv_row_info_t row_info;
                          kv_col_info_t col_info;
                          row.getInfo(&row_info);
                          col.getInfo(&col_info);
                          callback(true, key, col.ldo, row_info, col_info);
                        }
                        return KELPIE_OK;
                      }


                      //Not here. Make a note of it
                      if(requesting_node_id_if_missing==NODE_LOCALHOST){
                        if(callback!=nullptr){
                          col.appendCallbackList(callback);
                        }
                      } else {
                        col.appendWaitingList(requesting_node_id_if_missing);
                      }

                      bool already_requested = (col.availability == Availability::Requested);

                      //Mark up if caller is responsible for fetching it
                      if((caller_will_fetch_if_missing) && (!already_requested)) {
                          col.availability = Availability::Requested;
                          return KELPIE_ENOENT;  //First time needs a trigger
                      }

                      return (already_requested) ? KELPIE_WAITING : KELPIE_ENOENT;
                    });
  return rc;
}


/**
 * @brief Drop a k/v by key name, freeing space
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @return rc_t
 */
rc_t LocalKV::drop(bucket_t bucket, const Key &key){

  kassert(key.valid(), "drop given invalid key");

  dbg("Drop "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doRowOp(bucket, key,
                    false,
                    nullptr,
                    [&key] (LocalKVRow &row, bool previously_existed) {

                 //Try to drop the cell
                 LocalKVCell *cell = row.getCol(key);
                 if(!cell) return KELPIE_ENOENT;
                 if(!cell->isDroppable()) return KELPIE_WAITING;  //TODO: Should be put on a queue for delete later
                 delete cell;

                 if(key.K2()=="") row.col_single=nullptr;
                 else             row.cols.erase(key.K2());

                 //Check to see if whole row is empty
                 if(row.isDroppable())
                   return KELPIE_RECHECK;

                 return KELPIE_OK;
               });

  //When nothing left, we may need to recheck to get rid of the old row
  if(rc==KELPIE_RECHECK){
    table_mutex->WriterLock();
    string fullrowname = makeRowname(bucket, key);
    LocalKVRow *row = getRow(fullrowname);
    if(row==nullptr){
      //Nothing to delete. Someone beat us to it?
      table_mutex->Unlock();
      return KELPIE_OK;
    }
    if(row->isDroppable()){
      delete row;
      rows.erase(fullrowname);
    }
    table_mutex->Unlock();
    rc=KELPIE_OK;
  }

  return rc;
}

/**
 * @brief Use a lambda to manipulate a column inside the localkv
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] create_if_missing Boolean specifying whether the row/column should be created if missing
 * @param[in] trigger_dependency_check Check to see if this item had waiting actions
 * @param[out] row_info Optional information about the row
 * @param[out] col_info Optional information about the column
 * @param[in] func The function to be applied on the column in the local k/v
 * @return rc_t The rc of the Lambda or KELPIE_ENOENT if it was not available when create_if_missing is false
 */
rc_t LocalKV::doColOp(bucket_t bucket, const Key &key,
                    bool create_if_missing,
                    bool trigger_dependency_check,
                    kv_row_info_t *row_info, kv_col_info_t *col_info,
                    fn_column_op_t func){

  table_mutex->ReaderLock();
  string fullrowname = makeRowname(bucket, key);
  LocalKVRow *row = getRow(fullrowname);

  if(row==nullptr){

    //Row not available. Create it or bail out
    table_mutex->Unlock();

    if(!create_if_missing){
      //done: bail out
      if(row_info) memset(row_info, 0, sizeof(kv_row_info_t));
      if(col_info) memset(col_info, 0, sizeof(kv_col_info_t));
      return KELPIE_ENOENT;
    }

    //Attempt to create the row. Must be a writer
    table_mutex->WriterLock();

    //Retry fetching, in case someone created row in release
    row = getRow(fullrowname);
    if(row==nullptr){
      row = new LocalKVRow(key.K1(), row_mutex_type_id);
      rows[fullrowname] = row;
    }
  }
  row->lock();
  table_mutex->Unlock();

  //Do the user's op, which may trigger a dependency check after a cell is updated
  rc_t rc = row->doColOp(key, create_if_missing, trigger_dependency_check, col_info, func);

  //Create an updated row info for user if requested
  if(row_info) row->getInfo(row_info);

  row->unlock();

  return rc;
}

/**
 * @brief Use a lambda to manipulate a row inside the k/v
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] create_if_missing Boolean specifying whether the row/column should be created if missing
 * @param[out] row_info Optional information about the row
 * @param[in] func The function to be applied on the column in the local k/v
 * @return rc_t The rc of the Lambda or KELPIE_ENOENT if it was not available when create_if_missing is false
 */
rc_t LocalKV::doRowOp(bucket_t bucket, const Key &key,
                    bool create_if_missing,
                    kv_row_info_t *row_info,
                    fn_row_op_t func){

  table_mutex->ReaderLock();
  string fullrowname = makeRowname(bucket, key);
  LocalKVRow *row = getRow(fullrowname);
  bool previously_existed=true;
  if(row==nullptr){
    //Row not available. Create it or bail out.
    table_mutex->Unlock();

    if(!create_if_missing){
      //done: bail out
      if(row_info) memset(row_info, 0, sizeof(kv_row_info_t));
      return KELPIE_ENOENT;
    }

    //Attempt to create the row. Must be a writer
    table_mutex->WriterLock();

    //Retry fetching, in case someone created row in release
    row = getRow(fullrowname);
    if(row==nullptr){
      row = new LocalKVRow(key.K1(), row_mutex_type_id);
      rows[fullrowname] = row;
      previously_existed=false;
    }
  }
  row->lock();
  table_mutex->Unlock();

  rc_t rc = func(*row, previously_existed);
  if(row_info) row->getInfo(row_info);

  row->unlock();

  return rc;
}

/**
 * @brief Find a particular column by key and return its information
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[out] col_info Optional information about the column
 * @retval KELPIE_OK if column was here
 * @retval KELPIE_ENOENT if column was not here
 * @retval KELPIE_WAITING we're waiting for this column to be generated
 */
rc_t LocalKV::getColInfo(bucket_t bucket, const Key &key, kv_col_info_t *col_info) {

  dbg("GetColInfo "+bucket.GetHex()+"|"+key.str());
  return doColOp(bucket, key, false, false, nullptr, col_info,
                 [] (LocalKVRow &row, LocalKVCell &cell, bool previously_existed) {
                   if(cell.availability==Availability::Requested) return KELPIE_WAITING;
                   return (previously_existed) ? KELPIE_OK : KELPIE_ENOENT;
                 });
}

/**
 * @brief Find a particular row by key and return its information
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[out] row_info Optional information about the row
 * @retval KELPIE_OK if row was here
 * @retval KELPIE_ENOENT if row was not here
 * @retval KELPIE_WAITING if row was not here, but has been requested
 */
rc_t LocalKV::getRowInfo(bucket_t bucket, const Key &key, kv_row_info_t *row_info) {

  dbg("GetRowInfo "+bucket.GetHex()+"|"+key.str());
  return doRowOp(bucket, key, false, row_info,
                 [] (LocalKVRow &row, bool previously_existed) {
                   if(row.getAvailability()==Availability::Requested) return KELPIE_WAITING;
                   return (previously_existed) ? KELPIE_OK : KELPIE_ENOENT;
                 });
}


string LocalKV::makeRowname(bucket_t bucket, const Key &key){
  stringstream ss;
  ss << bucket.GetHex();
  ss << key.K1();
  return ss.str();
}

/**
 * @brief With table already locked, find a desired row for a key
 * @param[in] full_row_name The bucket+row name (use makeRowname() to build)
 * @returns Pointer to the row
 * @note table must already be locked when calling this
 */
LocalKVRow * LocalKV::getRow(string full_row_name){

  //printf("GetRow for bucket %d key '%s' fullrowname: '%s\n",bucket, key.str().c_str(), full_row_name.c_str() );

  map<string, LocalKVRow *>::iterator it;
  it=rows.find(full_row_name);
  if(it==rows.end())
    return nullptr;
  return it->second;
}

/**
 * @brief Remove all rows and columns from the lkv
 * @param[in] iuo Designates that the user understands this is not intended for general use
 */
void LocalKV::wipeAll(faodel::internal_use_only_t iuo){
  if(configured){
    table_mutex->WriterLock();

    //Don't remove rows until we know everyone is out of them
    for(auto &name_rowptr : rows){
      LocalKVRow *row = name_rowptr.second;
      row->lock();
      row->unlock();
      delete row;
    }
    //Remove all the entries from the map
    rows.clear();
    table_mutex->Unlock();
  }
}

/**
 * @brief Generate a web page with information about this node's LocalKV
 *
 * @param[in] args Key/Value list of args the user passed us
 * @param[in] results A stringstream for this function to write its results
 */
void LocalKV::HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results){

  webhook::ReplyStream rs(args, "Kelpie LocalKV Status", &results);
  auto it = args.find("detail");
  bool detailed = (it != args.end());
  webhookInfo(rs,detailed);
  rs.Finish();
}

/**
 * @brief Append a ReplyStream with localkv info
 *
 * @param[in] rs The ReplyStream to be written to
 * @param[in] detailed Whether to include detailed information or not
 */
void LocalKV::webhookInfo(webhook::ReplyStream &rs, bool detailed){

  rs.tableBegin("LocalKV");
  rs.tableTop({"Parameter","Setting"});
  rs.tableRow({"Configured:", (configured)?"True":"False"});
  rs.tableRow({"Max Capacity:", to_string(max_capacity)});
  rs.tableRow({"Current Rows:", to_string(rows.size())});
  rs.tableEnd();

  if(!detailed){
  //  rs.mkText(html::mkLink("Full LKV Details", "/kelpie/lkv&detail"));
  //} else {

    //Print row summaries
    table_mutex->ReaderLock();
    rs.tableBegin("LocalKV Row Summary");
    rs.tableTop({"FullRowID","RowName","NumCols","FirstColumn", "RowBytes","Subscribers","LocalCallbacks"});
    for(auto rname_rptr : rows){
      rname_rptr.second->lock();
      kv_row_info_t row_info;
      rname_rptr.second->getInfo(&row_info);
      string cname=rname_rptr.second->getFirstColumnName();
      if(cname=="") cname=html::mkLink("(noname)",       "/kelpie/lkv/cell&row="+rname_rptr.first);
      else          cname=html::mkLink(cname, "/kelpie/lkv/cell&row="+rname_rptr.first+"&col="+cname);

      rs.tableRow( {rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname,"/kelpie/lkv/row&row="+rname_rptr.first),
            to_string(row_info.num_cols_in_row),
            cname,
            to_string(row_info.row_bytes),
            to_string(row_info.num_row_receiver_nodes),
            to_string(row_info.num_row_dependencies)});
      rname_rptr.second->unlock();
    }
    rs.tableEnd();
    table_mutex->Unlock();

  } else {
    //Print each column in its own row
    kv_col_info_t col_info;

    table_mutex->ReaderLock();
    rs.tableBegin("LocalKV Full Details");
    rs.tableTop({"FullRowID","RowName","ColumnName","ColBytes","NodeDependencies","LocalDependencies"});
    for(auto rname_rptr : rows){
      rname_rptr.second->lock();

      //Pull out the default, no-name column
      if(rname_rptr.second->col_single){
        rname_rptr.second->col_single->getInfo(&col_info);
        rs.tableRow( {
            rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname,"/kelpie/lkv/row&row="+rname_rptr.first),
            html::mkLink("(noname)","/kelpie/lkv/cell&row="+rname_rptr.first),
            to_string(col_info.num_bytes),
            to_string(col_info.num_col_receiver_nodes),
            to_string(col_info.num_col_dependencies)});

      }
      //Now look at all named columns
      for(auto cname_cptr : rname_rptr.second->cols){

        cname_cptr.second->getInfo(&col_info);

        rs.tableRow( {
            rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname, "/kelpie/lkv/row&row="+rname_rptr.first),
            html::mkLink(cname_cptr.first, "/kelpie/lkv/cell&row="+rname_rptr.first+"&col="+cname_cptr.first),
            to_string(col_info.num_bytes),
            to_string(col_info.num_col_receiver_nodes),
            to_string(col_info.num_col_dependencies)});
      }

      rname_rptr.second->unlock();
    }
    rs.tableEnd();
    table_mutex->Unlock();


  }
}


/**
 * @brief Generate a web page with information about this node's LocalKV
 *
 * @param[in] args Key/Value list of args the user passed us
 * @param[in] results A stringstream for this function to write its results
 */
void LocalKV::HandleWebhookRow(const std::map<std::string,std::string> &args, std::stringstream &results){

  webhook::ReplyStream rs(args, "Kelpie LocalKV Row", &results);
  string rname="";
  auto ritr = args.find("row");
  if(ritr != args.end()) rname=ritr->second;

  string rname_txt="\""+rname+"\"";

  table_mutex->ReaderLock();
  auto row = getRow(rname);
  if(row==nullptr){
    table_mutex->Unlock();
    rs.mkSection("Results for Row "+rname_txt);
    rs.mkText("Entry not found");
  } else {

    row->lock();
    table_mutex->Unlock();
    kv_row_info_t ri;
    row->getInfo(&ri);

    rs.tableBegin("Row Info");
    rs.tableTop({"Parameter","Setting"});
    rs.tableRow({"Row Name:", row->rowname});
    rs.tableRow({"Total Bytes:", to_string(ri.row_bytes)});
    rs.tableRow({"Node Dependencies:", to_string(ri.num_row_receiver_nodes)});
    rs.tableRow({"Local Dependencies:", to_string(ri.num_row_dependencies)});
    rs.tableRow({"Availability:", availability_to_string(ri.availability)});
    rs.tableRow({"Current Columns:", to_string(row->cols.size()+((row->col_single!=nullptr)?1:0))});
    rs.tableEnd();
    
    rs.tableBegin("Row "+rname_txt+" Summary");
    rs.tableTop({"Column", "Bytes", "Availability"});
    if(row->col_single!=nullptr){
      rs.tableRow({
            html::mkLink("(noname)","/kelpie/lkv/cell&row="+rname),
            to_string(row->col_single->getUserSize()),
            availability_to_string(row->col_single->availability)} );
    }
    for(auto const &col : row->cols) {
      rs.tableRow({
            html::mkLink(col.first,"/kelpie/lkv/cell&row="+rname+"&col="+col.first),
            to_string(col.second->getUserSize()),
            availability_to_string(col.second->availability) });
    }
    rs.tableEnd();    
    row->unlock();
  }
  rs.Finish();
}



/**
 * @brief Generate a web page with information about this node's LocalKV
 *
 * @param[in] args Key/Value list of args the user passed us
 * @param[in] results A stringstream for this function to write its results
 */
void LocalKV::HandleWebhookCell(const std::map<std::string,std::string> &args, std::stringstream &results){

  const int chars_per_line=32;

  webhook::ReplyStream rs(args, "Kelpie LocalKV Cell", &results);
  string rname="";
  string cname="";
  auto ritr = args.find("row");
  auto citr = args.find("col");
  if(ritr != args.end()) rname=ritr->second;
  if(citr != args.end()) cname=citr->second;

  string rname_txt="\""+rname+"\"";
  string cname_txt="\""+cname+"\"";

  table_mutex->ReaderLock();
  auto row = getRow(rname);
  if(row==nullptr){
    rs.mkSection("Results for "+rname_txt+" "+cname_txt);
    rs.mkText("Entry not found");
  } else {
    //Found row
    auto col = row->getCol(cname);
    if(col==nullptr){
      rs.mkText("Row found, but not column");
    } else {
      //Found column
      ssize_t msize = col->ldo.GetMetaSize();
      ssize_t dsize = col->ldo.GetDataSize();

      rs.tableBegin("Column Entry "+rname_txt+" "+cname_txt);
      rs.tableTop({"Parameter", "Setting"});
      //rs.tableRow({"Row Name", row->rowname});
      rs.tableRow({"Row Name", html::mkLink(row->rowname, "/kelpie/lkv/row&row="+rname)});
      rs.tableRow({"Column Name", cname_txt});
      rs.tableRow({"Availability:", availability_to_string(col->availability)});

      rs.tableRow({"Object Meta Size", to_string(msize) });
      rs.tableRow({"Object Data Size", to_string(dsize) });
      rs.tableRow({"Total Allocation", to_string(col->ldo.GetTotalSize()) });
      rs.tableRow({"Local RefCount",   to_string(col->ldo.internal_use_only.GetRefCount()) });

      #if 1
      {
        net::NetBufferLocal  *nbl = nullptr;
        net::NetBufferRemote  nbr;
        net::GetRdmaPtr(&col->ldo, &nbl, &nbr);
        rs.tableRow({"RDMA Pointer", nbr.str()});
      }
      #endif
      
      rs.tableEnd();

      rs.mkSection("Data Object Dump");

      if(msize > 0) {
        vector<string> offset_lines, hex_lines, txt_lines;

        ssize_t size = (msize < 256) ? msize : 256;
        faodel::ConvertToHexDump(col->ldo.GetMetaPtr<char *>(), size,
                                  chars_per_line, 8,
                                  "<span class=\"HEXE\">", "</span>",
                                  "<span class=\"HEXO\">", "</span>",
                                  &offset_lines, &hex_lines, &txt_lines);

        vector<vector<string>> rows;
        rows.push_back({"Offset","Hex Data","Text"});
        for(int i=0; i<offset_lines.size(); i++){
          vector<string> row;
          row.push_back(offset_lines[i]);
          row.push_back(hex_lines[i]);
          row.push_back(txt_lines[i]);
          rows.push_back(row);
        }
        rs.mkTable(rows, "Meta Section");
      }

      if(dsize > 0) {
        vector<string> offset_lines, hex_lines, txt_lines;

        ssize_t size = (dsize < 2048) ? dsize : 2048;
        faodel::ConvertToHexDump(col->ldo.GetDataPtr<char *>(), size,
                                  chars_per_line, 8,
                                  "<span class=\"HEXE\">", "</span>",
                                  "<span class=\"HEXO\">", "</span>",
                                  &offset_lines, &hex_lines, &txt_lines);

        vector<vector<string>> rows;
        rows.push_back({"Offset","Hex Data","Text"});
        for(int i=0; i<offset_lines.size(); i++){
          vector<string> row;
          row.push_back(offset_lines[i]);
          row.push_back(hex_lines[i]);
          row.push_back(txt_lines[i]);
          rows.push_back(row);
        }
        rs.mkTable(rows, "Data Section");

      }
    }

  }
  table_mutex->Unlock();


  rs.Finish();
}


/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void LocalKV::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') << "[LKV] Number of Rows: " << rows.size() <<endl;
  if(depth>0){
    for(map<string, LocalKVRow *>::const_iterator ii=rows.begin(); ii!=rows.end(); ++ii){
      ss << string(indent+1,' ') << ii->first << " ";
      ii->second->sstr(ss, depth-1, indent+1);
    }
  }
}

}  // namespace kelpie
