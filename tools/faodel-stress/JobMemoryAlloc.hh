// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOBMEMORYALLOC_HH
#define FAODEL_JOBMEMORYALLOC_HH

#include "Job.hh"

/***
 * @brief Request different memory allocations from lunasa
 *
 * Faodel uses Lunasa to allocate memory for objects that are shipped over
 * the network and hide the high overhead of obtaining registered memory
 * from applications. This test allocates different sizes of memory and then
 * releases them. Memory can either be plain memory (via the lazy allocator)
 * or registered memory (via the eager allocator).
 */
class JobMemoryAlloc :
        public Job {

public:
  explicit JobMemoryAlloc(const faodel::Configuration &config);

  ~JobMemoryAlloc() override = default;

  int Execute(const std::string &job_name) override;


  struct params_t { uint32_t num_items; bool netmem; uint32_t min_ldo_size; uint32_t max_ldo_size; };
  const std::map<std::string, params_t> options = {
          {"PlainMem-FixedSize-1K",       { 1024, false, 1024,1024}},
          {"PlainMem-FixedSize-1M",       { 1024, false, 1024*1024,1024*1024}},
          {"PlainMem-RandomSize-1K",      { 1024, false, 128, 1024}},
          {"PlainMem-RandomSize-1M",      { 1024, false, 1024, 1024*1024}},

          {"RegisteredMem-FixedSize-1K",  { 1024, true, 1024,1024}},
          {"RegisteredMem-FixedSize-1M",  { 1024, true, 1024*1024,1024*1024}},
          {"RegisteredMem-RandomSize-1K", { 1024, true, 128, 1024}},
          {"RegisteredMem-RandomSize-1M", { 1024, true, 1024, 1024*1024}}
  };
  constexpr const char* JobCategoryName() { return "memalloc"; }

};


#endif //FAODEL_JOBMEMORYALLOC_HH
