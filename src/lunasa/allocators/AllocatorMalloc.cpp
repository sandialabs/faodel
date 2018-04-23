// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "lunasa/allocators/AllocatorMalloc.hh"
#include "lunasa/common/Allocation.hh"

using namespace std;
namespace lunasa {

AllocatorMalloc::AllocatorMalloc(const faodel::Configuration &config, bool eagerPinning) 
  : AllocatorBase(config, "Malloc", eagerPinning) 
{
  dbg("AllocatorMalloc ctor()");
}

AllocatorMalloc::~AllocatorMalloc() 
{
  dbg("~AllocatorMalloc(): have "+std::to_string(allocations.size())+" allocations left");
  //PrintAllocations(allocations);

  mutex->WriterLock();
  bool danglingRefs = false;
  //Walk through any remaining allocations and free them
  for(auto &alloc_ptr : allocations){
    if(alloc_ptr->GetRefCount() > 1) danglingRefs=true;
    if(alloc_ptr->local.net_buffer_handle){
      mUnpinFunc(alloc_ptr->local.net_buffer_handle);
      alloc_ptr->local.net_buffer_handle = nullptr;
    }
#ifdef DEBUG 
    cerr <<"~AllocatorMalloc(): free " << alloc_ptr << endl;
#endif
    free(alloc_ptr);
  }
  allocations.clear();
  mutex->Unlock();
  
  if(danglingRefs) {
    // TODO throw exceptions 
    cerr << "Warning: Lunasa allocator being destroyed but dangling references remain to LDOs\n";
  }
}

allocation_t *AllocatorMalloc::Allocate(uint32_t capacity)
{
  if( allocator_has_been_shutdown ) {
    cerr << "WARNING: attempting to allocate memory after allocator shutdown" << endl;
    return nullptr;
  }

  int total_capacity = capacity + sizeof(allocation_t);
  allocation_t *alloc = static_cast<allocation_t *>(malloc(total_capacity));

  /* RECORD where the allocation came from. */
  alloc->local.allocator = this;
  alloc->local.net_buffer_handle = nullptr;
  alloc->local.net_buffer_offset = 0;
  alloc->local.allocated_bytes = total_capacity;
  alloc->local.user_data_segments = nullptr;

  // Pin the whole chunk when eager
  if( mEagerPinning ) {
    mPinFunc(alloc, total_capacity, alloc->local.net_buffer_handle);
  }

  // Add to the set
  mutex->WriterLock();
  mTotalAllocated += total_capacity;
  mTotalUsed += capacity;
  allocations.insert(alloc);
  mutex->Unlock();

  return alloc;
}

void AllocatorMalloc::Free(allocation_t *allocation) {

  bool is_empty;

  mutex->WriterLock();
  auto it = allocations.find(allocation);
  if(it != allocations.end()){
    allocations.erase(it);
    if(allocation->local.net_buffer_handle!=nullptr)
    {
      mUnpinFunc(allocation->local.net_buffer_handle);
    }
    mTotalAllocated -= allocation->local.allocated_bytes;
    mTotalUsed -= allocation->local.allocated_bytes - sizeof(allocation_t);
    free(allocation);
  } else {
    assert(0);
  }

  is_empty = allocations.empty();
  mutex->Unlock();

  //If this was the last allocation and we were shutdown, delete ourself
  if((is_empty) && (allocator_has_been_shutdown)){
    delete this;
  }

  // TODO throw exception if we didn't find the allocation
  //std::cerr << "Allocation " << std::hex <<(uint64_t)allocation << std::dec << " wasn't allocated by this implementation"
  //          << " db: "<<allocation->data_bytes<<" mb: "<<allocation->meta_bytes<<std::endl
  //          << "Current list:\n";
  //PrintAllocations(allocations);
}

bool AllocatorMalloc::SanityCheck() {
  return true;
}
void AllocatorMalloc::PrintState(ostream& stream) {
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
bool AllocatorMalloc::HasActiveAllocations() const {
  bool is_empty;
  mutex->ReaderLock();
  is_empty = allocations.empty();
  mutex->Unlock();
  return !is_empty;
}
size_t AllocatorMalloc::TotalAllocated () const {
  return mTotalAllocated;
}

/* REPORTS the total number of bytes managed by the allocator */
size_t AllocatorMalloc::TotalManaged () const {
  return 0;
}

/* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
   plus overhead). */
size_t AllocatorMalloc::TotalUsed () const {
  return 0;
}

/* REPORTS the total number of bytes that are not currently in use. */
size_t AllocatorMalloc::TotalFree () const {
  return 0;
}

void AllocatorMalloc::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << std::string(indent,' ') << "[Allocator] "
     << " Type: "<<AllocatorType()
     << " Pinning: "<< ((mEagerPinning) ? string("Eager") : string("Lazy"))
     << " TotalAllocated: "<< to_string(TotalAllocated())
     << endl;

  if(depth<1) return;
  ss << std::string(indent+2,' ') << "DataObjects:\n";
  for(size_t i=0; i<allocations.size(); i++){
    ss << std::string(indent+6,' ') << "["<<i<<"]: todo";
  }

}

} //namespace lunasa
