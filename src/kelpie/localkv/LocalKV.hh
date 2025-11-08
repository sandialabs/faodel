// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_LOCALKV_HH
#define KELPIE_LOCALKV_HH

#include <cinttypes>
#include <string>
#include <sstream>
#include <memory>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/Configuration.hh"
#include "faodel-common/InfoInterface.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/ReplyStream.hh"

#include "kelpie/Key.hh"
#include "kelpie/localkv/LocalKVTypes.hh"
#include "kelpie/localkv/LocalKVRow.hh"
#include "kelpie/pools/Pool.hh"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"


#include "lunasa/DataObject.hh"



namespace kelpie {

/**
 * The LocalKV provides a flexible local k/blob store that can be used in multiple
 * Kelpie scenarios. The store is a sparse 2D store, where data is located by
 * a key's row/column labels. The 2D aspect of Kelpie is intended for situations
 * where a user needs to group related items together (eg, multiple writers generate
 * different portions of a result, and a reader waits for all portions to be available
 * before fetching the data in one operation). As such it is not intended to be
 * an efficient sparse matrix for random access.
 *
 * Data is stored using a map of maps. The row portion of the key is first used to
 * locate the LocalKVRow that holds the items. The column portion of the key is then
 * used to find the particular LocalKVCell that holds the desired memory block. Because
 * the LocalKV is designed to operate in a multithreaded environment, two locks are
 * employed to protect the hashes. The table_mutex in this class reserves access
 * to the first map. Once the proper LocalKVRow is retrieved, the table_lock is released
 * and a lock is made on the row. The row remains locked until the put/get completes.
 *
 * @brief LocalKV provides a simple 2D Key/Blob store for different Kelpie tasks.
 */
class LocalKV :
    public faodel::InfoInterface,
    public faodel::LoggingInterface {

public:
  LocalKV();
  ~LocalKV() override;

  //One-time configure the lkv
  rc_t Init(const faodel::Configuration &config);


  //Put a reference into local store. Ref should not be modified
  rc_t put(faodel::bucket_t bucket, const Key &key,
                    const lunasa::DataObject &new_ldo,
                    pool_behavior_t behavior_flags,
                    internal::IomBase *iom,
                    object_info_t *info);

  //Get local reference. Do nothing if unavailable
  rc_t get(faodel::bucket_t bucket, const Key &key,
                    lunasa::DataObject *ext_ldo,
                    object_info_t *info);

  //Get multiple references. Do nothing if unavailable
  rc_t getAvailable(faodel::bucket_t bucket, const Key &key,
                    std::map<Key, lunasa::DataObject> &ldos);

  //Get local reference. Leave OpBox mailbox dependency if unavailable
  rc_t getForOp(faodel::bucket_t bucket, const Key &key,
                    opbox::mailbox_t op_mailbox_if_missing,
                    pool_behavior_t behavior_flags,
                    iom_hash_t iom_hash,
                    lunasa::DataObject *ext_ldo,
                    object_info_t *info);


  //Copy the data out of the store and into the new struct
  rc_t getData(faodel::bucket_t bucket, const Key &key,
                    void *mem_ptr, size_t max_size,
                    size_t *copied_size,
                    object_info_t *info);

  //Request a callback be made when an item becomes available
  rc_t wantLocal(faodel::bucket_t bucket, const Key &key,
                    bool caller_will_fetch_if_missing,
                    fn_want_callback_t callback);

  //Drop a particular item
  rc_t drop(faodel::bucket_t bucket, const Key &key_prefix);

  //Locate info about one or more keys
  rc_t list(faodel::bucket_t bucket, const Key &key_prefix,
                    internal::IomBase *iom,
                    ObjectCapacities *object_capacities);


  //Fetch local objects from row, perform computation, return ldo result
  rc_t doCompute(const std::string &function_name, const std::string &args,
                    faodel::bucket_t bucket, const Key &key,
                    lunasa::DataObject *ext_ldo);

  //Get info for a particular item
  rc_t getInfo(faodel::bucket_t bucket, const Key &key, object_info_t *info);

  //Workhorse: Does all the work of finding a column and doing a lambda on it
  rc_t doColOp(faodel::bucket_t bucket, const Key &key,
                    lkv::lambda_flags_t flags,
                    object_info_t *info,
                    fn_column_op_t func);

  //Workhorse: Does all the work of finding a row and doing a lambda on it
  rc_t doRowOp(faodel::bucket_t bucket, const Key &key,
                    lkv::lambda_flags_t flags,
                    object_info_t *info,
                    fn_row_op_t func);



  //Internal: Delete everything
  void wipeAll(faodel::internal_use_only_t iuo);

  //Whookie helpers
  void HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieRow(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieCell(const std::map<std::string,std::string> &args, std::stringstream &results);
  void whookieInfo(faodel::ReplyStream &rs, bool detailed=false);

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

private:
  faodel::MutexWrapperTypeID row_mutex_type_id;  //!< The mutex type to use for creating new rows
  bool configured;                               //!< Whether LovalKV has been Configured yet

  std::map<std::string, LocalKVRow *> rows;      //!< Map for storing each row's LocalKVRow
  faodel::MutexWrapper *table_mutex;             //!< Mutex needed for controlling single-access to the top-level row map

  std::string makeRowname(faodel::bucket_t bucket, const Key &key);
  LocalKVRow * getRow(const std::string &full_row_name);



};

}  // namespace kelpie


#endif  // KELPIE_LOCALKV_HH
