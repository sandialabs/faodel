// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_GENERICDATABUNDLE_HH
#define FAODEL_GENERICDATABUNDLE_HH

#include <cstdint>
#include <string.h> //memcpy

#include "lunasa/DataObject.hh"

namespace lunasa {

struct bundle_offsets_t {
  uint32_t max_payload_bytes;   //How much data can be stored in the LDO's data section
  uint32_t current_byte_offset; //Where we are in appending to/reading from payload
  uint32_t current_id;          //Which item we're currently at

  bundle_offsets_t() : max_payload_bytes(0), current_byte_offset(0), current_id(0) {
  }
  bundle_offsets_t(DataObject *ldo) : bundle_offsets_t() {
    max_payload_bytes = ldo->GetDataSize();
  }
};

/**
 * @brief A bundle is a collection of variable length data values, packed into a DataObject
 * Often users need a way to ship a large number of variable-length blobs. This template
 * provides a simple way to pack those items into an LDO. Users may provide their own
 * meta data section in this template (which takes away from the number of items
 * @note Each data item must be less than 64KB
 */
template<class EXTRA_META_TYPE>
struct GenericRandomDataBundle {
  uint16_t num_items;
  uint16_t pad1; //Reserved
  uint32_t pad2; //Reserved
  EXTRA_META_TYPE meta;
  uint16_t lens[(64*1024-8-(sizeof(uint64_t)+sizeof(EXTRA_META_TYPE)))/sizeof(uint16_t)]; //Avoid last work
  unsigned char packed_data[0];

  void Init() {
    num_items = 0;
  }

  constexpr int GetMaxItems() {
    return sizeof(lens)/sizeof(uint16_t);
  }

  /**
   * @brief Append a new item to the back of the list
   * @param max_payload_bytes Maximum amount of data that can be stored in the data section
   * @param current_byte_offset Where the system is in the packing
   * @param new_data Pointer to the new data
   * @param new_data_len How large the new item is
   * @retval TRUE Item could fit and was added
   * @retval FALSE Item was not added because it would exceed capacity
   */
  bool AppendBack(const uint32_t max_payload_bytes, uint32_t &current_byte_offset, const unsigned char *new_data, uint16_t new_data_len) {

    if(num_items+1> GetMaxItems()) return false;
    if(current_byte_offset + new_data_len > max_payload_bytes) return false;

    if(new_data_len>0) //Allow zero-items, but don't use the space
      memcpy(&packed_data[current_byte_offset], new_data, new_data_len);

    lens[num_items] = new_data_len;
    current_byte_offset+=new_data_len;
    num_items++;
    return true;
  }

  /**
   * @brief Append a new item to the back of the list, using a bundle_offset_t to keep the pointers
   * @param bundle_offsets Holds the current pointers into the structure
   * @param new_data Pointer to the new data
   * @param new_data_len How large the new item is
   * @retval TRUE Item could fit and was added
   * @retval FALSE Item was not added because it would exceed capacity
   */
  bool AppendBack(bundle_offsets_t &bundle_offsets, const unsigned char *new_data, uint16_t new_data_len) {
    bool ok= AppendBack(bundle_offsets.max_payload_bytes,
                      bundle_offsets.current_byte_offset,
                      new_data, new_data_len);
    bundle_offsets.current_id = num_items;
    return ok;
  }

   /**
   * @brief Append a new string to the back of the list, using a bundle_offset_t to keep the pointers
   * @param bundle_offsets Holds the current pointers into the structure
   * @param s The string that should be added
   * @retval TRUE Item could fit and was added
   * @retval FALSE Item was not added because it would exceed capacity
   */
  bool AppendBack(bundle_offsets_t &bundle_offsets, const std::string &s) {
    bool ok= AppendBack(bundle_offsets.max_payload_bytes,
                      bundle_offsets.current_byte_offset,
                      (const unsigned char *) s.c_str(), s.size() );
    bundle_offsets.current_id = num_items;
    return ok;
  }

  /**
   * @brief Get the next data pointer from the bundle (and update offsets)
   * @param max_payload Maximum capacity of the data section of LDO
   * @param current_id Which item will be pulled out next
   * @param current_offset Payload offset for next item
   * @param data_ptr Pointer to the item (user must not free. user must not use after ldo deleted)
   * @param data_len Returned size of the item
   * @retval TRUE Item was valid and returned
   * @retval FALSE Item not available in bundle. No data returned
   */
  bool GetNext(const uint32_t max_payload, uint32_t &current_id, uint32_t &current_offset,
               unsigned char **data_ptr, uint16_t *data_len) {

    if(current_id >= num_items) return false;
    if(current_offset+lens[current_id]>max_payload) return false; //Should really just assert

    //Extract the pointer
    *data_len = lens[current_id];
    *data_ptr = (lens[current_id]==0) ? nullptr : &packed_data[current_offset];

    //Update the counters
    current_offset += lens[current_id];
    current_id++;

    return true;
  }

  /**
   * @brief Get the next data pointer from the bundle (using a bundle_offsets)
   * @param bundle_offsets Data structure containing pointers to the next item in this bundle
   * @param data_ptr Pointer to the item (user must not free. user must not use after ldo deleted)
   * @param data_len Returned size of the item
   * @retval TRUE Item was valid and returned
   * @retval FALSE Item not available in bundle. No data returned
   */
  bool GetNext(bundle_offsets_t &bundle_offsets, unsigned char **data_ptr, uint16_t *data_len) {
    return GetNext(bundle_offsets.max_payload_bytes,
                   bundle_offsets.current_id,
                   bundle_offsets.current_byte_offset,
                   data_ptr, data_len);
  }

  /**
   * @brief Get the next data pointer from the bundle (using a bundle_offsets)
   * @param bundle_offsets Data structure containing pointers to the next item in this bundle
   * @param result_string Next item as a string (if available)
   * @retval TRUE Item was valid and returned
   * @retval FALSE Item not available in bundle. No data returned
   */
  bool GetNext(bundle_offsets_t &bundle_offsets, std::string *result_string) {
    uint16_t len;
    unsigned char *ptr;
    bool ok = GetNext(bundle_offsets.max_payload_bytes,
                   bundle_offsets.current_id,
                   bundle_offsets.current_byte_offset,
                   &ptr, &len);
    if(ok) {
      result_string->assign((const char *)ptr, len);
    }
    return ok;
  }

};

} //namespace lunasa

#endif //FAODEL_GENERICDATABUNDLE_HH
