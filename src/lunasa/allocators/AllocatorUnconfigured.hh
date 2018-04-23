// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATORUNCONFIGURED_HH
#define LUNASA_ALLOCATORUNCONFIGURED_HH


#include "lunasa/allocators/AllocatorBase.hh"

namespace lunasa {

class AllocatorUnconfigured : public AllocatorBase {
public:
  AllocatorUnconfigured();

  allocation_t *Allocate(uint32_t capacity)
  {
    Panic("Allocate");
    return nullptr;
  }

  void Free(allocation_t* allocation) 
  {          
    Panic("Free");
  }
  
  DataObject FindAllocation(allocation_t* allocation) 
  { 
    Panic("FindAllocation"); 
    return DataObject (); 
  }

  int GetRdmaPtr(allocation_t *allocation, void **rdma_ptr, uint32_t &offset)
  {
    Panic("GetRdmaPtr"); 
    return -1; 
  }

  int GetRdmaPtrs(allocation_t *allocation, std::queue<DataObject::rdma_segment_desc> &rdma_segments)
  {
    Panic("GetRdmaPtrs"); 
    return -1; 
  }

  bool   SanityCheck () {                        Panic("SanityCheck"); return false; }
  void   PrintState (std::ostream& stream) {     Panic("PrintState"); } 
  size_t PageSize () const {                     Panic("PageSize"); return 0; }
  void   PageSize (size_t size)  {               Panic("PageSize"); }
  size_t TotalPages () const {                   Panic("TotalPages"); return 0; }
  bool   HasActiveAllocations() const {          return false; } //Legit use during startup
  size_t TotalAllocated () const {               Panic("TotalAllocated"); return 0; }
  size_t TotalManaged () const {                 Panic("TotalManaged"); return 0; }
  size_t TotalUsed () const {                    Panic("TotalUsed"); return 0; }
  size_t TotalFree () const {                    Panic("TotalFree"); return 0; }

  std::string AllocatorType() const { return "unconfigured"; }

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

  
private:

  void Panic(std::string fname) const;

  // Not implemented
  AllocatorUnconfigured (AllocatorUnconfigured&);
  void operator= (AllocatorUnconfigured&);
};

} // namespace lunasa

#endif // LUNASA_ALLOCATORUNCONFIGURED_HH
