// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_ALLOCATORMALLOC_HH
#define LUNASA_ALLOCATORMALLOC_HH 1

#include "lunasa/allocators/AllocatorBase.hh"

namespace lunasa {
namespace internal {

class AllocatorMalloc
        : public AllocatorBase {


public:
  AllocatorMalloc(const faodel::Configuration &config, bool eagerPinning);

  Allocation *Allocate(uint32_t user_capacity) override;
  void Free(Allocation *allocation) override;

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);

  bool SanityCheck() override;

  void PrintState(std::ostream& stream) override;

  /* REPORTS the total number of bytes allocated to the user (excludes overhead such as 
     memory used to store Allocation structure). */
  size_t TotalAllocated() const override;

  /* REPORTS the total number of bytes managed by the allocator */
  size_t TotalManaged() const;

  /* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
     plus overhead). */
  size_t TotalUsed() const override;

  /* REPORTS the total number of bytes that are not currently in use. */
  size_t TotalFree() const override;

  std::string AllocatorType() const override { return "malloc"; }

  bool HasActiveAllocations() const override;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  

private:
  ~AllocatorMalloc() override;

  std::set<Allocation *> allocations;
 
  // Not implemented
  AllocatorMalloc(AllocatorMalloc&);
  void operator=(AllocatorMalloc&);
};

} // namespace internal
} // namespace lunasa

#endif // LUNASA_ALLOCATORMALLOC_HH
