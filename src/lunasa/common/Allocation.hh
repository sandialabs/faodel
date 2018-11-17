// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATION_HH
#define LUNASA_ALLOCATION_HH 

#include <atomic>
#include <assert.h>

#include "faodel-common/Common.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/allocators/AllocatorBase.hh"

/* !!!! TODO !!!!
 * Enumerate assumptions.
 *    Segments align with allocations (e.g., meta can't straddle two allocations)
 *
 *    Currently, we assume that if a user data segment exists, then it contains
 *    the User Meta and User Data segments.  This assumption makes the sanity 
 *    checking more straightforward, but it wouldn't be hard to change.
 *    
 *    Because the user data segment is explicitly registered, the current 
 *    assumption is that no offset is necessary (i.e., the base address of the
 *    user's memory is registered and the only reference that we need to retain is 
 *    the <handle>).
 */

namespace lunasa {
namespace internal {

struct AllocationSegment {

  void *buffer_ptr;             //!< Pointer to original memory
  void *net_buffer_handle;      //!< Handle to pinned memory

  uint32_t net_buffer_offset;   //!< Offset into pinned memory
  uint32_t size;                //!< Number of bytes

  void (*cleanup_func)(void *); //!< Function that releases the memory referenced by buffer_ptr

  AllocationSegment(void *buffer_ptr_, void *net_buffer_handle_, uint32_t net_buffer_offset_,
                    uint32_t size_, void (*cleanup_func_)(void *)):
    buffer_ptr(buffer_ptr_), 
    net_buffer_handle(net_buffer_handle_), 
    net_buffer_offset(net_buffer_offset_), 
    size(size_),
    cleanup_func(cleanup_func_)
  {}
  
};

//All of the local things that don't get put in a raw message
typedef struct {
  void *net_buffer_handle;     //!< Nonzero when this item is pinned
  std::atomic_int ref_count;   //!< Number of user LDOs that use this
  AllocatorBase *allocator;    //!< Reference to the allocator that provided the memory
  uint32_t net_buffer_offset;  //!< Starting offset into buffer? May be nonzero when doing a suballocation
  uint32_t allocated_bytes;    //!< Number of bytes that were allocated (includes local, header, and user sizes)

  // User-allocated memory segments that have been made part of the LDO.
  // NOTE: a std::vector is used here to support potential cases in the future
  // where multiple user data segments are supported.
  std::vector<AllocationSegment> *user_data_segments; 
} allocation_local_t;

//A short header in the front of the on-wire data. Has a type and breaks down message lengths
typedef struct {
  dataobject_type_t type;      //!< A hash ID used to specify a data type of an LDO
  uint16_t meta_bytes;         //!< Total bytes of meta data segment is (0B to (16KB-1))
  uint32_t data_bytes;         //!< Total bytes of user data segment (0B to (4GB-1-meta_bytes)
} allocation_header_t;

//One allocation to hold everything about an ldo
// local:  refcounts and pointers only available here
// wire:   data that gets shipped over the wire (header+user)
//   header: A short struct to hold the type and meta/data lengths
//   user:  The user's actual data. Up to 64KB-1 of meta followed by data
//   
struct Allocation {
  allocation_local_t  local;   //!< Pointers and bookkeeping only available on local node
  allocation_header_t header;  //!< Start of raw data, includes lengths
  uint8_t             user[0]; //!< Start of user's meta/data

  void setHeader(int initial_ref_count,
                 uint16_t meta_size, uint32_t data_size,
                 dataobject_type_t type) {
    local.ref_count = ATOMIC_VAR_INIT(initial_ref_count);
    header.type       = type;
    header.data_bytes = data_size;
    header.meta_bytes = meta_size;
  }

  dataobject_type_t getType() const { return header.type; }
  void              setType(dataobject_type_t type) { header.type = type; }

  /**
   * @brief Report how much user space is available in the allocation
   * @return Total allocations size minus local and header sections
   */
  uint32_t GetUserCapacity() {
    return local.allocated_bytes - offsetof(Allocation, user[0]);
  }

  // !!!! TODO not sure whether these get used !!!!
  bool IsPinned()    { return local.net_buffer_handle != nullptr; }
  int  GetRefCount() { return local.ref_count.load(); }
  void IncrRef()     { ++local.ref_count; }
  void DropRef()     { --local.ref_count; } //For internal patching
  int  DecrRef() {    //Issues a dealloc of this when reaches zero
    assert(local.ref_count > 0 && "LunasaDataObject refcount decremented to below zero");

    --local.ref_count;
    int num_left = local.ref_count.load();
    if(num_left == 0) {
      if( local.user_data_segments != nullptr ) {

        for( auto it = local.user_data_segments->begin();
                  it != local.user_data_segments->end(); ++it ) {
          AllocationSegment segment = *it;

          if( segment.cleanup_func != nullptr ) {
            (*segment.cleanup_func)(segment.buffer_ptr);
            segment.buffer_ptr = NULL;
          }
        }
      }
      local.allocator->Free(this);
    }
    return num_left;
  }

};

} // namespace internal
} // namespace lunasa

#endif // ALLOCATION_HH
