// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOBKEYS_HH
#define FAODEL_JOBKEYS_HH

#include <map>


#include "Job.hh"

/***
 * @brief Generate and sort a number of kelpie keys
 *
 * Kelpie uses a simple 2-string key to track objects. These tests measure
 * how quickly new keys of different sizes can be generated and sorted.
 */
class JobKeys :
    public Job {

public:
  explicit JobKeys(const faodel::Configuration &config)
   : Job(config, JobCategoryName()) {
    for(auto &k_p : options)
      job_names.push_back(k_p.first);
  }

  ~JobKeys() override = default;

  int Execute(const std::string &job_name) override;

  struct params_t { uint32_t num_keys; size_t k1_len; size_t k2_len;};
  const std::map<std::string, params_t> options = {
           {"GenSort-ShortRowKey", {1024, 16,  0}},
           {"GenSort-ShortColKey", {1024, 0,   16}},
           {"GenSort-Short2DKey",  {1024, 16,  16}},
           {"GenSort-LongRowKey",  {1024, 255, 0}},
           {"GenSort-LongColKey",  {1024, 0,   255}},
           {"GenSort-Long2DKey",   {1024, 255, 255}}
  };

  constexpr const char* JobCategoryName() { return "keys"; }

};



#endif // FAODEL_JOBKEYS_HH