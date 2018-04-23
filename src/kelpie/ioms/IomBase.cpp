// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "kelpie/ioms/IomBase.hh"


using namespace std;


namespace kelpie {
namespace internal {

void IomBase::finish() {
  //Default: no cleanup ops necessary
}

rc_t IomBase::GetInfo(faodel::bucket_t bucket, const std::vector<kelpie::Key> &keys, vector<kv_col_info_t> *col_infos) {
  rc_t rc = KELPIE_OK;
  for(auto &k : keys) {
    kv_col_info_t col_info;
    rc_t rc2 = GetInfo(bucket, k, &col_info);
    if(rc2!=KELPIE_OK) rc=rc2;
    if(col_infos) col_infos->push_back(col_info);
  }
  return rc;
}

//Helper: default helper for writing out many
void IomBase::WriteObjects(faodel::bucket_t bucket, const vector<pair<Key, lunasa::DataObject>> &items) {
  for(auto &k_v : items){
    WriteObject(bucket, k_v.first, k_v.second);
  }
}

//Helper: default helper for reading in many
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

string IomBase::Setting(string setting_name) const {
  faodel::ToLowercaseInPlace(setting_name);
  auto x=settings.find(setting_name);
  if(x==settings.end())
    return "";
  return x->second;
}


} // namespace internal
} // namespace kelpie
