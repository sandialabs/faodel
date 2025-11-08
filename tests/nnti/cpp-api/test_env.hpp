// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef TEST_ENV_HPP
#define TEST_ENV_HPP

#include <mpi.h>

#include "gtest/gtest.h"

#include "nnti/nntiConfig.h"

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "faodel-common/Bootstrap.hh"

#include "test_utils.hpp"

using namespace std;
using namespace faodel;

// Create a namespace for the NNTI tests
namespace FaodelNntiTests {
  
// Define a struct to contain the global values needed a test.
// These are the common values that all the NNTI test use to 
// allocate a transport, decide which ranks are servers and 
// exchange server urls.
struct Globals {
    Configuration config;

    nnti::transports::transport *t=nullptr;

    int mpi_rank, mpi_size;
    int root_rank;

    char                   server_url[1][NNTI_URL_LEN];
    const uint32_t         num_servers = 1;
    uint32_t               num_clients;
    bool                   i_am_server = false;
};
// Use a pointer so we can control when this object gets instantiated.
static struct Globals *globals=nullptr;

// Define a subclass of the googletest Environment.
// This class gets registered with googletest in main().  
// The SetUp() and TearDown() methods get called exactly 
// once before and after all the tests.
class Environment : public ::testing::Environment {
protected:
  std::string logfile_basename;
  std::string default_config_string;
public:
  Environment(std::string logfile_basename,
              std::string config_string)
  : logfile_basename(logfile_basename),
    default_config_string(config_string)
  { }
  ~Environment() override {}

  // Override this to define how to set up the environment.
  void SetUp() override
  {
      globals = new Globals();

      MPI_Comm_rank(MPI_COMM_WORLD, &globals->mpi_rank);
      MPI_Comm_size(MPI_COMM_WORLD, &globals->mpi_size);
      globals->root_rank = 0;
      globals->config = Configuration(default_config_string);
      globals->config.AppendFromReferences();

      MPI_Barrier(MPI_COMM_WORLD);

      // test_setup() will call bootstrap::Start()
      test_setup(0,
                 NULL,
                 globals->config,
                 this->logfile_basename.c_str(),
                 globals->server_url,
                 globals->mpi_size,
                 globals->mpi_rank,
                 globals->num_servers,
                 globals->num_clients,
                 globals->i_am_server,
                 globals->t);
  }

  // Override this to define how to tear down the environment.
  void TearDown() override
  {
      NNTI_result_t nnti_rc = NNTI_OK;
      bool init;

      init = globals->t->initialized();
      EXPECT_TRUE(init);

      if (init) {
          nnti_rc = globals->t->stop();
          EXPECT_EQ(nnti_rc, NNTI_OK);
      }

      MPI_Barrier(MPI_COMM_WORLD);
      bootstrap::Finish();
  }
};
}

#endif /* TEST_ENV_HPP */
