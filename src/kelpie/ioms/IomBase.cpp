// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <iostream>

#include "kelpie/ioms/IomBase.hh"


using namespace std;


namespace kelpie {
namespace internal {

void IomBase::finish() {
  //Default: no cleanup ops necessary
}

/**
 * @brief Get info about a collection of keys on this iom (simply iterates over all)
 * @param[in] bucket The bucket to search in
 * @param[in] keys Vector of keys to find
 * @param[out] infos The return list of object info (in key order)
 * @retval KELPIE_OK All requests were successful
 * @retval error One or more requests were not successful. Last unsuccessful error is returned
 */
rc_t IomBase::GetInfo(faodel::bucket_t bucket, const std::vector<kelpie::Key> &keys, vector<object_info_t> *infos) {
  rc_t rc = KELPIE_OK;
  for(auto &k : keys) {
    object_info_t info;
    rc_t rc2 = GetInfo(bucket, k, &info);
    if(rc2!=KELPIE_OK) rc=rc2;
    if(infos) infos->emplace_back(info);
  }
  return rc;
}

/**
 * @brief Write out a collection of key/value pairs (iterates on WriteObject)
 * @param bucket The bucket to prepend keys with
 * @param items Vector of keys/blobs
 */
void IomBase::WriteObjects(faodel::bucket_t bucket, const vector<pair<Key, lunasa::DataObject>> &items) {
  for(auto &k_v : items){
    WriteObject(bucket, k_v.first, k_v.second);
  }
}

/**
 * @brief Read in many objects at a time
 * @param bucket The bucket to search in
 * @param keys A vector of keys to request
 * @param found_objects A vector containing all the found key/blob pairs
 * @param missing_keys An optional list of keys that were not found
 * @retval KELPIE_OK All items were retrieved
 * @retval KELPIE_RECHECK One or more items were missing
 */
rc_t IomBase::ReadObjects(faodel::bucket_t bucket, const vector<Key> &keys,
                          vector<pair<Key, lunasa::DataObject>> *found_objects,
                          vector<Key> *missing_keys) {
  rc_t return_rc = KELPIE_OK;
  for(auto &k : keys) {
    lunasa::DataObject ldo;
    rc_t rc = ReadObject(bucket, k, &ldo);
    if(rc==KELPIE_OK) {
      if(found_objects) {
        found_objects->push_back(std::pair<kelpie::Key, lunasa::DataObject>(k,std::move(ldo)));
      }
    } else {
      return_rc = KELPIE_RECHECK; //Some items may be here
      if(missing_keys) {
        missing_keys->push_back(k);
      }
    }
  }
  return return_rc;
}

rc_t IomBase::ListObjects(faodel::bucket_t bucket, Key key, ObjectCapacities *oc) {
  // !!!! DEBUG !!!!
  printf("WARNING: Current IOM (%s) does NOT support ListObjects() method\n", Name().c_str()); 
  return KELPIE_ENOENT;
}

/**
 * @brief Get a particular setting that was passed in during creation
 * @param setting_name The name of the iom setting to search for
 * @return The value, or empty string
 */
string IomBase::Setting(string setting_name) const {
  faodel::ToLowercaseInPlace(setting_name);
  auto x=settings.find(setting_name);
  if(x==settings.end())
    return "";
  return x->second;
}


} // namespace internal
} // namespace kelpie
