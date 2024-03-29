// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_POOL_HH
#define KELPIE_POOL_HH

#include <memory>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/InfoInterface.hh"
#include "faodel-common/ResourceURL.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"
#include "opbox/net/net.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/pools/ResultCollector.hh"

namespace kelpie {

class PoolBase;  //forward reference

/**
 * @brief A handle the user can use to access a particular Kelpie pool
 *
 * This class is a lightweight handle the user can use to interact with a
 * a pool. This class contains a reference counted pointer to an
 * implementation and simply provides shortcuts into that implementation.
 */
class Pool :
    public faodel::InfoInterface {

public:
  Pool();
  Pool(faodel::internal_use_only_t iuo, std::shared_ptr<PoolBase> base); //Creation
  Pool(const Pool &source);  //Shallow copy
  Pool(Pool &&source);       //Move
  ~Pool() override = default;
  
  Pool& operator=(const Pool&); //For Shallow copy
  Pool& operator=(Pool&&);      //For Move
  
  //Core functions
  rc_t Publish(const Key &key, const fn_publish_callback_t &callback);                                     //Lookup locally and publish externally
  rc_t Publish(const Key &key, ResultCollector &collector);                                                //Async local publish that notifies a ResultCollector

  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo);                                        //Blocking publish
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, object_info_t *info);                   //Blocking publish

  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, const fn_publish_callback_t &callback); //Async publish
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, ResultCollector &collector);            //Async publish that notifies a ResultCollector

  rc_t Want(const Key &key, const fn_want_callback_t &callback={});                                        //Notify when available
  rc_t Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback={});        //Notify when available
  rc_t Want(const Key &key, ResultCollector &collector);                                                   //Notify collector when available
  rc_t Want(const Key &key, size_t expected_ldo_user_bytes, ResultCollector &collector);                   //Notify collector when available

  rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo);             //Block until get
  rc_t Need(const Key &key, lunasa::DataObject *returned_ldo) { return Need(key, 0, returned_ldo); }       //Block until get

  rc_t Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback);    //Async compute
  rc_t Compute(const Key &key, const std::string &function_name, const std::string &function_args, ResultCollector &collector);               //Async compute w/ ResultCollector
  rc_t Compute(const Key &key, const std::string &function_name, const std::string &function_args, lunasa::DataObject *returned_ldo=nullptr); //Blocking compute

  rc_t Info(const Key &key, object_info_t *info);
  rc_t RowInfo(const Key &key, object_info_t *info);

  rc_t Drop(const Key &key, fn_drop_callback_t callback=nullptr);
  rc_t BlockingDrop(const Key &key);

  rc_t List(const Key &search_key, ObjectCapacities *object_capacities=nullptr);

  //Locate where this pool would store the data
  int FindTargetNode(const Key &key, faodel::nodeid_t *node_id=nullptr, net::peer_ptr_t *peer_ptr=nullptr);

  //Determine if this pool was created correctly
  bool Valid(std::string *error_message=nullptr);
  void ValidOrDie();


  //Get info on Default settings
  faodel::bucket_t GetBucket();
  faodel::ResourceURL GetURL();
  faodel::DirectoryInfo GetDirectoryInfo();
  kelpie::pool_behavior_t GetBehavior();
  iom_hash_t GetIomHash();
  int GetRefCount();



  //Comparisons
  bool operator==(const Pool &other) const {
    return (this->impl == other.impl);
  }
  bool operator!=(const Pool &other) const {
    return (this->impl != other.impl);
  }


  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

private:
  std::shared_ptr<PoolBase> impl;

};

}  // namespace kelpie

#endif  // KELPIE_POOL_HH
