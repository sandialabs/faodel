// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "DataObject.hh"

#include "faodel-common/Common.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/allocators/AllocatorBase.hh"
#include "lunasa/common/Allocation.hh"
#include "lunasa/core/Singleton.hh"

#include <sstream>
#include <fstream>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

#include <atomic>

#include <execinfo.h>
#include <lunasa/common/Allocation.hh>

using namespace std;

namespace lunasa {

static InstanceUninitialized uninitializedError;
static SizeMismatch sizeError;
static InvalidArgument invalidArgumentError;
static UserLDOAccessError userLDOAccessError; 

namespace internal {
extern LunasaCoreBase *lunasa_core;
}

//Use specific things from internal
using Allocation = internal::Allocation;
using AllocationSegment = internal::AllocationSegment;
using AllocatorBase = internal::AllocatorBase;

//Our core is hidden inside of a singleton. Use this macro to make the code easier to understand
#define LCORE (internal::Singleton::impl.core)

/* SET proper alignment (multiples of 4-bytes required for RDMA GETs on Aries IC) */
#define LDO_ALIGNMENT 4

/**
 * @brief Empty constructor. Expects user will copy another ldo handle to this one later
 */
DataObject::DataObject()
        : internal_use_only(this) {
  //cout << "DataObject null CTOR" << endl;
  impl = nullptr;
}

/**
 * @brief Full ctor for allocating a new LDO, providing meta, data, and allocators
 * @param[in] total_user_capacity Maximum amount of space a user can use for meta+data after allocation
 * @param[in] meta_capacity How large user's meta section should be (0 to 64KB-1)
 * @param[in] data_capacity How large data section should be (4GB - 1 - metaCapacity)
 * @param[in] allocator Whether this is allocated in eager (pre-registered) memory or not
 * @param[in] tag Optional data type tag for identifying what kind of LDO this is
 * @throw     invalidArgmentError if capacity problems (total less than meta+data) or unsupported allocator
 */
DataObject::DataObject(uint32_t total_user_capacity,
                       uint16_t meta_capacity, uint32_t data_capacity,
                       DataObject::AllocatorType allocator,
                       dataobject_type_t tag)
        : internal_use_only(this) {

  //Sanity check. Have to convert to uint64 to avoid max overflow
  if(((uint64_t)meta_capacity + (uint64_t)data_capacity > (uint64_t)total_user_capacity)) {
    throw invalidArgumentError;
  }

  /* FORCE capacity to be properly aligned (multiples of 4-bytes required for RDMA GETs on Aries IC) */
  int padding = LDO_ALIGNMENT - ((meta_capacity+data_capacity) & (LDO_ALIGNMENT-1));

  total_user_capacity += padding;

  Allocation *allocation = nullptr;
  if( allocator ==  DataObject::AllocatorType::eager ) {
    allocation = LCORE->AllocateEager(total_user_capacity);

  } else if( allocator ==  DataObject::AllocatorType::lazy ) {
    allocation = LCORE->AllocateLazy(total_user_capacity);

  } else {
    /* THROW exception */
    throw invalidArgumentError;
  }

  allocation->setHeader(1, meta_capacity, data_capacity, padding, tag);
  impl = allocation;

  F_ASSERT((((uint64_t)&allocation->header) & (LDO_ALIGNMENT-1)) == 0, "");
}

/**
 * @brief Minimal ctor that allocates a single chunk of user data from eager memory
 * @param dataCapacity How big this allocation is
 * @note Allocates from EAGER memory
 * @note Users may adjust the meta/data sizes later
 */
DataObject::DataObject(uint32_t dataCapacity)
        : DataObject::DataObject(0, dataCapacity, DataObject::AllocatorType::eager) {
}

/**
 * @brief Minimal ctor that allocates a single chunk of user data + metadata from eager memory
 * @param metaCapacity How much memory to use for metadata
 * @param dataCapacity How much memory to use for user data
 * @note Allocates from EAGER memory
 * @note Users may adjust the meta/data sizes later
 */
DataObject::DataObject(uint16_t metaCapacity, uint32_t dataCapacity)
        : DataObject::DataObject(metaCapacity, dataCapacity, DataObject::AllocatorType::eager) {
}

// === Create an LDO from user-allocated memory ===
//
// The current use-case for "User" LDOs is that a user wants to be able to 
// move data off-node.  As a result, we currently only support "eager"
// allocation of memory for "User" LDOs.
//
// In principle, we could support mixed allocations (e.g., some user data is 
// in user-allocated, some user data is in Lunasa-allocated memory).  However, 
// there is currently no use-case for this. 
//
// We currently assume that the data stored in user memory is all of the user data 
// that is associated with this LDO.
//
// The assumption is that once user memory is used to create an LDO, that memory 
// is managed by Lunasa.  <cleanupFunc> allows Lunasa to properly de-allocate
// the memory.  NOTE: using stack memory to create LDOs is potentially fraught; the
// user will need to be vigilant to make sure that the LDO is destroyed before the
// function returns.
DataObject::DataObject(void *userMemory,
                       uint16_t metaCapacity, uint32_t dataCapacity,
                       void (*userCleanupFunc)(void *))
        : internal_use_only(this) {

  //Only allocate memory for the headers.
  Allocation *allocation = LCORE->AllocateEager(0);
  allocation->setHeader(1, 0, 0, 0, 0);
  impl = allocation;

  // Add our user data segment
  AddUserDataSegment(userMemory, metaCapacity, dataCapacity, userCleanupFunc);
}

DataObject::~DataObject() {
  //cout << "DataObject DTOR" << endl;
  if(impl!=nullptr){
    impl->DecrRef(); //Deallocates when gets to zero
  }  
}

//Shallow copy
DataObject::DataObject(const DataObject &source)
        : internal_use_only(this) {

  //cout << "DataObject copy CTOR" << endl;
  if(source.impl != nullptr)
    source.impl->IncrRef();
  impl = source.impl;
}

//move
DataObject::DataObject(DataObject &&source)
        : internal_use_only(this) {
  //cout << "DataObject move CTOR" << endl;
  impl = source.impl;

  source.impl = nullptr;
}

AllocatorBase * DataObject::Allocator() {
  return (impl!=nullptr) ? impl->local.allocator : nullptr;
}

void DataObject::DropReference(AllocatorBase *allocator) {
  if( (impl != nullptr) && (allocator == impl->local.allocator) ) {
    impl->DropRef();
  }
}

/**
 * @brief Get the number of ldo's that are referencing this data
 * @return Reference count for item
 * @note This function is for Internal Use Only. Users MUST NOT rely on these counts
 */
int DataObject::InternalUseOnly::GetRefCount() const {
  Allocation *impl = ldo->impl;
  return (impl==nullptr) ? 0 : impl->GetRefCount(); 
}

/**
 * @brief Get a pointer to the underlying allocation's data structures
 * @return Pointer to Allocation
 * @note This function is for Internal Use Only. Users should not use it
 */
void *DataObject::InternalUseOnly::GetLocalHeaderPtr() const {
  return (void *)ldo->impl;
}

/**
 * @brief Get a pointer to the start of the on-wire data, which is the header
 * @return Pointer to allocation header
 * @note This function is for Internal Use Only. Users should use functions to manipulate header.
 */
void *DataObject::InternalUseOnly::GetHeaderPtr() const {
  Allocation *impl = ldo->impl;
  return (impl==nullptr) ? nullptr : (void *)&impl->header;
}

DataObject& DataObject::operator=(const DataObject &source) {
  //cout << "DataObject copy assignment" << endl;
  //Get rid of old version, point to new version
  if( impl != nullptr ) {
    impl->DecrRef();
  }

  if(source.impl != nullptr)
    source.impl->IncrRef();
  impl = source.impl;
  return *this;
}

DataObject& DataObject::operator=(DataObject &&source) {
  //cout << "DataObject move assignment" << endl;
  //Get rid of old version, point to new version
  if(impl != nullptr) {
    impl->DecrRef();
  }
  impl = source.impl;
  source.impl = nullptr;
  return *this;
}

DataObject& DataObject::deepcopy(const DataObject &source) {

  // !!!! TODO: not currently supported for user LDOs
  F_ASSERT(source.impl->local.user_data_segments == nullptr, "Deeo copy not supported on user LDOs");

  if( impl != nullptr ) {
    impl->DecrRef();
    impl = nullptr;
  }
  
  /* FORCE capacity to be properly aligned (multiples of 4-bytes required for RDMA GETs on Aries IC) */
  int padding = LDO_ALIGNMENT - ((source.impl->header.meta_bytes+source.impl->header.data_bytes) & (LDO_ALIGNMENT-1));

  //ALLOCATE a mirror of the source's allocation.
  size_t alloc_size = source.impl->header.meta_bytes + source.impl->header.data_bytes + padding;
  Allocation *allocation = source.impl->local.allocator->Allocate(alloc_size);
  allocation->setHeader(1, source.impl->header.meta_bytes, source.impl->header.data_bytes, padding, source.impl->header.type);
  impl = allocation;

  //COPY the source to the mirror
  memcpy(GetMetaPtr(), source.GetMetaPtr(), impl->header.meta_bytes+impl->header.data_bytes);

  return *this;
}

/**
 * @brief Snapshot the user section of LDO out to disk (header+meta+data)
 * @param[in] filename to write data to
 * @return 0
 * @todo This does not yet handle multiple allocation segments yet
 */
uint32_t DataObject::writeToFile(const char *filename) const {

  ofstream f;
  f.open(filename, ios::out | ios::binary);
  const char *p = reinterpret_cast<const char *>(internal_use_only.GetHeaderPtr());
  f.write(p, GetHeaderSize() + GetMetaSize() + GetDataSize());
  f.close();
  return 0;
}

/**
 * @brief Read a snapshotted LDO from disk into this LDO's memory (includes header)
 * @param[in] filename to read from
 * @return 0
 * @todo Currently requires LDO to have already been allocated. Should allocate based on file size
 */
uint32_t DataObject::readFromFile(const char *filename) {

  ifstream f;
  f.open(filename, ios::in | ios::binary);
  char *p = reinterpret_cast<char *>(internal_use_only.GetHeaderPtr());
  struct stat results;

  int rc = stat(filename, &results);
  F_ASSERT(rc == 0,"File stat failed in readFromFile");
  F_ASSERT(results.st_size <= GetHeaderSize() + GetMetaSize() + GetDataSize(), "File size mismatch in readFromFile");

  f.read(p, results.st_size);
  f.close();
  return 0;
}

/**
 * @brief Set the contents of the metadata field to zero.
 * @return void
 */
void DataObject::WipeMeta() {
  // Using functions to maintain consistent definition of how size and pointer are determined
  memset(GetMetaPtr(), 0, GetMetaSize());
}

/**
 * @brief Set the contents of the user data field to zero.
 * @return void
 */
void DataObject::WipeData() {
  // Using functions to maintain consistent definition of how size and pointer are determined
  memset(GetDataPtr(), 0, GetDataSize());
}

/**
 * @brief Set the contents of the user data and metadata fields to zero.
 * @return void
 */
void DataObject::WipeUser() { //Meta + Data
  // Using functions to maintain consistent definition of how size and pointer are determined
  memset(GetMetaPtr(), 0, GetUserSize());
}

/**
 * @brief Get the meta_tag (an id for this particular LDO type) from the header
 * @return meta_tag A hash id value that can be used to verify an LDO is an expected LDO
 */
dataobject_type_t DataObject::GetTypeID() const {
  if( impl==nullptr) return 0;
  return impl->getType();
}

/**
 * @brief Set the header's meta_tag (an id for this particular LDO type)
 * @param[in] type_id The id to store in this LDO's header
 * @note Value is NOT set if this ldo has not been allocated yet
 */
void DataObject::SetTypeID(dataobject_type_t type_id) {
  if( impl==nullptr) return;
  impl->setType(type_id);
}

/**
 * @brief Get the size of the local bookkeeping required for this LDO (everything up to header)
 * @return Size of the local bookkeeping
 */
uint32_t DataObject::GetLocalHeaderSize()  const {
  return offsetof(Allocation, header); //Get everything up to where header starts
}

/**
 * @brief Get the size of the header that travels with the LDO (meta_tag, meta_size, data_size)
 * @return Size of the header
 */
uint32_t DataObject::GetHeaderSize() {
  return offsetof(Allocation, user[0]) -
         offsetof(Allocation, header);
}

/**
 * @brief Get the size of the user-defined meta data included in the user section
 * @return Size of the meta data (0 to 64KB-1)
 */
uint32_t DataObject::GetMetaSize() const {
  if( impl == nullptr ) return 0;
  return impl->header.meta_bytes;
}

/**
 * @brief Get the size of the data portion of the user section
 * @return Size of user data (0 to 4GB-1-meta_size)
 */
uint32_t DataObject::GetDataSize() const {
  if( impl == nullptr ) return 0;
  return impl->header.data_bytes; 
}

uint32_t DataObject::GetPaddingSize() const {
  if( impl == nullptr ) return 0;
  return impl->local.padding; 
}

/**
 * @brief Get the size of the user section (meta_size + data_size)
 * @return Size of meta and data sections
 */
uint32_t DataObject::GetUserSize() const {
  if( impl == nullptr ) return 0;
  return impl->header.meta_bytes + impl->header.data_bytes;
}

/**
 * @brief Get the amount of space this LDO takes when put on the wire (header + meta + data)
 * @return Size of header + meta + data sections
 */
uint32_t DataObject::GetWireSize() const {
  if( impl == nullptr ) return 0;
  return GetHeaderSize() + impl->header.meta_bytes + impl->header.data_bytes;
}

/**
 * @brief Get the raw amount of space Lunasa allocated for this (local, header, and data)
 * @return Size of the entire allocation
 * @note It is UNCOMMON to use this function. This may be deprecated in the future
 */
uint32_t DataObject::GetRawAllocationSize() const {
  if( impl == nullptr ) return 0;
  return impl->local.allocated_bytes;
}

/**
 * Get the heap address for the bookkeeping
 * @deprecated This should be marked iuo or removed
 */
void *DataObject::GetBasePtr() const {
  return (void *)impl; 
}



/**
 * @brief Get a pointer to the meta data part of the user section
 * @return Metadata pointer
 * @note This will be the same as the Data pointer of meta size is zero
 */
void *DataObject::GetMetaPtr() const {

  if( impl == nullptr ) {
    return nullptr;

  } else if( impl->local.user_data_segments != nullptr ) {
    /* === Allocation only contains headers === */
    F_ASSERT(impl->local.user_data_segments->empty() == false, "");
    F_ASSERT(impl->local.allocated_bytes == sizeof(Allocation), "");

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &segment = impl->local.user_data_segments->front();

    /* Segment must be big enough to store the entire User Metadata segment. */
    F_ASSERT(segment.size > impl->header.meta_bytes, "");

    return segment.buffer_ptr;

  } else if( impl->local.allocated_bytes >= sizeof(Allocation) ) {
    /* === Allocation contains User Metadata === */

    return (void *)&impl->user[0];
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    std::cerr<<"Lunasa internal allocation: "<<impl->local.allocated_bytes<<" vs "<<sizeof(Allocation)<<endl;
    F_ASSERT(0,"Lunasa Data Object internal allocation smaller than a valid header");
  }

  return nullptr; 
}

/**
 * @brief Get a pointer to the data part of the user section
 * @return Data pointer
 * @note This will be the same as the Meta pointer if meta_size is zero
 */
void *DataObject::GetDataPtr() const {
  /* USER DATA segment begins at the first byte after the USER META segment. */

  if( impl == nullptr ) {
    return nullptr;

  } else if( impl->local.user_data_segments != nullptr ) {
    /* === Allocation only contains headers === */
    F_ASSERT(impl->local.user_data_segments->empty() == false, "");
    F_ASSERT(impl->local.allocated_bytes == sizeof(Allocation), "");

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &segment = impl->local.user_data_segments->front();
    /* Segment must be big enough to store the entire User Metadata and User Data segments. */
    F_ASSERT(segment.size > (impl->header.meta_bytes+impl->header.data_bytes), "");

    return (void *)((uint8_t *)segment.buffer_ptr + impl->header.meta_bytes);
  } else if( impl->local.allocated_bytes >= sizeof(Allocation) ) {
    /* === Allocation contains User Data === */
    return (void *)(impl->user + impl->header.meta_bytes);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    F_ASSERT(0, "Allocation smaller than header?");
  }

  return nullptr; 
}

/* Get RDMA handles */
int DataObject::GetBaseRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const {

  if( impl == nullptr ) return 0;

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    F_ASSERT(impl->local.allocator->UsingLazyRegistration() == true, "");
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  rdma_segment_desc header_segment(impl->local.net_buffer_handle, 
                                   impl->local.net_buffer_offset, 
                                   impl->local.allocated_bytes);
  rdma_segments.push(header_segment);

  /* ADD user segments, if any, to the queue */
  if( impl->local.user_data_segments != nullptr && impl->local.user_data_segments->empty() == false ) {
    /* Current assumption is that there's just one user data segment. */
    F_ASSERT(impl->local.user_data_segments->size() == 1, "");

    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();
    rdma_segment_desc segment(allocation_segment.net_buffer_handle, 0, allocation_segment.size);

    rdma_segments.push(segment);
  }

  return 0;
}

int DataObject::GetBaseRdmaHandle(void **rdma_handle, uint32_t &offset) const {

  std::queue<rdma_segment_desc> segments;
  GetBaseRdmaHandles(segments);
  F_ASSERT(segments.size() == 1, "");
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetLocalHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const {
  /* From an API-design perspective, it's important to distinguish between
     the BASE of the allocation and the LOCAL HEADER because they may not
     be forever and always the same.  *But*, at the moment, they are, so there's
     no reason not to reuse what we've already done. */
  return GetBaseRdmaHandles(rdma_segments);
}

int DataObject::GetLocalHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const {
  return GetBaseRdmaHandle(rdma_handle, offset);
}

int DataObject::GetHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const {

  F_ASSERT(rdma_segments.size() == 0, "");
  if( impl == nullptr ) return 0;

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    F_ASSERT(impl->local.allocator->UsingLazyRegistration() == true, "");
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  uint32_t offset = impl->local.net_buffer_offset + offsetof(Allocation, header);
  uint32_t size = impl->local.allocated_bytes - offsetof(Allocation, header);
  rdma_segment_desc header_segment(impl->local.net_buffer_handle, offset, size);
  rdma_segments.push(header_segment);
  rdma_segment_desc &segment = rdma_segments.front();

  /* ADD user segments, if any, to the queue */
  if( impl->local.user_data_segments != nullptr && impl->local.user_data_segments->empty() == false ) {
    /* Current assumption is that there's just one user data segment. */
    F_ASSERT(impl->local.user_data_segments->size() == 1, "");

    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();
    rdma_segment_desc segment(allocation_segment.net_buffer_handle, 0, allocation_segment.size);

    rdma_segments.push(segment);
  }

  return 0;
}

int DataObject::GetHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const {
  std::queue<rdma_segment_desc> segments;
  F_ASSERT(segments.size() == 0, "");
  GetHeaderRdmaHandles(segments);
  F_ASSERT(segments.size() == 1, "");
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetMetaRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const {
  if( impl == nullptr ) {
    return 0;
  }

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    F_ASSERT(impl->local.allocator->UsingLazyRegistration() == true, "");
    impl->local.allocator->RegisterMemory(impl,
                                          impl->local.allocated_bytes,
                                          impl->local.net_buffer_handle);
  }

  if( impl->local.allocated_bytes == sizeof(Allocation) ) {
    /* === Allocation only contains headers === */
    rdma_segment_desc header_segment(impl->local.net_buffer_handle, 
                                     impl->local.net_buffer_offset, 
                                     impl->local.allocated_bytes);
    rdma_segments.push(header_segment);

    /* First data segment must contain the User Metadata section. */
    F_ASSERT(impl->local.user_data_segments->size() == 1, "");
    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();

    /* Segment must be big enough to store the entire User Metadata segment. */
    rdma_segment_desc user_segment(allocation_segment.net_buffer_handle, 
                                   allocation_segment.net_buffer_offset, 
                                   allocation_segment.size); 

    rdma_segments.push(user_segment);
    
  } else if( impl->local.allocated_bytes > sizeof(Allocation) ) {
    /* === Allocation contains User METADATA === */

    /* Allocation must be big enough to store the entire User METADATA segment. */
    uint32_t meta_offset = offsetof(Allocation, user);
    F_ASSERT(impl->local.allocated_bytes >= meta_offset + impl->header.meta_bytes, "");

    uint32_t total_offset = impl->local.net_buffer_offset + offsetof(Allocation, user);
    uint32_t size = impl->local.allocated_bytes - offsetof(Allocation, user);
    rdma_segment_desc rdma_segment(impl->local.net_buffer_handle, total_offset, size);
    rdma_segments.push(rdma_segment);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    F_ASSERT(0, "Allocation is smaller than header");
  }
  
  return 0;
}

int DataObject::GetMetaRdmaHandle(void **rdma_handle, uint32_t &offset) const {

  std::queue<rdma_segment_desc> segments;
  GetMetaRdmaHandles(segments);
  F_ASSERT(segments.size() == 1, "");
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetDataRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const {

  if( impl == nullptr ) return 0;

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    F_ASSERT(impl->local.allocator->UsingLazyRegistration() == true, "");
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  if( impl->local.allocated_bytes == sizeof(Allocation) ) {
    /* === Allocation only contains headers === */
    F_ASSERT(impl->local.user_data_segments->empty() == false, "");

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();

    /* Segment must be equal to the combined size of the User METADATA and DATA sections. */
    F_ASSERT(allocation_segment.size == impl->header.meta_bytes + impl->header.data_bytes, "");

    uint32_t offset = impl->local.net_buffer_offset + 
                      offsetof(Allocation, user) +
                      impl->header.meta_bytes;

    uint32_t size = impl->header.data_bytes;

    rdma_segment_desc header_segment(impl->local.net_buffer_handle, offset, size);
    rdma_segments.push(header_segment);
  } else if( impl->local.allocated_bytes > sizeof(Allocation) ) {
    /* === Allocation contains User METADATA === */

    /* Segment must be equal to the combined size of the User METADATA and DATA sections. */
    if(impl->local.allocated_bytes != offsetof(Allocation, user) +
                                          impl->header.meta_bytes + 
                                          impl->header.data_bytes +
                                          impl->local.padding)
    {
      fprintf(stderr, "impl->local.allocated_bytes (%d) != offsetof(Allocation, user) (%ld) +\n"
                      "                              impl->header.meta_bytes (%d) + \n"
                      "                              impl->header.data_bytes  (%d)+ \n"
                      "                              impl->local.padding (%d)",
             impl->local.allocated_bytes, offsetof(Allocation, user), impl->header.meta_bytes,
             impl->header.data_bytes, impl->local.padding);
      fflush(stderr);
    }
    F_ASSERT(impl->local.allocated_bytes == offsetof(Allocation, user) +
                                            impl->header.meta_bytes +
                                            impl->header.data_bytes +
                                            impl->local.padding, "");

    uint32_t offset = impl->local.net_buffer_offset + 
                      offsetof(Allocation, user) +
                      impl->header.meta_bytes;

    uint32_t size = impl->header.data_bytes;

    rdma_segment_desc rdma_segment(impl->local.net_buffer_handle, offset, size);
    rdma_segments.push(rdma_segment);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    F_ASSERT(0, "");
  }
  
  return 0;
}

int DataObject::GetDataRdmaHandle(void **rdma_handle, uint32_t &offset) const {
  std::queue<rdma_segment_desc> segments;
  GetDataRdmaHandles(segments);
  F_ASSERT(segments.size() == 1, "");
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

/**
 * @brief Do a detailed comparison of two LDO and determine if they're equal
 * @param[in] other Another data object to compare against
 * @retval 0 The two objects are equal (either by reference or deep comparison)
 * @retval -1 One of the objects is empty while the other is not
 * @retval -2 The Object type fields do not match
 * @retval -3 The Meta sizes are different
 * @retval -4 The Data sizes are different
 * @retval -5 The Meta sections are not identical
 * @retval -6 The Data sections are not identical
 */
int DataObject::DeepCompare(const DataObject &other) const {

  if(*this == other) return 0; //References to same thing

  if(isNull() || other.isNull()) return -1;
  if(GetTypeID() != other.GetTypeID()) return -2;
  if(GetMetaSize() != other.GetMetaSize()) return -3;
  if(GetDataSize() != other.GetDataSize()) return -4;

  int rc;
  //Byte compare the meta section
  rc = memcmp(GetMetaPtr(), other.GetMetaPtr(), GetMetaSize());
  if(rc!=0) return -5;

  //Byte compare the data section
  rc = memcmp(GetDataPtr(), other.GetDataPtr(), GetDataSize());
  if(rc!=0) return -6;

  return 0;
}


bool DataObject::isPinned() const {
  if( impl == nullptr ) {
    return false;
  }
  return (impl->local.net_buffer_handle != nullptr);
}

void DataObject::AddUserDataSegment(
        void *userMemory,
        uint32_t metaCapacity, uint32_t dataCapacity,
        void (*userCleanupFunc)(void *)) {

  void *pinnedMemory;
  impl->local.allocator->RegisterMemory(userMemory, metaCapacity+dataCapacity, pinnedMemory);

  /* Because we have just registered the entirety of the user's memory, the offset is 0 */
  AllocationSegment segment(userMemory, pinnedMemory, metaCapacity, dataCapacity, userCleanupFunc);

  if( impl->local.user_data_segments == nullptr ) {
    impl->local.user_data_segments = new std::vector<AllocationSegment>();
  }
  impl->local.user_data_segments->push_back(segment);
  impl->header.meta_bytes += metaCapacity;
  impl->header.data_bytes += dataCapacity;
}

void DataObject::sstr(stringstream &ss, int depth, int indent) const {
  F_TODO("Data Object sstr missing");
#if 0
  // !!!! TODO : needs to be updated to account for API changes
  if(depth<0) return;
  if(impl!=nullptr){
    ss << string(indent,' ') << "[LDO]"
       << " RawSize: "  << impl->getRawSize()
       << " MetaSize: " << impl->getMetaSize()
       << " DataSize: " << impl->getDataSize()
       << " RawPtr: "  << impl->getRawPtr()
       << " MetaPtr: " << impl->getMetaPtr()
       << " DataPtr: " << impl->getDataPtr()
       << " RawOffset: "  << impl->getRawOffset()
       << " MetaOffset: " << impl->getMetaOffset()
       << " DataOffset: " << impl->getDataOffset()
       << " RefCount: " << impl->GetRefCount()
       << endl;
  } else {
    ss << string(indent,' ') << "[LDO] nullptr\n";
  }
#endif
}

/**
 * @brief Adjust the meta/data boundaries in the header of this message
 * @param[in] new_meta_size How big the meta portion of user section should be
 * @param[in] new_data_size How big the data portion of user section should be
 * @retval 0 success
 * @retval -1 Values were NOT adjusted due to a lack of capacity
 * @note This does NOT move data. It just updates the header. USE AT YOUR OWN RISK
 */
int DataObject::ModifyUserSizes(uint16_t new_meta_size, uint32_t new_data_size) {

  if(impl==nullptr) return -1;

  uint64_t capacity = ((uint64_t)impl->GetUserCapacity());
  uint64_t new_size = ((uint64_t)new_meta_size) + ((uint64_t)new_data_size);

  if(new_size > capacity) return -1;

  impl->header.meta_bytes = new_meta_size;
  impl->header.data_bytes = new_data_size;

  return 0;
}

uint32_t DataObject::GetUserCapacity() const {
  if(impl==nullptr) return 0;
  return impl->GetUserCapacity();
}


} // namespace lunasa
