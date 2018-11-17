// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_POOLBASE_HH
#define KELPIE_POOLBASE_HH

#include <string>
#include <vector>
#include <atomic>
#include <future>
#include <inttypes.h>

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"

#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/pools/Pool.hh"
#include "kelpie/ioms/IomBase.hh"

#include "lunasa/DataObject.hh"



namespace kelpie {

class Kelpie;  //Forward reference

/**
 * @brief The base class for an implementation of a Pool (client-side handle)
 *
 * PoolBase is the underlying base class for a pool. Users may publish data
 * to the pool, request data (Want/Need), get info about an item, or signify
 * an item should be dropped. Each pool derived from this class is expected
 * to implement different behaviors for each function.
 *
 */
class PoolBase
        : public faodel::InfoInterface,
          public faodel::LoggingInterface {

public:
  PoolBase(faodel::ResourceURL const &pool_url);

  ~PoolBase() override { }  //Fill in if creates anything

  virtual rc_t Publish(const Key &key, fn_publish_callback_t callback) = 0;
  virtual rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, fn_publish_callback_t callback) = 0;

  virtual rc_t Want(const Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback) = 0;     //Notify when available

  virtual rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo) = 0;  //Block until get

  virtual rc_t Info(const Key &key,  kv_col_info_t *col_info) = 0;
  virtual rc_t RowInfo(const Key &key,  kv_row_info_t *row_info) = 0;
  virtual rc_t Drop(const Key &key) = 0;

  virtual int FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr) = 0;

  virtual std::string TypeName() const = 0;


  faodel::bucket_t GetBucket() { return default_bucket; }
  faodel::ResourceURL GetURL() { return pool_url; }
  faodel::DirectoryInfo GetDirectoryInfo() { return dir_info; }
  pool_behavior_t GetBehavior() { return behavior_flags; }
  std::string GetIomName(bool use_web_formatting=false, bool add_details=false);
  iom_hash_t GetIomHash() { return iom_hash; }

protected:

  faodel::nodeid_t my_nodeid;
  faodel::bucket_t default_bucket;
  faodel::ResourceURL pool_url;
  faodel::DirectoryInfo dir_info;

  LocalKV *lkv;  //Not allocated here
  internal::IomBase *iom;  //Not allocated here - for local reference
  iom_hash_t iom_hash; //For remote reference

  pool_behavior_t behavior_flags;

  std::string pool_type;  //Which class this resolves to

};

}  // namespace kelpie

#endif  // KELPIE_POOLBASE_HH
