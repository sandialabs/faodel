// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <assert.h>

#include "faodel-common/Debug.hh"
#include "lunasa/allocators/Allocators.hh"

using namespace std;

namespace lunasa {
namespace internal {

AllocatorBase * createAllocator(const faodel::Configuration &config,
                                string allocator_name, bool eagerPinning) {

  //cout <<"CreateAllocators: "<<allocator_name<<" "<<( (eagerPinning)?"Eager" :"Lazy")<<endl;

  if(allocator_name=="malloc") return new AllocatorMalloc(config, eagerPinning);
  if(allocator_name=="unconfigured") return new AllocatorUnconfigured();
  if(allocator_name=="tcmalloc") {
#ifdef Faodel_ENABLE_TCMALLOC
    return AllocatorTcmalloc::GetInstance(config, eagerPinning);
#else
    
    faodel::fatal_fn("LunasaAllocator","Requested tcmalloc allocator, but Lunasa was not built with support for tcmalloc.");
#endif /* Faodel_ENABLE_TCMALLOC */
  }
  
  faodel::fatal_fn("LunasaAllocator", "Unknown Allocator '"+allocator_name+"' given to lunasa createAllocator");
  return nullptr;
}
AllocatorBase * reuseAllocator(AllocatorBase *existing_allocator){
  existing_allocator->IncrRef();
  return existing_allocator;
}


} // namespace internal
} // namespace lunasa
