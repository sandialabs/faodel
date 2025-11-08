// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "lunasa/allocators/AllocatorBase.hh"
#include "lunasa/common/Allocation.hh"

using namespace std;

namespace lunasa {
namespace internal {

void noopPin(void *base_addr, size_t length, void *&pinned) { /* noop */ }

void noopUnpin(void *&pinned) { /* noop */ }

void fakePin(void *base_addr, size_t length, void *&pinned) {
  pinned = (void *) 1;
}

void fakeUnpin(void *&pinned) {
  pinned = nullptr;
}


void PrintAllocations(std::set<Allocation *> allocations) {
  int i = 0;
  for(auto &ptr : allocations) {
    std::cerr << "[" << i << "] " << std::hex << (uint64_t) ptr << std::dec
              << " db: " << ptr->header.data_bytes << " mb " << ptr->header.meta_bytes
              << " rc: " << ptr->GetRefCount()
              << " pn: " << ptr->IsPinned()
              << " " << ptr->local.allocator->AllocatorType() << std::endl;
    i++;
  }
}

/**
 * @brief Constructor for allocator base class
 *
 * @param[in] config Configuration information
 * @param[in] subcomponent_name The name of the derived class (for labeling purposes)
 * @param[in] eagerPinning Whether to use eager pinning or not
 */
AllocatorBase::AllocatorBase(const faodel::Configuration &config, string subcomponent_name, bool eagerPinning)
        : LoggingInterface("lunasa.allocator", subcomponent_name),
          mRefCount(1), // start with the reference we're allocating here
          allocator_has_been_shutdown(false),
          mTotalManaged(0),
          mTotalAllocated(0),
          mTotalUsed(0),
          mTotalFree(0),
          mEagerPinning(eagerPinning) {

  RegisterPinUnpin(fakePin, fakeUnpin);
  mutex = config.GenerateComponentMutex("lunasa.allocator", "rwlock");

  ConfigureLogging(config); //Set logging
  dbg("Creating allocator ");
}

/**
 * @brief Increase the refcount for a particular allocator (eg, when used in multiple places)
 * @note The refcount only counts instances of an allocator, not the LDOs that need the allocator
 */
void AllocatorBase::IncrRef() {
  mRefCount++;
}

/**
 * @brief Decrease the number of instances that use this allocator (possibly destorying)
 * @retval refcount The remaining number of references to this allocator
 * @note The refcount only counts instances of an allocator, not the LDOs that need the allocator
 */
int AllocatorBase::DecrRef() {
  dbg("Allocator DecrRef for " + AllocatorType());
  --mRefCount;
  int num_left = mRefCount.load();
  if(num_left == 0) {
    //This allocator is no longer owned by anyone. If the nobody is holding
    //on to an allocation, then destroy us. Otherwise, disable this allocator
    //and let any existing allocations continue on so LDO's can clean up
    //themselves.
    if(!HasActiveAllocations()) {
      delete this;
    } else {
      allocator_has_been_shutdown = true;
    }
  }
  return num_left;
}

/**
 * @brief Get the reference count for this allocator
 * @retval refcount The remaining number of references to this allocator
 */
int AllocatorBase::RefCount() {
  return mRefCount.load();
}

/**
 * @brief One-time registration function for specifying network pin/unpin functions
 * @param[in] pin The function handler used to pin a block of memory for network use
 * @param[in] unpin The function handler used to unpin a block of memory for network use
 * @note: Internally, this also sets the eager/lazy pin functions accordingly
 */
void AllocatorBase::RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) {
  // save the application pinning functions for later
  mPinFunc = pin;
  mUnpinFunc = unpin;
}

void AllocatorBase::RegisterMemory(void *base_addr, size_t length, void *&pinned) {
  if(mPinFunc) {
    mPinFunc(base_addr, length, pinned);
  } else {
    cerr << "WARNING: Attempting to pin memory without registering pin function" << endl;
    pinned = nullptr;
  }
}

bool AllocatorBase::UsingEagerRegistration() {
  return mEagerPinning;
}

bool AllocatorBase::UsingLazyRegistration() {
  return !mEagerPinning;
}


void AllocatorBase::whookieStatus(faodel::ReplyStream &rs, const std::string &allocator_name) {
  rs.tableBegin("Lunasa " + allocator_name + " Allocator");
  rs.tableTop({"Parameter", "Setting"});
  rs.tableRow({"Allocator Type", AllocatorType()});
  rs.tableRow({"Total Allocated", to_string(mTotalAllocated)});
  rs.tableRow({"Total Managed", to_string(mTotalManaged)});
  rs.tableRow({"Total Used", to_string(mTotalUsed)});
  rs.tableRow({"Total Free", to_string(mTotalFree)});
  rs.tableEnd();
}

void AllocatorBase::whookieMemoryAllocations(faodel::ReplyStream &rs, const string &allocator_name) {
  rs.mkSection("Lunasa " + allocator_name + " Memory Allocations");
  rs.mkText("Allocator does not provide listing support");
}

/**
 * @brief Dump debug info
 */
void AllocatorBase::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth < 0) return;
  ss << std::string(indent, ' ') << "[Allocator] "
     << " Type: " << AllocatorType()
     << " Pinning: " << ((mEagerPinning) ? string("Eager") : string("Lazy"))
     << " TotalAllocated: " << to_string(TotalAllocated())
     << endl;
}


} //namespace internal
} //namespace lunasa
