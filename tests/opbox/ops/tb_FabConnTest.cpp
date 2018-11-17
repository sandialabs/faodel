// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "webhook/Server.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

security_bucket                       bobbucket

# Tester: Run a dedicated tester that has a resource manager tester named /
tester.rpc_tester_type                single
#tester.net.url.write_to_file          .tester-url
tester.resource_manager.type          tester
tester.resource_manager.path          /bob
tester.resource_manager.write_to_file .tester-url
tester.webhook.interfaces             ipogif0,eth,lo

# Client: Don't use a tester, just send requests
target.webhook.interfaces              ipogif0,eth,lo
target.rpc_tester_type                 none
target.resource_manager.path           /bob/1
target.resource_manager.read_from_file .tester-url
)EOF";


class OpboxConnectTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  virtual void SetUp() {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    root_rank = 0;
  }

  virtual void TearDown() {
  }
};

TEST_F(OpboxConnectTest, start1) {
  std::cout << "Our MPI rank is " << mpi_rank << std::endl;

  nodeid myid = webhook::Server::GetMyNodeID();
  std::cout << "Our webhook server is: " << myid.GetHttpLink() << endl;

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(attrs);


  faodel::nodeid_t gather_result[mpi_size];
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result, sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

  if(mpi_rank == root_rank) {
  } else {
    opbox::net::peer_t *peer;
    faodel::nodeid_t root_nodeid = gather_result[root_rank];
    opbox::net::Connect(&peer, root_nodeid);
  }
}

int main(int argc, char **argv) {
  int rc = 0;
  ::testing::InitGoogleTest(&argc, argv);
  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  Configuration conf(default_config_string);
  if(argc>1) {
    if(string(argv[1]) == "-v") {
      conf.Append("loglevel all");
    } else if(string(argv[1]) == "-V") {
      conf.Append("loglevel all\nnssi_rpc.loglevel all");
    }
  }
  conf.Append("node_role", (mpi_rank == 0) ? "tester" : "target");
  bootstrap::Start(conf, opbox::bootstrap);

  rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
