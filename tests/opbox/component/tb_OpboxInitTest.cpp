// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

bootstrap.debug                               false
webhook.debug                                 false

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

)EOF";


class OpboxInitTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  virtual void SetUp () {
      MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
      MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
      root_rank = 0;
  }
  virtual void TearDown () {
  }
};

TEST_F(OpboxInitTest, start1) {
    sleep(1);
}

int main(int argc, char **argv){

    ::testing::InitGoogleTest(&argc, argv);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    Configuration conf(default_config_string);
    conf.AppendFromReferences();
    if(argc>1){
        if(string(argv[1])=="-v"){         conf.Append("loglevel all");
        } else if(string(argv[1])=="-V"){  conf.Append("loglevel all\nnssi_rpc.loglevel all");
        }
    }
    conf.Append("node_role", (mpi_rank==0) ? "tester" : "target");
    bootstrap::Start(conf, opbox::bootstrap);


    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Barrier(MPI_COMM_WORLD);
    bootstrap::Finish();

    MPI_Finalize();
    return rc;
}
