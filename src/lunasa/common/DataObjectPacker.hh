// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_DATAOBJECTPACKER_HH
#define FAODEL_DATAOBJECTPACKER_HH

#include <string>
#include <vector>

#include "faodel-common/StringHelpers.hh"
#include "lunasa/DataObject.hh"

namespace lunasa {

class DataObjectPacker {

public:
  DataObjectPacker(const std::vector<std::string>  &names,
                   const std::vector<const void *> &ptrs,
                   const std::vector<size_t>       &bytes,
                   const std::vector<uint8_t>      &types,
                   uint32_t                        data_type_hash,
                   int                             dop_format_version=1,
                   DataObject::AllocatorType       memory_type = DataObject::AllocatorType::eager);

  DataObjectPacker(size_t                         max_data_capacity,
                   uint32_t                       data_type_hash,
                   int                            dop_format_version=1,
                   DataObject::AllocatorType      memory_type = DataObject::AllocatorType::eager);

  explicit DataObjectPacker(const DataObject &ldo);


  static size_t GetEntryOverhead(int dop_format_version);
  static size_t ComputeEntrySize(int dop_format_version, const std::string &name, uint32_t data_bytes);
  size_t ComputeEntrySize(const std::string &name, uint32_t data_bytes) const { return ComputeEntrySize(version, name, data_bytes); }


  bool VerifyDataType(uint32_t expected_data_type_hash);

  uint32_t RemainingCapacity();
  int AppendVariable(const std::string &name, void *data_ptr, size_t data_bytes, uint8_t type);

  int GetVarPointer(const std::string &name, void **raw_ptr_to_data, size_t *bytes, uint8_t *type);
  int GetVarPointer(uint32_t hash, void **raw_ptr_to_data, size_t *bytes, uint8_t *type);

  int GetVarNames(std::vector<std::string> *names);

  DataObject GetDataObject();

  const static uint16_t object_type_id;
  const static std::string  object_type_name;

private:

  struct dop_meta_t {
    uint32_t num_vars;         //!< How many variables are stored in this DataObject
    uint32_t data_type_hash;   //!< Unique id for this data (eg const_hash32("my special data"))
    uint8_t  packing_version;  //!< Which format was used to pack the data
  };

  //Version 1: Keep first 255 bytes of name and a short hash
  struct dop_entry_v1_t {
    uint8_t name_length;
    uint8_t data_type;
    uint16_t name_hash;
    uint32_t data_length;
    uint8_t  name_data[0]; //non-compliant convenience to get to name and data

    size_t GetTotalSize() const { return sizeof(dop_entry_v1_t) + name_length + data_length; }
    std::string GetName() { return std::string(reinterpret_cast<char *>(name_data), name_length); }
    uint8_t * GetDataPtr() { return &name_data[name_length]; }
    bool MatchesName(const std::string &name) {
      auto my_name = GetName();
      if(my_name == name) return true;
      return ((name_length==255)&&(faodel::StringBeginsWith(name, my_name))); //Truncated string
    }
  };

  //Version 2: We only send the hash (no name)
  struct dop_entry_v2_t {
    uint32_t hash;
    uint32_t data_length;
    uint8_t  data_type;
    uint8_t  pad[3];  //Pad to force alignment.
    uint8_t  data[0]; //non-compliant convenience to get to name and data

    size_t GetTotalSize() const { return sizeof(dop_entry_v2_t) + data_length; }
    uint8_t * GetDataPtr() { return &data[0]; }
  };



  bool finalized;  //!< Prevent user from appending new variables
  uint8_t version; //!< Which packing version is being used
  DataObject ldo;  //!< The object managed by this Packer. Users call GetDataObject() when done packing


  std::multimap<uint32_t, void *> index; //!< Optional index for locating items


  size_t getEntryOverhead();
  size_t computeEntrySize(const std::string &name, uint32_t data_bytes) { return ComputeEntrySize(version, name, data_bytes); }

  size_t writeEntry(uint8_t *payload_ptr,
                    const std::string &name, uint8_t type,
                    const void *data_ptr, uint32_t data_bytes);

  void rebuildIndexIfNeeded();

};

};

#endif //FAODEL_DATAOBJECTPACKER_HH
