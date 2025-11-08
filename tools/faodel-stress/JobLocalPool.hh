// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOBLOCALPOOL_HH
#define FAODEL_JOBLOCALPOOL_HH

#include "Job.hh"

/***
 * @brief A stress test that writes objects into the local store in different ways
 *
 * This stress test focuses on inserting and dropping objects into a kelpie local
 * pool. This pool uses a row/column notation and uses a good bit of locking to
 * ensure that multiple threads do not disturb each other. This test picks keys
 * in a way to either avoid collisions or cause contention.
 */
class JobLocalPool :
        public Job {

public:
  explicit JobLocalPool(const faodel::Configuration &config);

  ~JobLocalPool() override = default;

  int Execute(const std::string &job_name) override;


  struct params_t { uint32_t num_kvs; uint64_t ldo_size; bool allocate_ondemand; int key_strategy; };
  const std::map<std::string, params_t> options = {
          {"PutGetDrop-1D",         {1024, 1024, false, 1}},
          {"PutGetDrop-PrivateRows",{1024, 1024, false, 2}},
          {"PutGetDrop-CombinedRow",{1024, 1024, false, 3}},

          {"AllocatePutGetDrop-1D",         {1024, 1024, true, 1}},
          {"AllocatePutGetDrop-PrivateRows",{1024, 1024, true, 2}},
          {"AllocatePutGetDrop-CombinedRow",{1024, 1024, true, 3}}


  };
  constexpr const char* JobCategoryName() { return "localpool"; }

};


#endif //FAODEL_JOBLOCALPOOL_HH
