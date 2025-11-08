// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "lunasa/allocators/AllocatorTcmalloc.hh"
#include "lunasa/common/Allocation.hh"


#if defined(HAVE_MALLOC_H)
#include "malloc.h"
#elif defined(HAVE_MALLOC_MALLOC_H)
#include "malloc/malloc.h"
#endif

#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif

using namespace std;

namespace lunasa {
namespace internal {

// !!!! TODO: manage statistics properly !!!!
unsigned long bytes = 0;

/* This is the method that the core tcmalloc code uses to request more memory. */
void *AllocatorTcmalloc::TcmallocSysAllocator::Alloc(size_t size, size_t *actual_size, size_t alignment) {
  int rc = 0;
  void *memory;

  bytes += size;
  *actual_size = size;
  rc = posix_memalign(&memory, 8 * 1024, size);
  F_ASSERT(rc == 0, "");
  AllocatorTcmalloc *tcmalloc = GetInstance();
  F_ASSERT(tcmalloc != NULL, "");
  tcmalloc->mTotalManaged += size;

  // if eager, this memory needs to be pinned
  void *pinnedPtr;
  if(tcmalloc->mEagerPinning) {
    tcmalloc->mPinFunc(memory, size, pinnedPtr);
    tcmalloc->AddPinnedRegion(memory, size, pinnedPtr);
  }
  return memory;
}

// Only one instance of this allocator may exist per address space.  

AllocatorTcmalloc::AllocatorTcmalloc(const faodel::Configuration &config, bool eagerPinning)
        : AllocatorBase(config, "Tcmalloc", eagerPinning) {
  dbg("AllocatorTcmalloc ctor()");

  size_t bytes_allocated = 0;
  size_t bytes_managed = 0;
  string min_system_alloc_string;
  size_t min_system_alloc;
  SysAllocator *sa;
  AllocatorTcmalloc::TcmallocSysAllocator *tcmalloc_allocator;

  MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &bytes_allocated);
  MallocExtension::instance()->GetNumericProperty("generic.heap_size", &bytes_managed);
  faodel::rc_t rc = config.GetLowercaseString(&min_system_alloc_string, "lunasa.tcmalloc.min_system_alloc", "");
  if( rc != ENOENT ) {
    min_system_alloc = (size_t)std::stol(min_system_alloc_string);
    MallocExtension::instance()->SetNumericProperty("tcmalloc.min_system_alloc", min_system_alloc);
  }

  sa = MallocExtension::instance()->GetSystemAllocator();
  tcmalloc_allocator = dynamic_cast<AllocatorTcmalloc::TcmallocSysAllocator *>(sa);

  if(tcmalloc_allocator == NULL) {
    // FLUSH any memory that was requested from the system during initialization but was not allocated.
    // All subsequent allocation requests should be handled by using memory obtained with our custom
    // allocator.  We accomplish this by requesting an allocation that is equal to the unallocated memory.
    // It doesn't release it back to the system, but it prevents it from being used to satisfy any
    // allocation requests from the user.

    // By repeatedly allocating from the smallest size class, we'll eventually pull in all of the free memory 
    // from all of the size classes. 
    for(size_t i=0; i < (bytes_managed - bytes_allocated) / 8; i++) {
      tc_malloc(8);
    }

    sa = new AllocatorTcmalloc::TcmallocSysAllocator();
    MallocExtension::instance()->SetSystemAllocator(sa);
  }
}

AllocatorTcmalloc::~AllocatorTcmalloc() {
  //cerr <<"~AllocatorTcmalloc() items are:\n";
  //PrintAllocations(allocations);

  mutex->WriterLock();
  bool danglingRefs = false;
  //Walk through any remaining allocations and free them
  for(auto &alloc_ptr : allocations) {
    if(alloc_ptr->GetRefCount() > 1) danglingRefs = true;
    if(alloc_ptr->local.net_buffer_handle) {
      mUnpinFunc(alloc_ptr->local.net_buffer_handle);
    }
    free(alloc_ptr);
  }
  allocations.clear();
  instance = NULL;
  mutex->Unlock();

  if(danglingRefs) {
    // TODO throw exceptions 
    cerr << "Warning: Lunasa allocator being destroyed but dangling references remain to LDOs\n";
  }
}

AllocatorTcmalloc *AllocatorTcmalloc::instance = NULL;

/* [ SINGLETON discussion ] This method is part of the mechanism that enforces the fact that AllocatorTcmalloc
   is implemented as a singleton.  Why is AllocatorTcmalloc a singleton?  Because tcmalloc is a single 
   monolithic library that relies on global variables/symbols.  As a result, we can use tcmalloc to manage
   either eagerly-allocated memory or lazily-allocated memory, but not both.  There's certainly a way to do 
   this without using a singleton (e.g., with a static variable).  However, given the underlying constraints, 
   it's not clear that there is a clear advantage to *not* using a Singleton.  [sll] */

AllocatorTcmalloc *AllocatorTcmalloc::GetInstance(const faodel::Configuration &config, bool eagerPinning) {
  if(instance == NULL) {
    /* No instance yet exists, construct one. */
    instance = new AllocatorTcmalloc(config, eagerPinning);
  } else if(instance->mEagerPinning != eagerPinning) {//||
    //instance->mThreadingModel != threading_model ||
    //instance->mMutexType != mutex_type) {
    //TODO: fix above checks, now that config holds mutex info. Previously passed in mutex_type and threading_model
    /* Requested instance doesn't match existing instance. */
    throw std::runtime_error("Lunasa configuration attempted to create multiple instaces of tcmalloc allocator (not possible)");
    return NULL;
  }

  return instance;
}

AllocatorTcmalloc *AllocatorTcmalloc::GetInstance() {
  return instance;
}

Allocation *AllocatorTcmalloc::Allocate(uint32_t user_capacity) {
  if(allocator_has_been_shutdown) {
    cerr << "WARNING: attempting to allocate memory after allocator shutdown" << endl;
    return nullptr;
  }

  user_capacity += sizeof(Allocation);
  Allocation *alloc = static_cast<Allocation *>(tc_malloc(user_capacity));

  /* RECORD where the allocation came from. */
  alloc->local.allocator = this;
  alloc->local.net_buffer_handle = GetPinnedAddr(alloc);
  alloc->local.net_buffer_offset = GetPinnedOffset(alloc);
  alloc->local.allocated_bytes = user_capacity;
  alloc->local.user_data_segments = nullptr;

  // Add to the set
  mutex->WriterLock();
  mTotalAllocated += user_capacity;
  mTotalUsed += user_capacity;
  allocations.insert(alloc);
  mutex->Unlock();

  return alloc;
}

void AllocatorTcmalloc::Free(Allocation *allocation) {
  //cerr <<"------> Free item "<<std::hex <<(uint64_t)allocation << std::dec << endl;
  //std::cerr << "pre-free list:\n";
  //PrintAllocations(allocations);
  bool is_empty;

  mutex->ReaderLock();
  auto it = allocations.find(allocation);
  mutex->Unlock();

  if(it == allocations.end()) {
    throw LunasaConfigurationException("Invalid FREE; Unrecognized allocation");
  }

  mutex->WriterLock();
  allocations.erase(it);
  mutex->Unlock();

  if(mEagerPinning == false && allocation->local.net_buffer_handle != nullptr) {
    mUnpinFunc(allocation->local.net_buffer_handle);
  }
  mTotalAllocated -= allocation->local.allocated_bytes;
  mTotalUsed -= allocation->local.allocated_bytes - sizeof(Allocation);
  tc_free(allocation);

  mutex->ReaderLock();
  is_empty = allocations.empty();
  mutex->Unlock();

  //If this was the last allocation and we were shutdown, delete ourself
  if((is_empty) && (allocator_has_been_shutdown)) {
    delete this;
  }

  // TODO throw exception if we didn't find the allocation
  //std::cerr << "Allocation " << std::hex <<(uint64_t)allocation << std::dec << " wasn't allocated by this implementation"
  //          << " db: "<<allocation->data_bytes<<" mb: "<<allocation->meta_bytes<<std::endl
  //          << "Current list:\n";
  //PrintAllocations(allocations);
}

void AllocatorTcmalloc::AddPinnedRegion(void *addr, size_t size, void *pinned_addr) {
  mutex->WriterLock();
  pinned_regions[addr] = std::pair<size_t, void *>(size, pinned_addr);
  mutex->Unlock();
}

void *AllocatorTcmalloc::GetPinnedAddr(Allocation *allocation) {
  mutex->ReaderLock();
  std::map<void *, std::pair<size_t, void *>>::iterator it;
  it = pinned_regions.lower_bound(allocation);
  if(it->first != allocation) {
    // We want the predecessor; the largest value that is less than <addr>
    F_ASSERT(it != pinned_regions.begin(), "");
    it--;
  }
  std::pair<size_t, void *> value = it->second;
  F_ASSERT((char *) allocation >= (char *) it->first, "");
  F_ASSERT((char *) it->first + value.first > (char *) allocation, "");
  mutex->Unlock();

  return value.second;
}

uint64_t AllocatorTcmalloc::GetPinnedOffset(Allocation *allocation) {
  mutex->ReaderLock();
  std::map<void *, std::pair<size_t, void *>>::iterator it;
  it = pinned_regions.lower_bound(allocation);
  if(it->first != allocation) {
    // We want the predecessor; the largest value that is less than <addr>
    F_ASSERT(it != pinned_regions.begin(), "");
    it--;
  }
  std::pair<size_t, void *> value = it->second;
  F_ASSERT((char *) allocation >= (char *) it->first, "");
  F_ASSERT((char *) it->first + value.first > (char *) allocation, "");

  uint64_t offset = (uint64_t) ((char *) allocation - (char *) it->first);
  mutex->Unlock();
  return offset;
}

bool AllocatorTcmalloc::SanityCheck() {
  // !!!! TODO !!!!
  return true;
}

void AllocatorTcmalloc::PrintState(ostream &stream) {
/* TODO when malloc.h exists this'll work
  struct mallinfo info = mallinfo();
  stream << "Total Tracked " << info.arena << std::endl;
  stream << "Total Allocated " << info.uordblks << std::endl;
  stream << "Total Free " << info.fordblks << std::endl;
*/
}

/**
 * @brief Determine if this allocator has allocations that are currently in use
 * @retval true Active allocations exist 
 * @retval false The allocator is currently empty
 */
bool AllocatorTcmalloc::HasActiveAllocations() const {
  bool is_empty;
  mutex->ReaderLock();
  is_empty = allocations.empty();
  mutex->Unlock();
  return !is_empty;
}

/* REPORTS the total number of bytes that have been allocated to satisfy user requests.
   Includes bytes requested for metadata. */
size_t AllocatorTcmalloc::TotalAllocated() const {
  return mTotalAllocated;
}

/* REPORTS the total number of bytes managed by the allocator */
size_t AllocatorTcmalloc::TotalManaged() const {
  return mTotalManaged;
}

/* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
   plus overhead). */
size_t AllocatorTcmalloc::TotalUsed() const {
  return mTotalUsed;
}

/* REPORTS the total number of bytes that are not currently in use. */
size_t AllocatorTcmalloc::TotalFree() const {
  return (mTotalManaged - mTotalUsed);
}

void AllocatorTcmalloc::whookieMemoryAllocations(faodel::ReplyStream &rs, const string &allocator_name) {
  rs.tableBegin("Lunasa " + allocator_name + " Memory Allocations");
  rs.tableTop({"Allocated Bytes", "RefCount", "MetaBytes", "DataBytes"});
  mutex->ReaderLock();
  for(auto &a : allocations) {
    rs.tableRow({
                        to_string(a->local.allocated_bytes),
                        to_string(a->local.ref_count),
                        to_string(a->header.meta_bytes),
                        to_string(a->header.data_bytes)});
  }
  mutex->Unlock();
  rs.tableEnd();
}

void AllocatorTcmalloc::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth < 0) return;
  ss << std::string(indent, ' ') << "[Allocator] "
     << " Type: " << AllocatorType()
     << " Pinning: " << ((mEagerPinning) ? string("Eager") : string("Lazy"))
     << " TotalAllocated: " << to_string(TotalAllocated())
     << endl;

  if(depth < 1) return;
  ss << std::string(indent + 2, ' ') << "DataObjects:\n";
  for(size_t i = 0; i < allocations.size(); i++) {
    ss << std::string(indent + 6, ' ') << "[" << i << "]: todo";
  }
}

} //namespace internal
} //namespace lunasa
