// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATORMALLOC_HH
#define LUNASA_ALLOCATORMALLOC_HH 1

#include "lunasa/allocators/AllocatorBase.hh"

namespace lunasa{

class AllocatorMalloc : public AllocatorBase {
public:
  AllocatorMalloc(const faodel::Configuration &config, bool eagerPinning);

  allocation_t *Allocate(uint32_t capacity);
  void Free (allocation_t *allocation);

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);

  bool SanityCheck () ;

  void PrintState (std::ostream& stream) ;

  /* REPORTS the total number of bytes allocated to the user (excludes overhead such as 
     memory used to store allocation_t structure). */
  size_t TotalAllocated () const;

  /* REPORTS the total number of bytes managed by the allocator */
  size_t TotalManaged () const;

  /* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
     plus overhead). */
  size_t TotalUsed () const;

  /* REPORTS the total number of bytes that are not currently in use. */
  size_t TotalFree () const;

  std::string AllocatorType() const { return "malloc"; }
  virtual bool HasActiveAllocations() const;

  //InfoInterface
  virtual void sstr(std::stringstream &ss, int depth=0, int indent=0) const;
  

private:
  virtual ~AllocatorMalloc ();

  std::set<allocation_t *> allocations;
 
  // Not implemented
  AllocatorMalloc (AllocatorMalloc&);
  void operator= (AllocatorMalloc&);
};

} // namespace lunasa

#endif // LUNASA_ALLOCATORMALLOC_HH
