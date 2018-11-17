// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATORS_HH
#define LUNASA_ALLOCATORS_HH

#include "faodelConfig.h" //for Faodel_ENABLE_TCMALLOC setting

#include "lunasa/allocators/AllocatorBase.hh"
#include "lunasa/allocators/AllocatorMalloc.hh"
#ifdef Faodel_ENABLE_TCMALLOC
#include "lunasa/allocators/AllocatorTcmalloc.hh"
#endif /* Faodel_ENABLE_TCMALLOC */
#include "lunasa/allocators/AllocatorUnconfigured.hh"

namespace lunasa {
namespace internal {

AllocatorBase * createAllocator(const faodel::Configuration &config, 
                                std::string allocator_name, 
                                bool eagerPinning);

AllocatorBase * reuseAllocator(AllocatorBase *);

} // namespace internal
} // namespace lunasa

#endif // LUNASA_ALLOCATORS_HH
