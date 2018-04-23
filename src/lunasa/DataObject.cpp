// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "DataObject.hh"

#include "common/Common.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/allocators/AllocatorBase.hh"
#include "lunasa/common/Allocation.hh"
#include "lunasa/core/Singleton.hh"

#include <sstream>
#include <fstream>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include <atomic>

#include <execinfo.h>

using namespace std;

namespace lunasa {

static InstanceUninitialized uninitializedError;
static SizeMismatch sizeError;
static InvalidArgument invalidArgumentError;
static UserLDOAccessError userLDOAccessError; 

namespace internal {

extern LunasaCoreBase *lunasa_core;

}

DataObject::DataObject() : internal_use_only(this)
{
  //cout << "DataObject null CTOR" << endl;
  impl = nullptr;
}

// Allocate memory for a new LDO
DataObject::DataObject(uint16_t metaCapacity, uint32_t dataCapacity, DataObject::AllocatorType allocator) :
  internal_use_only(this)
{
  allocation_t *allocation = nullptr;
  if( allocator ==  DataObject::AllocatorType::eager ) {
    allocation = internal::Singleton::impl.core->AllocateEager(metaCapacity, dataCapacity);
  } else if( allocator ==  DataObject::AllocatorType::lazy ) {
    allocation = internal::Singleton::impl.core->AllocateLazy(metaCapacity, dataCapacity);
  } else {
    /* THROW exception */
    throw invalidArgumentError;
  }

  allocation->setHeader(1, metaCapacity, dataCapacity);
  impl = allocation;
}

// A shortcut for the most common use of this constructor
DataObject::DataObject(uint32_t dataCapacity) : 
  DataObject::DataObject(0, dataCapacity, DataObject::AllocatorType::eager)
{}

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
DataObject::DataObject(void *userMemory, uint16_t metaCapacity, uint32_t dataCapacity, 
                       void (*userCleanupFunc)(void *)) : internal_use_only(this)
{
  /* only ALLOCATE memory for the headers. */
  allocation_t *allocation = internal::Singleton::impl.core->AllocateEager(0, 0);
  allocation->setHeader(1, 0, 0);
  impl = allocation;

  /* ADD our user data segment */
  AddUserDataSegment(userMemory, metaCapacity, dataCapacity, userCleanupFunc);
}

DataObject::~DataObject()
{
  //cout << "DataObject DTOR" << endl;
  if(impl!=nullptr){
    impl->DecrRef(); //Deallocates when gets to zero
  }  
}

//Shallow copy
DataObject::DataObject(const DataObject &source) : internal_use_only(this)
{
  //cout << "DataObject copy CTOR" << endl;
  if(source.impl != nullptr)
    source.impl->IncrRef();
  impl = source.impl;
}
//move
DataObject::DataObject(DataObject &&source) : internal_use_only(this)
{
  //cout << "DataObject move CTOR" << endl;
  impl = source.impl;

  source.impl = nullptr;
}

lunasa::AllocatorBase* DataObject::Allocator()
{
  return (impl!=nullptr) ? impl->local.allocator : nullptr;
}

void DataObject::DropReference(lunasa::AllocatorBase* allocator)
{
  if( (impl != nullptr) && (allocator == impl->local.allocator) ) {
    impl->DropRef();
  }
}

int DataObject::InternalUseOnly::GetRefCount() const
{
  allocation_t *impl = ldo->impl;
  return (impl==nullptr) ? 0 : impl->GetRefCount(); 
}

DataObject& DataObject::operator=(const DataObject& source) 
{
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

DataObject& DataObject::operator=(DataObject&& source) {
  //cout << "DataObject move assignment" << endl;
  //Get rid of old version, point to new version
  if(impl != nullptr) {
    impl->DecrRef();
  }
  impl = source.impl;
  source.impl = nullptr;
  return *this;
}

DataObject& DataObject::deepcopy(const DataObject &source)
{
  // !!!! TODO: not currently supported for user LDOs
  assert(source.impl->local.user_data_segments == nullptr);

  if( impl != nullptr ) {
    impl->DecrRef();
    impl = nullptr;
  }
  
  /* ALLOCATE a mirror of the source's allocation. */
  allocation_t *allocation = source.impl->local.allocator->Allocate(source.impl->header.meta_bytes +
                                                                    source.impl->header.data_bytes);
  allocation->setHeader(1, source.impl->header.meta_bytes, source.impl->header.data_bytes);
  impl = allocation;

  /* COPY the source to the mirror */
  memcpy(GetMetaPtr(), source.GetMetaPtr(), impl->header.meta_bytes+impl->header.data_bytes);

  return *this;
}

// !!!! TODO handle user segments !!!!
uint32_t DataObject::writeToFile(const char *filename) const
{
  ofstream f;
  f.open(filename, ios::out | ios::binary);
  const char *p = reinterpret_cast<const char *>(GetHeaderPtr());
  f.write(p, GetHeaderSize() + GetMetaSize() + GetDataSize());
  f.close();
  return 0;
}

uint32_t DataObject::readFromFile(const char *filename) 
{
  ifstream f;
  f.open(filename, ios::in | ios::binary);
  char *p = reinterpret_cast<char *>(GetHeaderPtr());
  struct stat results;

  assert(stat(filename, &results) == 0);
  assert(results.st_size <= GetHeaderSize() + GetMetaSize() + GetDataSize());

  f.read(p, results.st_size);
  f.close();
  return 0;
}

/* GET size of LDO segments */
uint32_t DataObject::GetLocalHeaderSize()  const 
{ 
  return offsetof(struct allocation_s, header); 
}

uint32_t DataObject::GetHeaderSize() const 
{ 
  return offsetof(struct allocation_s, meta_and_user_data[0]) -
         offsetof(struct allocation_s, header); 
}

uint32_t DataObject::GetMetaSize() const 
{ 
  if( impl == nullptr ) {
    return 0;
  }

  return impl->header.meta_bytes; 
}

uint32_t DataObject::GetDataSize() const 
{ 
  if( impl == nullptr ) {
    return 0;
  }

  return impl->header.data_bytes; 
}

uint32_t DataObject::GetUserSize() const 
{ 
  if( impl == nullptr ) {
    return 0;
  }

  return impl->header.meta_bytes + impl->header.data_bytes; 
}

uint32_t DataObject::GetTotalSize() const 
{
  if( impl == nullptr ) {
    return 0;
  }

  return impl->local.allocated_bytes;
}

/* Get HEAP addresses */
void *DataObject::GetBasePtr() const
{ 
  return (void *)impl; 
}

void *DataObject::GetLocalHeaderPtr() const
{ 
  return (void *)impl; 
}

void *DataObject::GetHeaderPtr() const
{ 
  return (void *)&impl->header; 
}

void *DataObject::GetMetaPtr() const
{ 
  if( impl == nullptr ) {
    return nullptr;
  } else if( impl->local.user_data_segments != nullptr ) {
    /* === Allocation only contains headers === */
    assert(impl->local.user_data_segments->empty() == false); 
    assert(impl->local.allocated_bytes == sizeof(allocation_t));

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &segment = impl->local.user_data_segments->front(); 

    /* Segment must be big enough to store the entire User Metadata segment. */
    assert(segment.size > impl->header.meta_bytes); 

    return segment.buffer_ptr;
  } else if( impl->local.allocated_bytes >= sizeof(allocation_t) ) {
    /* === Allocation contains User Metadata === */

    return (void *)&impl->meta_and_user_data[0];
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    assert(0);
  }

  return nullptr; 
}

void *DataObject::GetDataPtr() const
{ 
  /* USER DATA segment begins at the first byte after the USER META segment. */

  if( impl == nullptr ) {
    return nullptr;
  } else if( impl->local.user_data_segments != nullptr ) {
    /* === Allocation only contains headers === */
    assert(impl->local.user_data_segments->empty() == false); 
    assert(impl->local.allocated_bytes == sizeof(allocation_t));

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &segment = impl->local.user_data_segments->front(); 
    /* Segment must be big enough to store the entire User Metadata and User Data segments. */
    assert(segment.size > (impl->header.meta_bytes+impl->header.data_bytes));

    return (void *)((uint8_t *)segment.buffer_ptr + impl->header.meta_bytes);
  } else if( impl->local.allocated_bytes >= sizeof(allocation_t) ) {
    /* === Allocation contains User Data === */
    return (void *)(impl->meta_and_user_data + impl->header.meta_bytes);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    assert(0);
  }

  return nullptr; 
}

/* Get RDMA handles */
int DataObject::GetBaseRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const
{
  if( impl == nullptr ) {
    return 0;
  }

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    assert(impl->local.allocator->UsingLazyRegistration() == true);
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  rdma_segment_desc header_segment(impl->local.net_buffer_handle, 
                                   impl->local.net_buffer_offset, 
                                   impl->local.allocated_bytes);
  rdma_segments.push(header_segment);

  /* ADD user segments, if any, to the queue */
  if( impl->local.user_data_segments != nullptr && impl->local.user_data_segments->empty() == false ) {
    /* Current assumption is that there's just one user data segment. */
    assert(impl->local.user_data_segments->size() == 1);

    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();
    rdma_segment_desc segment(allocation_segment.net_buffer_handle, 0, allocation_segment.size);

    rdma_segments.push(segment);
  }

  return 0;
}

int DataObject::GetBaseRdmaHandle(void **rdma_handle, uint32_t &offset) const
{
  std::queue<rdma_segment_desc> segments;
  GetBaseRdmaHandles(segments);
  assert(segments.size() == 1);
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetLocalHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const
{
  /* From an API-design perspective, it's important to distinguish between
     the BASE of the allocation and the LOCAL HEADER because they may not
     be forever and always the same.  *But*, at the moment, they are, so there's
     no reason not to reuse what we've already done. */
  return GetBaseRdmaHandles(rdma_segments);
}

int DataObject::GetLocalHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const
{
  return GetBaseRdmaHandle(rdma_handle, offset);
}

int DataObject::GetHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const
{
  assert(rdma_segments.size() == 0);
  if( impl == nullptr ) {
    return 0;
  }

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    assert(impl->local.allocator->UsingLazyRegistration() == true);
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  uint32_t offset = impl->local.net_buffer_offset + offsetof(struct allocation_s, header); 
  uint32_t size = impl->local.allocated_bytes - offsetof(struct allocation_s, header); 
  rdma_segment_desc header_segment(impl->local.net_buffer_handle, offset, size);
  rdma_segments.push(header_segment);
  rdma_segment_desc &segment = rdma_segments.front();

  /* ADD user segments, if any, to the queue */
  if( impl->local.user_data_segments != nullptr && impl->local.user_data_segments->empty() == false ) {
    /* Current assumption is that there's just one user data segment. */
    assert(impl->local.user_data_segments->size() == 1);

    AllocationSegment &allocation_segment = impl->local.user_data_segments->front();
    rdma_segment_desc segment(allocation_segment.net_buffer_handle, 0, allocation_segment.size);

    rdma_segments.push(segment);
  }

  return 0;
}

int DataObject::GetHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const
{
  std::queue<rdma_segment_desc> segments;
  assert(segments.size() == 0);
  GetHeaderRdmaHandles(segments);
  assert(segments.size() == 1);
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetMetaRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const
{
  if( impl == nullptr ) {
    return 0;
  }

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    assert(impl->local.allocator->UsingLazyRegistration() == true);
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  if( impl->local.allocated_bytes == sizeof(allocation_t) ) {
    /* === Allocation only contains headers === */
    rdma_segment_desc header_segment(impl->local.net_buffer_handle, 
                                     impl->local.net_buffer_offset, 
                                     impl->local.allocated_bytes);
    rdma_segments.push(header_segment);

    /* First data segment must contain the User Metadata section. */
    assert(impl->local.user_data_segments->size() == 1); 
    AllocationSegment &allocation_segment = impl->local.user_data_segments->front(); 

    /* Segment must be big enough to store the entire User Metadata segment. */
    rdma_segment_desc user_segment(allocation_segment.net_buffer_handle, 
                                   allocation_segment.net_buffer_offset, 
                                   allocation_segment.size); 

    rdma_segments.push(user_segment);
    
  } else if( impl->local.allocated_bytes > sizeof(allocation_t) ) {
    /* === Allocation contains User METADATA === */

    /* Allocation must be big enough to store the entire User METADATA segment. */
    uint32_t meta_offset = offsetof(struct allocation_s, meta_and_user_data); 
    assert(impl->local.allocated_bytes >= meta_offset + impl->header.meta_bytes);

    uint32_t total_offset = impl->local.net_buffer_offset + offsetof(struct allocation_s, meta_and_user_data); 
    uint32_t size = impl->local.allocated_bytes - offsetof(struct allocation_s, meta_and_user_data); 
    rdma_segment_desc rdma_segment(impl->local.net_buffer_handle, total_offset, size);
    rdma_segments.push(rdma_segment);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    assert(0);
  }
  
  return 0;
}

int DataObject::GetMetaRdmaHandle(void **rdma_handle, uint32_t &offset) const
{
  std::queue<rdma_segment_desc> segments;
  GetMetaRdmaHandles(segments);
  assert(segments.size() == 1);
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

int DataObject::GetDataRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const
{
  if( impl == nullptr ) {
    return 0;
  }

  /* If memory is EAGERLY registered, then this handle will never be NULL.  As a result,
     if this handle is NULL, we can assume that it's LAZILY registered. */
  if( impl->local.net_buffer_handle == nullptr ) {
    assert(impl->local.allocator->UsingLazyRegistration() == true);
    impl->local.allocator->RegisterMemory(impl, impl->local.allocated_bytes, impl->local.net_buffer_handle);
  }

  if( impl->local.allocated_bytes == sizeof(allocation_t) ) {
    /* === Allocation only contains headers === */
    assert(impl->local.user_data_segments->empty() == false); 

    /* First data segment must contain the User Metadata segment. */
    AllocationSegment &allocation_segment = impl->local.user_data_segments->front(); 

    /* Segment must be equal to the combined size of the User METADATA and DATA sections. */
    assert(allocation_segment.size == impl->header.meta_bytes + impl->header.data_bytes); 

    uint32_t offset = impl->local.net_buffer_offset + 
                      offsetof(struct allocation_s, meta_and_user_data) +
                      impl->header.meta_bytes;

    uint32_t size = impl->header.data_bytes;

    rdma_segment_desc header_segment(impl->local.net_buffer_handle, offset, size);
    rdma_segments.push(header_segment);
  } else if( impl->local.allocated_bytes > sizeof(allocation_t) ) {
    /* === Allocation contains User METADATA === */

    /* Segment must be equal to the combined size of the User METADATA and DATA sections. */
    assert(impl->local.allocated_bytes == offsetof(struct allocation_s, meta_and_user_data) +
                                          impl->header.meta_bytes + 
                                          impl->header.data_bytes);

    uint32_t offset = impl->local.net_buffer_offset + 
                      offsetof(struct allocation_s, meta_and_user_data) +
                      impl->header.meta_bytes;

    uint32_t size = impl->header.data_bytes;

    rdma_segment_desc rdma_segment(impl->local.net_buffer_handle, offset, size);
    rdma_segments.push(rdma_segment);
  } else {
    /* This can only mean that the allocation is smaller than the header.  Not good. */
    assert(0);
  }
  
  return 0;
}

int DataObject::GetDataRdmaHandle(void **rdma_handle, uint32_t &offset) const
{
  std::queue<rdma_segment_desc> segments;
  GetDataRdmaHandles(segments);
  assert(segments.size() == 1);
  rdma_segment_desc &segment = segments.front();
  *rdma_handle = segment.net_buffer_handle;
  offset = segment.net_buffer_offset;
  return 0;
}

bool DataObject::isPinned() const
{ 
  if( impl == nullptr ) {
    return false;
  }
  return (impl->local.net_buffer_handle != nullptr);
}

void DataObject::sstr(stringstream &ss, int depth, int indent) const
{
  assert(0 &&"TODO");
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

void DataObject::AddUserDataSegment(void *userMemory, uint32_t metaCapacity, uint32_t dataCapacity, 
                                    void (*userCleanupFunc)(void *))
{
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

} // namespace lunasa
