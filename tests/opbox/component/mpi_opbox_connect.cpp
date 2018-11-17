// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;


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

  void TearDown() override {
  }
};

TEST_F(OpboxConnectTest, start1) {
  int rc;

  //std::cout << "Our MPI rank is " << mpi_rank << std::endl;

  faodel::nodeid_t myid = opbox::GetMyID();
  //std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

  std::vector<faodel::nodeid_t> gather_result(mpi_size);
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result.data(), sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

  for(int i = 0; i<10; i++) {
    if(mpi_rank == root_rank) {
    } else {
      opbox::net::peer_t *peer;
      faodel::nodeid_t root_nodeid = gather_result[root_rank];
      rc = opbox::net::Connect(&peer, root_nodeid.GetIP().c_str(), root_nodeid.GetPort().c_str());
      EXPECT_EQ(rc, 0);
      rc = opbox::net::Disconnect(peer);
      EXPECT_EQ(rc, 0);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

TEST_F(OpboxConnectTest, start2) {
  int rc;

  //std::cout << "Our MPI rank is " << mpi_rank << std::endl;

  faodel::nodeid_t myid = opbox::GetMyID();
  //std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

  std::vector<faodel::nodeid_t> gather_result(mpi_size);
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result.data(), sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

  for(int i = 0; i<10; i++) {
    if(mpi_rank == root_rank) {
    } else {
      opbox::net::peer_t *peer;
      faodel::nodeid_t root_nodeid = gather_result[root_rank];
      rc = opbox::net::Connect(&peer, root_nodeid);
      EXPECT_EQ(rc, 0);
      rc = opbox::net::Disconnect(root_nodeid);
      EXPECT_EQ(rc, 0);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  bootstrap::Start(Configuration(""), opbox::bootstrap);

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
