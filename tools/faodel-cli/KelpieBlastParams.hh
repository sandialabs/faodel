// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_KELPIEBLASTPARAMS_HH
#define FAODEL_KELPIEBLASTPARAMS_HH

#include <string>
#include <vector>
#include <thread>

#include "faodel-common/Common.hh"


class KelpieBlastParams {
public:

  KelpieBlastParams() = delete;
  KelpieBlastParams(const std::vector<std::string> &args);
  ~KelpieBlastParams();


  int mpi_rank;
  int mpi_size;

  std::string pool_name;

  uint64_t timestep;
  uint64_t num_timesteps;
  uint64_t delay_between_timesteps_us;

  int k1_length;
  int k2_length;

  std::vector<uint64_t> object_sizes_per_timestep;
  uint64_t max_object_size;
  uint64_t bytes_per_rank_step;

  bool reuse_memory;
  bool async_pubs;
  bool use_rft_keys;
  bool no_barrier_before_generate;

  void barrier();


  void dumpSettings(faodel::DirectoryInfo dir_info);

  void sleepUS(uint64_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

  void SleepForComputePhase() { sleepUS(delay_between_timesteps_us); }


  bool IsOk() { return !failed; }

private:

  int verbose;
  bool failed;
  std::string pool_external;
  std::string pool_internal;

  void dumpHelp();

  int parseArgs(const std::vector<std::string> &args);


};


#endif //FAODEL_KELPIEBLASTPARAMS_HH
