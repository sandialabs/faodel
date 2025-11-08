// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "faodel-common/Common.hh"
#include "whookie/Server.hh"
#include "opbox/OpBox.hh"

#include "whookie/Server.hh"
#include "whookie/client/Client.hh"
#include "faodel-common/QuickHTML.hh"

#include "opbox/ops/OpPing.hh"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

#
security_bucket                       bobbucket

# Tester: Run a dedicated tester that has a resource manager tester named /
tester.rpc_tester_type                single
#tester.net.url.write_to_file          .tester-url
tester.resource_manager.type          tester
tester.resource_manager.path          /bob
tester.resource_manager.write_to_file .tester-url
tester.whookie.interfaces             ipogif0,eth,lo

# Client: Don't use a tester, just send requests
client.rpc_tester_type                 none
client.whookie.interfaces             ipogif0,eth,lo
client.resource_manager.path           /bob/1
client.resource_manager.read_from_file .tester-url
)EOF";


class OpboxOpPingFabTest : public testing::Test {
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

TEST_F(OpboxOpPingFabTest, start1) {
  std::cout << "Our MPI rank is " << mpi_rank << std::endl;


  //Now try pulling the data  from libfaric interface back
  int rc;
  string result;


  faodel::nodeid_t myid = opbox::GetMyID();
  cout << "Our nodeid is " << myid.GetHex() << endl;
  cout << "Our web address is: " << myid.GetHttpLink("/fab/iblookup") << endl;
  rc = whookie::retrieveData(myid, "/fab/iblookup", &result);

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(attrs);

  //Share node ids with everyone
  faodel::nodeid_t all_nodes[mpi_size];
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, all_nodes, sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);


  if(mpi_rank == root_rank) {
    //for (int i=0;i<5;i++) {
    //     opbox::OpPing ping_dst(op_create_as_target);
    //}
  } else {
    sleep(1);

    opbox::net::peer_t *peer;
    opbox::net::Connect(&peer, all_nodes[root_rank]);

    for(int i = 0; i<5; i++) {
      opbox::OpPing ping_src(peer, "This is a test!");
    }
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
