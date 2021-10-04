// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_DHTDRIVER_HH
#define KELPIE_DHTDRIVER_HH

#include <string>
#include <iostream>

#include "boost/program_options.hpp"

#include "faodel-common/Common.hh"

namespace opts = boost::program_options;

namespace kelpie {
/**
 * @brief A server for hosting a pool on a collection of MPI nodes
 */
class PoolServerDriver {
public:
  PoolServerDriver(int argc, char **argv);
  PoolServerDriver() = delete;
  virtual ~PoolServerDriver();

  virtual int run();

protected:

  void command_line_options();

  void start_dirman();

  void stop_dirman();

  void whookie_killswitch();

  opts::options_description op_desc;
  opts::variables_map op_map;

  static std::string config_string;

  int argc;
  char **argv;

  int mpi_rank;
  int comm_size;

  std::string pool_url_string;
  std::string pool_info;
  std::string iom_name;
  std::string iom_dir;
  std::string iom_type;
  std::string root_id_filename;

  int dirroot_rank_;

  faodel::Configuration config;

  bool keep_going;

};

}


#endif
    
