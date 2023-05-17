// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_JOBSERDES_HH
#define FAODEL_JOBSERDES_HH

#include <map>

#include "Job.hh"

/***
 * @brief A stress test that serializes/deserializes different objects to/from Lunasa DataObjects
 *
 * Faodel allows you to use whatever serialization library you want, provided
 * that object data is packed into a single, continuous buffer. There's always
 * a tradeoff between how easy it is to serdes an object and how quickly it
 * can be converted. This test packs a few different types of data structure
 * into LDOs, using Boost or Lunasa's serdes helpers.
 */
class JobSerdes :
        public Job {

public:
  explicit JobSerdes(const faodel::Configuration &config);

  ~JobSerdes() override = default;

  int Execute(const std::string &job_name) override;

  struct params_t { uint32_t num_iters; int obj_type; int method; bool pack; bool unpack; int num_items; int item_len_min; int item_len_max;};
  const std::map<std::string, params_t> options = {
          {"Strings-Pack-Small-Boost",             {64, 1, 1, true, false,16, 4, 16}},
          {"Strings-Unpack-Small-Boost",           {64, 1, 1, false,true, 16, 4, 16}},
          {"Strings-PackUnpack-Small-Boost",       {64, 1, 1, true, true, 16, 4, 16}},

          {"Strings-Pack-Small-Cereal",            {64, 1, 2, true, false,16, 4, 16}},
          {"Strings-Unpack-Small-Cereal",          {64, 1, 2, false,true, 16, 4, 16}},
          {"Strings-PackUnpack-Small-Cereal",      {64, 1, 2, true, true, 16, 4, 16}},

          {"Strings-Pack-Small-LDOPacker",         {64, 1, 3, true, false,16, 4, 16}},
          {"Strings-Unpack-Small-LDOPacker",       {64, 1, 3, false,true, 16, 4, 16}},
          {"Strings-PackUnpack-Small-LDOPacker",   {64, 1, 3, true, true, 16, 4, 16}},


          {"Strings-Pack-Large-Boost",             {64, 1, 1, true, false,256, 32, 256}},
          {"Strings-Unpack-Large-Boost",           {64, 1, 1, false,true, 256, 32, 256}},
          {"Strings-PackUnpack-Large-Boost",       {64, 1, 1, true, true, 256, 32, 256}},

          {"Strings-Pack-Large-Cereal",            {64, 1, 2, true, false,256, 32, 256}},
          {"Strings-Unpack-Large-Cereal",          {64, 1, 2, false,true, 256, 32, 256}},
          {"Strings-PackUnpack-Large-Cereal",      {64, 1, 2, true, true, 256, 32, 256}},


          {"Strings-Pack-Large-LDOPacker",         {64, 1, 3, true, false,256, 32, 256}},
          {"Strings-Unpack-Large-LDOPacker",       {64, 1, 3, false,true, 256, 32, 256}},
          {"Strings-PackUnpack-Large-LDOPacker",   {64, 1, 3, true, true, 256, 32, 256}},


          {"Particles-Pack-Small-Boost",           {64, 2, 1, true, false,1024, 0, 0}},
          {"Particles-Unpack-Small-Boost",         {64, 2, 1, false,true, 1024, 0, 0}},
          {"Particles-PackUnpack-Small-Boost",     {64, 2, 1, true, true, 1024, 0, 0}},

          {"Particles-Pack-Small-Cereal",          {64, 2, 2, true, false,1024, 0, 0}},
          {"Particles-Unpack-Small-Cereal",        {64, 2, 2, false,true, 1024, 0, 0}},
          {"Particles-PackUnpack-Small-Cereal",    {64, 2, 2, true, true, 1024, 0, 0}},

          {"Particles-Pack-Small-LDOPacker",       {64, 2, 3, true, false,1024, 0, 0}},
          {"Particles-Unpack-Small-LDOPacker",     {64, 2, 3, false,true, 1024, 0, 0}},
          {"Particles-PackUnpack-Small-LDOPacker", {64, 2, 3, true, true, 1024, 0, 0}}

  };

  constexpr const char* JobCategoryName() { return "serdes"; }

};



#endif //FAODEL_JOBSERDES_HH
