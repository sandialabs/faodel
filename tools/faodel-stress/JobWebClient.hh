// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOBWEBCLIENT_HH
#define FAODEL_JOBWEBCLIENT_HH

#include <map>

#include "Job.hh"

class JobWebClient :
        public Job {
public:
  explicit JobWebClient(const faodel::Configuration &config);

  ~JobWebClient() override = default;

  int Init() override;

  int Execute(const std::string &job_name) override;

  int wh_Reply(const std::map<std::string,std::string> &args, std::stringstream &results) const;

  struct params_t { uint32_t batch_size;  int item_len_min; int item_len_max;};
  const std::map<std::string, params_t> options = {
          {"GetEmpty",             {1024,  0, 0}},
          {"GetFixed-128",         {1024,  128, 128}},
          {"GetFixed-1K",          {1024,  1024, 1024}},
          {"GetRandom-128",        {1024,  128-64, 128+64}},
          {"GetRandom-1K",         {1024,  1024-128, 1024+128}}
  };

  constexpr const char* JobCategoryName() { return "webclient"; }

};


#endif //FAODEL_JOBWEBCLIENT_HH
