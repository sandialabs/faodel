// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstdio>
#include <cstring>
#include <iostream>

#include "faodel-common/Debug.hh"
#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/ioms/IomBase.hh"
#include "kelpie/core/Singleton.hh"

using namespace std;
using namespace faodel;
using namespace kelpie::lkv;

namespace kelpie {

LocalKV::LocalKV()
  : LoggingInterface("kelpie.lkv"),
    row_mutex_type_id(faodel::MutexWrapperTypeID::DEFAULT),
    configured(false), table_mutex(nullptr) {

  //Real work handled in Init when we have a real config
}

LocalKV::~LocalKV(){

  //Caution: assumes lkv lives inside something like kelpieCore, which uses bootstrap to preserve shutdown
  //         standalone tests of lkv must perform the same kind of order-preserving shutdown
  whookie::Server::deregisterHook("/kelpie/lkv/cell");
  whookie::Server::deregisterHook("/kelpie/lkv/row");
  whookie::Server::deregisterHook("/kelpie/lkv");

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

  F_ASSERT(!configured, "Attempted to call LocalKV Init more than once");

  //Enable our configuration
  ConfigureLogging(config);

  //Create our mutex
  table_mutex = config.GenerateComponentMutex("kelpie.lkv", "rwlock");
  configured=true;

  //Register whookies
  whookie::Server::updateHook("/kelpie/lkv", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieStatus(args, results);
  });
  whookie::Server::updateHook("/kelpie/lkv/row", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieRow(args, results);
  });
  whookie::Server::updateHook("/kelpie/lkv/cell", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieCell(args, results);
  });

  return KELPIE_OK;
}

/**
 * @brief Put a lunasa data object reference into the LocalKV
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] new_ldo The Lunasa Data Object that the lkv will use to reference an object
 * @param[in] behavior_flags Info about how the lkv should handle new data
 * @param[in] iom Pointer to the iom that may be affected by this put
 * @param[out] info Information about the row/column, after update
 * @retval KELPIE_OK Success with no triggers
 * @retval KELPIE_TODO Success with triggers that need to be handled
 * @retval KELPIE_EEXIST Entry already exists and therefore was not written
 * @note This put stores a reference (via LDO) to the user's data.
 * As such the user should not modify the LDO's data until it has been evicted.
 */
rc_t LocalKV::put(bucket_t bucket, const Key &key,
                    lunasa::DataObject const &new_ldo,
                    pool_behavior_t behavior_flags,
                    internal::IomBase *iom,
                    object_info_t *info) {

  F_ASSERT(key.valid(), "Put given invalid key");

  dbg("Put "+bucket.GetHex()+"|"+key.str()+" length "+to_string(new_ldo.GetUserSize())+" behavior: "+to_string(behavior_flags));

  //Only create if writing to local, but always see if we trigger dependencies
  lambda_flags_t lambda_flags = LambdaFlags::TRIGGER_DEPENDENCIES;
  if (behavior_flags & PoolBehavior::WriteToLocal) {
    lambda_flags |= lkv::LambdaFlags::CREATE_IF_MISSING;
  }

  rc_t rc = doColOp(bucket, key,
                    lambda_flags, //Pub triggers dependencies, and may need create
                    info,
                    [&new_ldo, key, behavior_flags] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      //Bail out if this already exists in memory and we aren't overwriting things.
                      if( (col.availability==Availability::InLocalMemory) &&
                          (!(behavior_flags & PoolBehavior::EnableOverwrites))  ) {
                        //Exists - ignore updates
                        return KELPIE_EEXIST;
                      }
                      //New item. Entry created, we just need to fill in the data
                      col.availability = Availability::InLocalMemory;
                      col.ldo = new_ldo;  //todo: deep copy?
                      col.time_posted = col.getTime();

                      return KELPIE_OK;
                    });

  dbg("put to lkv returned "+to_string(rc));

  //See if we need to write out to storage
  if(behavior_flags & PoolBehavior::WriteToIOM) {
    if(iom) {
      rc_t rc2 = iom->WriteObject(bucket, key, new_ldo);
      if(rc==KELPIE_OK) rc=rc2; //Capture first error code
    } else {
      rc=KELPIE_EIO; //Asked us to use an IOM but it didn't work
    }
  }


  return rc;
}

/**
 * @brief Get a Lunasa Data Object back for a desired key, if available. If not, do nothing.
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[out] ext_ldo A new (reference counted) ldo that provides access to the data stored in the lkv
 * @param[out] info Information about the row this ldo is/should live in
 * @retval KELPIE_OK Succes
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_WAITING Data not here, but request has already been made
 * @retval KELPIE_TODO Needed load from disk
 * @retval KELPIE_EINVAL Invalid input (eg, requesting_node_id was UNSPECIFIED)
 * @note This version passes back an ldo reference to the data. Modifying the data could have side effects.
 * @todo Put some logic here to disable wait lists?
 */
rc_t LocalKV::get(faodel::bucket_t bucket, const Key &key,
                    lunasa::DataObject *ext_ldo,
                    object_info_t *info) {

  F_ASSERT(key.valid(), "get given invalid key");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key,
                    lkv::LambdaFlags::DONT_CREATE_OR_TRIGGER, //<--Get doesn't create or trigger
                    info,
                    [ext_ldo] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      if(col.availability == Availability::InLocalMemory){
                        if(ext_ldo)    *ext_ldo = col.ldo;
                        return KELPIE_OK;
                      }

                      return KELPIE_ENOENT;
                    });
  return rc;
}


rc_t LocalKV::getAvailable(faodel::bucket_t bucket, const Key &key,
                           std::map<Key, lunasa::DataObject> &ldos) {

  F_ASSERT(key.valid(), "getAvailable given invalid key");
  F_ASSERT(!key.IsRowWildcard(), "getAvailable given a row wildcard");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc=KELPIE_ENOENT;

  if(!key.IsColWildcard()) {
    lunasa::DataObject ldo;
    rc = get(bucket, key, &ldo, nullptr);
    if(rc==KELPIE_OK) {
      ldos[key] = ldo;
    }
  } else {

    //Col wildcard
    rc = doRowOp(bucket, key,
                 lkv::LambdaFlags::DONT_CREATE_OR_TRIGGER, //<--Get doesn't create or trigger
                 nullptr,
                 [&ldos, key](LocalKVRow &row, bool previously_existed) {

                     vector<string> col_names;
                     row.getActiveColumnNamesCapacities(key.K2(), &col_names, nullptr);

                     for(auto &name : col_names) {
                       auto *cell = row.getCol(name);
                       if((cell) && (cell->availability == Availability::InLocalMemory)) {
                         ldos[Key(key.K1(), name)] = cell->ldo;
                       }
                     }
                     return (!ldos.empty()) ? KELPIE_OK : KELPIE_ENOENT;
                 });
  }
  return rc;
}



/**
 * @brief Get a Lunasa Data Object back for a desired key. Leave mailbox dependency if not available
 * @param[in]  bucket The user id that is marked as the bucket of this data
 * @param[in]  key The key used to reference this data
 * @param[in]  mailbox_if_missing An op mailbox to wakeup if the object isn't available
 * @param[in]  behavior_flags Specifies whether a load from iom gets cached here when miss and in iom
 * @param[in]  iom_hash The hash of the iom to consult if item is not found in memory
 * @param[out] ext_ldo A new (reference counted) ldo that provides access to the data stored in the lkv
 * @param[out] info Information about the row/column this ldo is/should live in
 * @retval KELPIE_OK Success, item found and returned in this call
 * @retval KELPIE_ENOENT Data not here, but node was placed on waiting list
 * @retval KELPIE_EIO The requested iom could not be found, but op was placed in waiting list
 * @note This version passes back an ldo reference to the data. Modifying the data could have side effects.
 */
rc_t LocalKV::getForOp(faodel::bucket_t bucket, const Key &key,
                    opbox::mailbox_t mailbox_if_missing,
                    pool_behavior_t behavior_flags,
                    iom_hash_t iom_hash,
                    lunasa::DataObject *ext_ldo,
                    object_info_t *info) {

  F_ASSERT(key.valid(), "get given invalid key");

  dbg("Get "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key,
                    LambdaFlags::CREATE_IF_MISSING, //Get: creates mailbox dependency but doesn't trigger a dependency check
                    info,
                    [bucket, key, behavior_flags, iom_hash, ext_ldo, mailbox_if_missing] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      //See if this item is in memory
                      if(col.availability == Availability::InLocalMemory){
                        if(ext_ldo)    *ext_ldo = col.ldo;
                        return KELPIE_OK;
                      }

                      //Not here. See if we need to load from disk
                      rc_t rc = KELPIE_ENOENT;
                      if(iom_hash!=0) {
                        auto iom = kelpie::internal::FindIOM(iom_hash);

                        if(iom==nullptr) {
                          rc = KELPIE_EIO; //IOM not found
                        } else {
                          rc = iom->ReadObject(bucket, key, ext_ldo);
                          if(rc==KELPIE_OK) {
                            //Loaded it from disk. Do we keep a copy?
                            if(behavior_flags & PoolBehavior::ReadToRemote) {
                              col.availability = Availability::InLocalMemory;
                              col.ldo = *ext_ldo;
                              col.time_posted = col.getTime();
                            } else {
                              //Don't cache, but keep a record of it
                              col.availability = Availability::InDisk;
                            }
                            return KELPIE_OK;
                          }
                        }
                      }

                      //Not in memory and not in disk. Add op to waiting list
                      col.appendWaitingList(mailbox_if_missing);
                      return rc; //This is KELPIE_ENOENT unless iom not found (KELPIE_EIO)

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
 * @param[out] info Optional information about the row/column
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
                    object_info_t *info){

  //No locks here. All handled by getRef

  F_ASSERT(key.valid(), "getData given invalid key");

  lunasa::DataObject ldo_ref;
  size_t tmp_copied_size;

  tmp_copied_size = 0;

  rc_t rc = get(bucket, key, &ldo_ref, info);
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
 * @param[in] caller_will_fetch_if_missing For designating that a network request will be made to retrieve if not here
 * @param[in] callback The function to execute when this item becomes available
 * @return rc_t
 */
rc_t LocalKV::wantLocal(bucket_t bucket, const Key &key,
                    bool caller_will_fetch_if_missing,
                    fn_want_callback_t callback){

  F_ASSERT(key.valid(), "want given invalid key");

  dbg("Want "+bucket.GetHex()+"|"+key.str());

  rc_t rc = doColOp(bucket, key,
                    LambdaFlags::CREATE_IF_MISSING,    //Creates entry if missing, but does not dispatch callbacks
                    nullptr,
                    [callback,key, caller_will_fetch_if_missing] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {

                      //Is available
                      if(col.availability==Availability::InLocalMemory){

                        if(callback!=nullptr){
                          //Pass info about the row/col back
                          object_info_t info;
                          row.getInfo(key, &info);
                          col.getInfo(&info);
                          callback(true, key, col.ldo, info);
                        }
                        return KELPIE_OK;
                      }


                      //Not here. Make a note of it
                      if(callback!=nullptr){
                          col.appendCallbackList(callback);
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
 * @param[in] key_prefix The key used to reference this data. Key row/col may end in '*' for prefix matching
 * @retval KELPIE_OK Item found and drop
 * @retval KELPIE_ENOENT Item not found
 */
rc_t LocalKV::drop(bucket_t bucket, const Key &key_prefix){

  F_ASSERT(key_prefix.valid(), "drop given invalid key_prefix");

  dbg("Drop "+bucket.GetHex()+"|"+key_prefix.str());

  int found_items=0;

  vector<string> check_rows;   //Candidate rows to inspect
  vector<string> recheck_rows; //Rows where drops happened and may be removals

  //Determine which rows to look at
  if(!key_prefix.IsRowWildcard()) {
    //Exact match on K1, partial match on K2
    check_rows.push_back(key_prefix.K1());

  } else {
    //Search for possible rows
    string prefix = makeRowname(bucket, key_prefix.K1());
    prefix.erase(prefix.size()-1); //remove the trailing *

    table_mutex->ReaderLock();
    for(auto name_rowptr = rows.lower_bound(prefix); name_rowptr!=rows.end(); ++name_rowptr) {
      if(StringBeginsWith(name_rowptr->first, prefix))
        check_rows.push_back(name_rowptr->second->rowname);
    }
    table_mutex->Unlock();
  }

  //Inspect each row that matches row key and delete matching columns.
  for(auto &rowname : check_rows) {
    rc_t rc = doRowOp(bucket, Key(rowname),
                      false,
                      nullptr,
                      [&key_prefix, &found_items](LocalKVRow &row, bool previously_existed) {

                          found_items += row.dropColumns(key_prefix.K2());

                          //Check to see if whole row is gone. If so, remember it for cleanup
                          return (row.isEmpty()) ? KELPIE_RECHECK : KELPIE_OK;
                      });

    if(rc == KELPIE_RECHECK) recheck_rows.push_back(rowname);
  }

  //Cleanup: Search for rows that can be deleted
  if(!recheck_rows.empty()) {
    table_mutex->WriterLock();
    for(auto &rowname : recheck_rows) {
      string fullrowname = makeRowname(bucket, Key(rowname));
      LocalKVRow *row = getRow(fullrowname);
      if((row) && (row->isEmpty())) {
        delete row;
        rows.erase(fullrowname);
      }
    }
    table_mutex->Unlock();
  }

  return (found_items) ? KELPIE_OK : KELPIE_ENOENT;

}

/**
 * @brief Perform a search for keys that match a specific pattern
 * @param[in] bucket The bucket id we should limit the search to
 * @param[in] key_prefix The key to search for. Row and/or Column may end in '*' for prefix matching
 * @param[in] iom Pointer to iom related to this operation
 * @param[out] object_capacities The results in this localkv that matched the key_prefix
 * @retval KELPIE_OK Found matches
 * @retval KELPIE_ENOENT Did not find matches
 */
rc_t LocalKV::list(bucket_t bucket, const Key &key_prefix,
                    internal::IomBase *iom,
                    ObjectCapacities *object_capacities) {


  F_ASSERT(key_prefix.valid(), "list given an invalid key");

  dbg("List "+key_prefix.str());

  bool found_items=false;
  bool needs_an_iom_check = (iom!=nullptr);

  if(!key_prefix.IsRowWildcard()) {
    //Exact row known. Search on the columns

    vector<string> col_names;

    //Exact match on K1, partial match on K2
    doRowOp(bucket, key_prefix,
                      false,
                      nullptr,
                      [&key_prefix, &col_names, &object_capacities] (LocalKVRow &row, bool previously_existed) {
                          row.getActiveColumnNamesCapacities(key_prefix.K2(), &col_names, &object_capacities->capacities);
                          return KELPIE_OK;
                      });

    //Convert row's column names into full keys
    for(auto &col_name : col_names) {
      object_capacities->keys.emplace_back(Key(key_prefix.K1(), col_name));
    }

    //Append any info from iom. Skip the case where we had an exact column name and we got back a result
    needs_an_iom_check = needs_an_iom_check && ( key_prefix.IsColWildcard() || (col_names.size()==1));

    found_items = (col_names.size()!=0); //Only report what we found in this function


  } else {
    //Fuzzy Row Name

    //Build the full row name we'll use
    string prefix = makeRowname(bucket, key_prefix.K1());
    prefix.erase(prefix.size()-1); //remove the trailing *


    //Find all the row names that match and query each one
    table_mutex->ReaderLock();
    for(auto name_rowptr = rows.lower_bound(prefix); name_rowptr!=rows.end(); ++name_rowptr) {
      if(StringBeginsWith(name_rowptr->first, prefix)) {
        vector<string> col_names;

        LocalKVRow *row = name_rowptr->second;
        row->lock();
        /*int num_found =*/ row->getActiveColumnNamesCapacities(key_prefix.K2(), &col_names, &object_capacities->capacities);
        string row_name = row->rowname;
        row->unlock();

        //Append all keys for each column to the output
        for(auto &c : col_names) {
          object_capacities->keys.emplace_back(Key(row_name, c));
        }
        found_items |= (col_names.size()!=0); //Only report what we found in this function
      }
    }
    table_mutex->Unlock();
  }

  if(needs_an_iom_check) {
    ObjectCapacities oc2;
    iom->ListObjects(bucket, key_prefix, &oc2);
    object_capacities->Merge(oc2); //Only inserts items that were unknown
    found_items |= (oc2.Size()>0);
  }

  return (found_items) ? KELPIE_OK : KELPIE_ENOENT;
}

rc_t LocalKV::doCompute(const string &function_name, const string &args,
                             bucket_t bucket, const Key &key,
                             lunasa::DataObject *ext_ldo) {

  map<Key, lunasa::DataObject> ldos;
  rc_t rc = getAvailable(bucket, key, ldos);

  kelpie::internal::Singleton::impl.core->compute_registry.doCompute(
          function_name, args,
          bucket, key,
          ldos,
          ext_ldo);

  return rc;
}

/**
 * @brief Use a lambda to manipulate a column inside the localkv
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] flags Specific actions to take during lambda (eg create row if missing)
 * @param[out] info Optional information about the row/column
 * @param[in] func The function to be applied on the column in the local k/v
 * @return rc_t The rc of the Lambda or KELPIE_ENOENT if it was not available when create_if_missing is false
 */
rc_t LocalKV::doColOp(bucket_t bucket, const Key &key,
                    lambda_flags_t flags,
                    object_info_t *info,
                    fn_column_op_t func) {

  table_mutex->ReaderLock();
  string fullrowname = makeRowname(bucket, key);
  LocalKVRow *row = getRow(fullrowname);

  if(row==nullptr){

    //Row not available. Create it or bail out
    table_mutex->Unlock();

    //Didn't want us to create, so bail out
    if(!(flags & LambdaFlags::CREATE_IF_MISSING)) {
      if(info) info->Wipe();
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
  rc_t rc = row->doColOp(key, flags, info, func);

  //Create an updated row info for user if requested
  if(info) row->getInfo(key, info);

  row->unlock();

  return rc;
}

/**
 * @brief Use a lambda to manipulate a row inside the k/v
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[in] flags Specific actions to take during lambda (eg create row if missing)
 * @param[out] info Optional information about the row
 * @param[in] func The function to be applied on the column in the local k/v
 * @return rc_t The rc of the Lambda or KELPIE_ENOENT if it was not available when create_if_missing is false
 */
rc_t LocalKV::doRowOp(bucket_t bucket, const Key &key,
                    lambda_flags_t flags,
                    object_info_t *info,
                    fn_row_op_t func) {

  table_mutex->ReaderLock();
  string fullrowname = makeRowname(bucket, key);
  LocalKVRow *row = getRow(fullrowname);
  bool previously_existed=true;
  if(row==nullptr){
    //Row not available. Create it or bail out.
    table_mutex->Unlock();

    if(!(flags & LambdaFlags::CREATE_IF_MISSING)) {
      //done: Didn't find and didn't want to create
      if(info) info->Wipe();
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
  if(info) row->getInfo(key, info);

  row->unlock();

  return rc;
}


/**
 * @brief Find a particular row by key and return its information
 * @param[in] bucket The user id that is marked as the bucket of this data
 * @param[in] key The key used to reference this data
 * @param[out] info Optional information about the item
 * @retval KELPIE_OK if row was here
 * @retval KELPIE_ENOENT if row was not here
 * @retval KELPIE_WAITING if row was not here, but has been requested
 */
rc_t LocalKV::getInfo(bucket_t bucket, const Key &key, object_info_t *info) {

  dbg("GetRowInfo "+bucket.GetHex()+"|"+key.str());
  rc_t rc = doColOp(bucket, key,
                    LambdaFlags::DONT_CREATE_OR_TRIGGER,
                    info,
                    [] (LocalKVRow &row, LocalKVCell &col, bool previously_existed) {
                        if(row.getAvailability()==Availability::Requested) return KELPIE_WAITING;
                        return (previously_existed) ? KELPIE_OK : KELPIE_ENOENT;
                    });
  if((rc==KELPIE_WAITING) || (!info)) return rc;

  //Check the findings. Wildcards stats don't get generated until after the above lambda is called
  return (info->row_num_columns!=0) ? KELPIE_OK : KELPIE_ENOENT;

}


string LocalKV::makeRowname(bucket_t bucket, const Key &key) {
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
LocalKVRow * LocalKV::getRow(const string &full_row_name){

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
void LocalKV::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results){

  faodel::ReplyStream rs(args, "Kelpie LocalKV Status", &results);
  auto it = args.find("detail");
  bool detailed = (it != args.end());
  whookieInfo(rs,detailed);
  rs.Finish();
}

/**
 * @brief Append a ReplyStream with localkv info
 *
 * @param[in] rs The ReplyStream to be written to
 * @param[in] detailed Whether to include detailed information or not
 */
void LocalKV::whookieInfo(faodel::ReplyStream &rs, bool detailed){

  rs.tableBegin("LocalKV");
  rs.tableTop({"Parameter","Setting"});
  rs.tableRow({"Configured:", (configured)?"True":"False"});
  rs.tableRow({"Current Rows:", to_string(rows.size())});
  rs.tableEnd();

  if(!detailed){
  //  rs.mkText(html::mkLink("Full LKV Details", "/kelpie/lkv&detail"));
  //} else {

    //Print row summaries
    table_mutex->ReaderLock();
    rs.tableBegin("LocalKV Row Summary");
    rs.tableTop({"FullRowID","RowName","NumCols","FirstColumn", "RowBytes"});
    for(auto rname_rptr : rows){
      rname_rptr.second->lock();
      object_info_t info;
      rname_rptr.second->getInfo(Key(rname_rptr.first), &info);
      string cname=rname_rptr.second->getFirstColumnName();
      if(cname.empty()) cname=html::mkLink("(noname)",       "/kelpie/lkv/cell&row="+rname_rptr.first);
      else              cname=html::mkLink(cname, "/kelpie/lkv/cell&row="+rname_rptr.first+"&col="+cname);

      rs.tableRow( {
            rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname,"/kelpie/lkv/row&row="+rname_rptr.first),
            to_string(info.row_num_columns),
            cname,
            to_string(info.row_user_bytes)} );

      rname_rptr.second->unlock();
    }
    rs.tableEnd();
    table_mutex->Unlock();

  } else {
    //Print each column in its own row
    object_info_t info;

    table_mutex->ReaderLock();
    rs.tableBegin("LocalKV Full Details");
    rs.tableTop({"FullRowID","RowName","ColumnName","ColBytes","Dependencies"});
    for(auto rname_rptr : rows){
      rname_rptr.second->lock();

      //Pull out the default, no-name column
      if(rname_rptr.second->col_single){
        rname_rptr.second->col_single->getInfo(&info);
        rs.tableRow( {
            rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname,"/kelpie/lkv/row&row="+rname_rptr.first),
            html::mkLink("(noname)","/kelpie/lkv/cell&row="+rname_rptr.first),
            to_string(info.col_user_bytes),
            to_string(info.col_dependencies)});

      }
      //Now look at all named columns
      for(auto cname_cptr : rname_rptr.second->cols) {
        info.Wipe();
        cname_cptr.second->getInfo(&info);

        rs.tableRow( {
            rname_rptr.first,
            html::mkLink(rname_rptr.second->rowname, "/kelpie/lkv/row&row="+rname_rptr.first),
            html::mkLink(cname_cptr.first, "/kelpie/lkv/cell&row="+rname_rptr.first+"&col="+cname_cptr.first),
            to_string(info.col_user_bytes),
            to_string(info.col_dependencies)});
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
void LocalKV::HandleWhookieRow(const std::map<std::string,std::string> &args, std::stringstream &results){

  faodel::ReplyStream rs(args, "Kelpie LocalKV Row", &results);
  string rname;
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
    object_info_t info;
    row->getInfo(Key(rname), &info);

    rs.tableBegin("Row Info");
    rs.tableTop({"Parameter","Setting"});
    rs.tableRow({"Row Name:", row->rowname});
    rs.tableRow({"Total Bytes:", to_string(info.row_user_bytes)});
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
void LocalKV::HandleWhookieCell(const std::map<std::string,std::string> &args, std::stringstream &results){

  faodel::ReplyStream rs(args, "Kelpie LocalKV Cell", &results);
  string rname, cname;
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

      rs.tableBegin("Column Entry " + rname_txt + " " + cname_txt);
      rs.tableTop({"Parameter", "Setting"});
      //rs.tableRow({"Row Name", row->rowname});
      rs.tableRow({"Row Name", html::mkLink(row->rowname, "/kelpie/lkv/row&row=" + rname)});
      rs.tableRow({"Column Name", cname_txt});
      rs.tableRow({"Availability:", availability_to_string(col->availability)});
      rs.tableRow({"Dependencies", to_string(col->getNumDependencies())});
      rs.tableRow({"Object Meta Size", to_string(msize)});
      rs.tableRow({"Object Data Size", to_string(dsize)});
      rs.tableRow({"Object User Capacity", to_string(col->ldo.GetUserCapacity())});
      rs.tableRow({"Total Allocation", to_string(col->ldo.GetRawAllocationSize())});
      rs.tableRow({"Local RefCount", to_string(col->ldo.internal_use_only.GetRefCount())});

#if 0
      {
        net::NetBufferLocal  *nbl = nullptr;
        net::NetBufferRemote  nbr;
        net::GetRdmaPtr(&col->ldo, &nbl, &nbr);
        rs.tableRow({"RDMA Pointer", nbr.str()});
      }
#endif
      rs.tableEnd();

      lunasa::DumpDataObject(col->ldo, rs);
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
    for(auto &name_rowptr : rows) {
      name_rowptr.second->sstr(ss,depth-1, indent+1);
    }
  }
}

}  // namespace kelpie
