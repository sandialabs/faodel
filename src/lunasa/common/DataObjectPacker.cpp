// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <algorithm>
#include <iostream>
#include <cstring> //memcpy
#include "faodel-common/StringHelpers.hh"
#include "lunasa/common/DataObjectPacker.hh"

using namespace std;

namespace lunasa {

//Set the constants for this DataObject so they'll be recognizes
const string DataObjectPacker::object_type_name = "DataObjectPacker";
const uint16_t DataObjectPacker::object_type_id = faodel::const_hash16("DataObjectPacker");


DataObjectPacker::DataObjectPacker(const vector<std::string> &names,
                                   const vector<const void *> &ptrs,
                                   const vector<size_t> &bytes, const vector<uint8_t> &types,
                                   uint32_t data_type_hash,
                                   int dop_format_version,
                                   lunasa::DataObject::AllocatorType memory_type)
  : finalized(true), version(dop_format_version) {

  bool using_null_ptrs = ptrs.empty(); //Allow user to come back and fill in data later

  //Make sure all our vectors are the same size
  if (  (!using_null_ptrs && (names.size()!=ptrs.size()) ) ||
        (names.size() != bytes.size()) ||
        (bytes.size() != types.size()) ) {
    throw std::runtime_error("DataObjectPacker given vectors of different sizes");
  }

  if((dop_format_version<1) || (dop_format_version>2))
    throw std::runtime_error("DataObjectPacker constructed with an invalid format version number");

  //Figure out how much space we'll need
  size_t payload_size=0;
  for(int i=0; i<names.size(); i++) {
    payload_size += computeEntrySize(names[i], bytes[i]);
  }

  //Allocate the LDO
  ldo = DataObject(sizeof(dop_meta_t), payload_size, memory_type, DataObjectPacker::object_type_id);

  //Fill in the metadata
  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();
  meta->num_vars = names.size();
  meta->packing_version = version;
  meta->data_type_hash = data_type_hash;

  //Pack the payload (using specified version)
  auto *payload = ldo.GetDataPtr<uint8_t *>();
  for(int i=0; i<names.size(); i++) {
    size_t entry_size = writeEntry(payload, names[i], types[i],
                                   ((using_null_ptrs) ? nullptr : ptrs[i]),
                                   bytes[i]);
    payload+=entry_size;
  }

}
/**
 * @brief Ctor for situation where user wants to allocate a max capacity and then fill in variables
 * @param[in] max_data_capacity Max amount of space to be allocated for packed data
 * @param[in] data_type_hash A user tag for this data (eg hash("MyParticleData)")
 * @param[in] dop_format_version Which version of the packer to use
 * @param[in] memory_type The Lunasa memory allocation strategy
 */
DataObjectPacker::DataObjectPacker(size_t max_data_capacity, uint32_t data_type_hash, int dop_format_version,
                                   DataObject::AllocatorType memory_type)
  : finalized(false), version(dop_format_version) {

  if((dop_format_version<1) || (dop_format_version>2))
    throw std::runtime_error("DataObjectPacker constructed with an invalid format version number");


  ldo = DataObject(sizeof(dop_meta_t) + max_data_capacity, sizeof(dop_meta_t), 0, memory_type, DataObjectPacker::object_type_id);

  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();
  meta->num_vars = 0;
  meta->packing_version = dop_format_version;
  meta->data_type_hash = data_type_hash;

}


/**
 * @brief Ctor for when a user wants to parse an incoming DataObject
 * @param[in] ldo The DataObject that will be parsed
 */
DataObjectPacker::DataObjectPacker(const lunasa::DataObject &ldo)
        : ldo(ldo), finalized(true) {

  if(ldo.GetTypeID() != DataObjectPacker::object_type_id) {
    throw std::runtime_error("DataObjectPacker asked to parse a DataObject that does not match its TypeID");
  }
  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();
  version = meta->packing_version;
  if((version < 1) || (version >2))
    throw std::runtime_error("DataObjectPacker asked to parse DataObject with invalid packing version number");

}

/**
 * @brief Inspect the DataObject's metadata and see if the data type hash matches an exected value
 * @param[in] expected_data_type_hash The hash you expect this to be (eg const_hash32("my special data"))
 * @retval True Hashes match
 * @retval False The hashes don't match
 */
bool DataObjectPacker::VerifyDataType(uint32_t expected_data_type_hash) {
  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();
  if(expected_data_type_hash != meta->data_type_hash) return false;
  return true;
}



/**
 * @brief Determine how much space a variable will require (overhead+data), given a particular format version
 * @param[in] dop_format_version The packing format version to use
 * @param[in] name The variable name
 * @param[in] data_bytes How many bytes the data is
 * @return EntrySize How many bytes all of this structure will use in
 */
size_t DataObjectPacker::ComputeEntrySize(int dop_format_version, const string &name, uint32_t data_bytes) {
  switch(dop_format_version) {
    case 1: return sizeof(dop_entry_v1_t) + std::min(name.size(), size_t(255)) + data_bytes;
    case 2: return sizeof(dop_entry_v2_t) + data_bytes;
    default:
      throw std::runtime_error("Attempted to pack with unknown version number: "+std::to_string(dop_format_version));
  }
}

/**
 * @brief Determine how much space is left in this allocation for additional variables
 * @return bytes Raw space left (note: user must take into account Entry Overhead!)
 */
uint32_t DataObjectPacker::RemainingCapacity() {
  if(finalized) return 0;

  uint32_t dspace_left = ldo.GetUserCapacity() - (ldo.GetMetaSize() + ldo.GetDataSize());
  if(dspace_left < getEntryOverhead()) return 0;

  return dspace_left;
}

/**
 * @brief Append a variable to the data section of a DataObject when Capacity is available
 * @param[in] name The name of the variable
 * @param[in] data_ptr Pointer to the variable to be copied in (nullptr = no copy)
 * @param[in] data_bytes How many bytes of data there are for this variable
 * @param[in] type The data type ID a user has assigned to this variable
 * @retval 0 Success: Variable was appended successfully
 * @retval -1 Failure: Either the object is finalized or there wasn't enough capacity left for the data
 * @note: This function is only for GenericPackers allocated using the max capacity ctor
 * @note: This function does not check to see if you inserted the same variable name multiple times
 */
int DataObjectPacker::AppendVariable(const string &name, void *data_ptr, size_t data_bytes, uint8_t type) {

  //Bail out if we don't allow modifications
  if(finalized) return -1;

  //Figure out how big this is and where it goes
  size_t entry_size = computeEntrySize(name, data_bytes);
  uint32_t offset = ldo.GetDataSize();

  //Try to adjust the data size. Exit if no room left
  int rc = ldo.ModifyUserSizes(sizeof(dop_meta_t), offset + entry_size);
  if(rc < 0) return -1; //No capacity left, ldo did not update

  auto *entry_ptr = ldo.GetDataPtr<uint8_t *>() + offset;
  writeEntry(entry_ptr, name, type, data_ptr, data_bytes);

  //Update the metadata
  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();
  meta->num_vars++;

  return 0;
}


/**
 * @brief Locate a variable by name in this ldo and return the raw pointer to its data
 * @param[in] name The name of the variable to lookup
 * @param[out] raw_ptr_to_data Pointer to the variable's data inside the DataObject
 * @param[out] bytes How many bytes of data there are for this variable
 * @param[out] type The data type ID users assigned to this variable
 * @retval 0 Found
 * @retval ENOENT Item not found
 * @note: It may be possible for the names of two variables to collide due to truncation and hashing
 */
int DataObjectPacker::GetVarPointer(const std::string &name, void **raw_ptr_to_data, size_t *bytes, uint8_t *type) {

  //First, Hash the input string. This hash varies depending on version
  uint32_t hash;
  switch(version){
    case 1: hash = faodel::hash16(name); break;
    case 2: hash = faodel::hash32(name); break;
  }

  //Second, build the index if we haven't already done so
  rebuildIndexIfNeeded();


  //Next, Look it up in the map
  typedef std::multimap<uint32_t, void *>::iterator MMiterator;
  std::pair<MMiterator, MMiterator> result = index.equal_range(hash);
  for(MMiterator it = result.first; it != result.second; ++it) {

    switch(version) {
      case 1:
      {
        auto *ptr = reinterpret_cast<dop_entry_v1_t *>(it->second);
        if(ptr->MatchesName(name)) {
          if(raw_ptr_to_data) *raw_ptr_to_data = reinterpret_cast<void *>(ptr->GetDataPtr());
          if(bytes) *bytes = ptr->data_length;
          if(type) *type = ptr->data_type;
          return 0;
        }
      }
        break;
      case 2:
      {
        auto *ptr = reinterpret_cast<dop_entry_v2_t *>(it->second);
        if(ptr->hash == hash) {
          if(raw_ptr_to_data) *raw_ptr_to_data = reinterpret_cast<void *>(ptr->GetDataPtr());
          if(bytes) *bytes = ptr->data_length;
          if(type) *type = ptr->data_type;
          return 0;
        }
      }
    }
  }
  //Not found, clear out results
  if(raw_ptr_to_data) *raw_ptr_to_data = nullptr;
  if(bytes) *bytes=0;
  if(type) *type=0;
  return ENOENT;
}

/**
 * @brief Retrieve a variable when using version 2 packing format
 * @param[in] hash The hash of the variable name to be retrieved
 * @param[out] raw_ptr_to_data Pointer to the variable's data inside the DataObject
 * @param[out] bytes How many bytes of data there are for this variable
 * @param[out] type The data type ID users assigned to this variable
 * @retval 0 Found
 * @retval ENOENT Item not found
 * @retval EINVAL Could not search on hash because data format is not version 2
 */
int DataObjectPacker::GetVarPointer(uint32_t hash, void **raw_ptr_to_data, size_t *bytes, uint8_t *type) {

  //Version 1 only has a 16b hash and is therefore not suitable. Bail out
  if(version!=2) return EINVAL;

  //First, build the index if we haven't already done so
  rebuildIndexIfNeeded();

  //Next, Look it up in the map
  typedef std::multimap<uint32_t, void *>::iterator MMiterator;
  std::pair<MMiterator, MMiterator> result = index.equal_range(hash);
  for(MMiterator it = result.first; it != result.second; ++it) {
    switch(version) {
      case 2:
      {
        auto *ptr = reinterpret_cast<dop_entry_v2_t *>(it->second);
        if(ptr->hash == hash) {
          if(raw_ptr_to_data) *raw_ptr_to_data = reinterpret_cast<void *>(ptr->GetDataPtr());
          if(bytes) *bytes = ptr->data_length;
          if(type) *type = ptr->data_type;
          return 0;
        }
      }
    }
  }
  if(raw_ptr_to_data) *raw_ptr_to_data = nullptr;
  if(bytes) *bytes=0;
  if(type) *type=0;
  return ENOENT;
}



/**
 * @brief Get a list of all (truncated) variable names from a DataObject, when using version 1 format
 * @param[out] names All of the truncated names
 * @retval 0 Finished without errors
 * @retval EINVAL Tried to run when not using version 1 format
 */
int DataObjectPacker::GetVarNames(std::vector<std::string> *names) {
  if (version != 1) return EINVAL;
  if(!names) return 0;

  rebuildIndexIfNeeded();

  for(auto &mm : index) {
    auto ptr = reinterpret_cast<dop_entry_v1_t *>(mm.second);
    names->push_back(ptr->GetName());
  }

  return 0;
}

/**
 * @brief Get the DataObject that this packer maintains
 * @return DataObject
 *
 * Pass back a (correctly ref counted) handle to the user about the DataObject
 * this packer maintains.
 */
DataObject DataObjectPacker::GetDataObject() {
  return ldo;
}

/**
 * @brief Recompute an Index for looking up variables in the DataObject if needed
 *
 * This function will walk through the current DataObject and insert the hash/ptr for
 * each item into a multimap index.
 */
void DataObjectPacker::rebuildIndexIfNeeded() {

  auto *meta = ldo.GetMetaPtr<dop_meta_t *>();

  if(index.size() == meta->num_vars) return;

  //We're rebuilding the index
  index.clear();

  //Make sure we don't get out of data section. Find location of last possible entry
  uint32_t max_offset = ldo.GetDataSize();
  switch(version) {
    case 1: max_offset -= sizeof(dop_entry_v1_t); break;
    case 2: max_offset -= sizeof(dop_entry_v2_t); break;
  }

  auto *payload = ldo.GetDataPtr<uint8_t *>();
  size_t offset = 0;
  for(int i=0; i < meta->num_vars; i++) {
    if(offset > max_offset) {
      throw std::runtime_error("Building index failed: exceeded boundaries for DataObject");
    }
    switch(version) {
      case 1:
      {
        auto *entry = reinterpret_cast<dop_entry_v1_t *>(payload);
        index.insert( std::pair<uint32_t,void*>(entry->name_hash, entry));
        payload += entry->GetTotalSize();
        break;
      }
      case 2:
      {
        auto *entry = reinterpret_cast<dop_entry_v2_t *>(payload);
        index.insert( std::pair<uint32_t,void*>(entry->hash, entry));
        payload += entry->GetTotalSize();
      }
    }

  }

}


/**
 * @brief Return information about how much overhead is needed for each variable that is added
 * @return OverheadSize The size in bytes of the overhead for the packing format being used
 */
size_t DataObjectPacker::getEntryOverhead() {
  switch(version) {
    case 1: return sizeof(dop_entry_v1_t);
    case 2: return sizeof(dop_entry_v2_t);
  }
  throw std::runtime_error("Unknown GeneralPacker format version");
}

/**
 * @brief Write entry data to a user-provided address
 * @param[in] payload_ptr Where to write this entry
 * @param[in] name Variable's name
 * @param[in] type What type of data this is
 * @param[in] data_ptr Source data for variable (nullptr = don't copy)
 * @param[in] data_bytes How long the data is
 * @return EntryBytes The total length of this entry (structure+data)
 */
size_t DataObjectPacker::writeEntry(uint8_t *payload_ptr, const string &name, uint8_t type, const void *data_ptr, uint32_t data_bytes) {

  switch(version) {
    case 1:
    {
      dop_entry_v1_t *entry = reinterpret_cast<dop_entry_v1_t *>(payload_ptr);
      entry->name_length = std::min(name.size(), size_t(255));
      entry->data_type = type;
      entry->name_hash = faodel::hash16(name);
      entry->data_length = data_bytes;
      memcpy(&entry->name_data[0], name.c_str(), entry->name_length);
      if(data_ptr) memcpy(entry->GetDataPtr(), data_ptr, data_bytes);
      return entry->GetTotalSize();
    }
    break;
    case 2:
    {
      dop_entry_v2_t *entry = reinterpret_cast<dop_entry_v2_t *>(payload_ptr);
      entry->hash = faodel::hash32(name);
      entry->data_length = data_bytes;
      entry->data_type = type;
      if(data_ptr) memcpy(&entry->data[0], data_ptr, data_bytes);
      return entry->GetTotalSize();
    }
    break;
    default:
      throw std::runtime_error("Attempted to pack with unknown version number: "+std::to_string(version));
  }

}




} //namespace lunasa
