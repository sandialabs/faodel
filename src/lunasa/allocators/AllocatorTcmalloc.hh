// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATORTCMALLOC_HH
#define LUNASA_ALLOCATORTCMALLOC_HH 1

#include "lunasa/allocators/AllocatorBase.hh"
#include <gperftools/tcmalloc.h>
#include <gperftools/malloc_extension.h>

namespace lunasa{

class AllocatorTcmalloc : public AllocatorBase {
public:
  // !!!! TODO: Add description !!!!
  static AllocatorTcmalloc *GetInstance(const faodel::Configuration &config, bool eagerPinning);
  static AllocatorTcmalloc *GetInstance();

  allocation_t *Allocate(uint32_t capacity) override;
  void Free (allocation_t *allocation) override;

  bool SanityCheck() override;

  void PrintState(std::ostream& stream) override;

  /* REPORTS the total number of bytes allocated to the user (excludes overhead such as 
     memory used to store allocation_t structure). */
  size_t TotalAllocated() const override;

  /* REPORTS the total number of bytes managed by the allocator */
  size_t TotalManaged() const override;

  /* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
     plus overhead). */
  size_t TotalUsed() const override;

  /* REPORTS the total number of bytes that are not currently in use. */
  size_t TotalFree() const override;


  std::string AllocatorType() const override { return "tcmalloc"; }

  bool HasActiveAllocations() const override;

  void webhookMemoryAllocations(webhook::ReplyStream &rs, const std::string &allocator_name) override;
  

  void AddPinnedRegion(void *addr, size_t size, void *pinned_addr);
  void *GetPinnedAddr(allocation_t *allocation);
  uint64_t GetPinnedOffset(allocation_t *allocation);

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  
private:
  virtual ~AllocatorTcmalloc ();
  AllocatorTcmalloc(const faodel::Configuration &config, bool eagerPinning);
  static AllocatorTcmalloc *instance;

  std::set<allocation_t *> allocations;
  std::map<void *, std::pair<size_t, void *>> pinned_regions;

  class TcmallocSysAllocator : public ::SysAllocator {
    void *Alloc(size_t size, size_t *actual_size, size_t alignment);
  };
 
  // Not implemented
  AllocatorTcmalloc (AllocatorTcmalloc&);
  void operator= (AllocatorTcmalloc&);
};

} // namespace lunasa

#endif // LUNASA_ALLOCATORTCMALLOC_HH
