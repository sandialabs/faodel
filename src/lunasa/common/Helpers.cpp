// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstring>

#include "faodel-common/StringHelpers.hh"
#include "lunasa/common/Helpers.hh"
#include "lunasa/DataObject.hh"

using namespace std;

namespace lunasa {

const uint16_t string_object_type_id = faodel::const_hash16("StringObject");

DataObject AllocateStringObject(const std::string &s) {
  DataObject ldo(s.size());
  ldo.SetTypeID(string_object_type_id);
  memcpy(ldo.GetDataPtr(), s.c_str(), s.size());
  return ldo;
}

std::string UnpackStringObject(DataObject &ldo) {
  if(ldo.GetTypeID()!=string_object_type_id) return "";
  return std::string(ldo.GetDataPtr<char *>(), ldo.GetDataSize());
}



} // namespace lunasa
