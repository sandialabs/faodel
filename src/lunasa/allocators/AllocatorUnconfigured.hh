// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_ALLOCATORUNCONFIGURED_HH
#define LUNASA_ALLOCATORUNCONFIGURED_HH


#include "lunasa/allocators/AllocatorBase.hh"

namespace lunasa {
namespace internal {


class AllocatorUnconfigured
        : public AllocatorBase {

public:
  AllocatorUnconfigured();

  Allocation *Allocate(uint32_t user_capacity) override {
    Panic("Allocate");
    return nullptr;
  }

  void Free(Allocation* allocation) override {
    Panic("Free");
  }
  
  DataObject FindAllocation(Allocation* allocation) {
    Panic("FindAllocation"); 
    return DataObject (); 
  }

  int GetRdmaPtr(Allocation *allocation, void **rdma_ptr, uint32_t &offset) {
    Panic("GetRdmaPtr"); 
    return -1; 
  }

  int GetRdmaPtrs(Allocation *allocation, std::queue<DataObject::rdma_segment_desc> &rdma_segments) {
    Panic("GetRdmaPtrs"); 
    return -1; 
  }

  bool   SanityCheck () override {                        Panic("SanityCheck"); return false; }
  void   PrintState (std::ostream& stream) override {     Panic("PrintState"); }
  size_t PageSize () const {                     Panic("PageSize"); return 0; }
  void   PageSize (size_t size)  {               Panic("PageSize"); }
  size_t TotalPages () const {                   Panic("TotalPages"); return 0; }
  bool   HasActiveAllocations() const override {          return false; } //Legit use during startup
  size_t TotalAllocated () const override {               Panic("TotalAllocated"); return 0; }
  size_t TotalManaged () const override {                 Panic("TotalManaged"); return 0; }
  size_t TotalUsed () const override {                    Panic("TotalUsed"); return 0; }
  size_t TotalFree () const override {                    Panic("TotalFree"); return 0; }

  std::string AllocatorType() const override { return "unconfigured"; }

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

  
private:
  void Panic(std::string fname) const;

  // Not implemented
  AllocatorUnconfigured(AllocatorUnconfigured&);
  void operator=(AllocatorUnconfigured&);
};

} // namespace internal
} // namespace lunasa

#endif // LUNASA_ALLOCATORUNCONFIGURED_HH
