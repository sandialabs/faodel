// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_opbox_hello
//  Purpose: This is just a hello example to demo how we can launch a few nodes
//           with mpi and do a gtest from node 0 that just pings the other
//           nodes. This examples starts up the bootstrap services, but does
//           not use them for anything. This is just a sanity check to make
//           sure mpi apps can still work with all of our code.



#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;



enum op_types {
  ping_req = 1,
  ping_rpl = 2,
  ping_rst = 3,
  ping_done = 4
};
typedef struct {
  int op;
  int val;
} mpi_msg_t;


class MPIHelloTest : public testing::Test {
protected:
  virtual void SetUp() {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  }

  void TearDown() override {
  }

  int mpi_rank, mpi_size;
};

TEST_F(MPIHelloTest, SimplePing) {

  mpi_msg_t msg_o = {};
  mpi_msg_t msg_i = {};

  for(int i = 0; i<5; i++) {
    msg_o = {ping_req, i};
    for(int t = 1; t<mpi_size; t++) {
      MPI_Send(&msg_o, sizeof(mpi_msg_t), MPI_BYTE, t, 0, MPI_COMM_WORLD);
    }
    for(int t = 1; t<mpi_size; t++) {
      msg_i = {};
      MPI_Recv(&msg_i, sizeof(mpi_msg_t), MPI_BYTE, t, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      assert(msg_i.op == ping_rpl);
      assert(msg_i.val == i);
    }
  }
  msg_o = {ping_done, 0};
  for(int t = 1; t<mpi_size; t++) {
    MPI_Send(&msg_o, sizeof(mpi_msg_t), MPI_BYTE, t, 0, MPI_COMM_WORLD);
  }
}

void targetLoop(int rank) {
  int exp_val = 0;
  mpi_msg_t msg;
  do {
    MPI_Recv(&msg, sizeof(mpi_msg_t), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    //cout <<"Node "<<rank<<": received message "<<msg.op<<"|"<<msg.val<<endl;
    switch(msg.op) {
      case ping_req:
        if(msg.val != exp_val) cout << "Unexpected value: " << msg.val << " vs " << exp_val << endl;
        exp_val = msg.val + 1;
        msg.op = ping_rpl;
        MPI_Send(&msg, sizeof(mpi_msg_t), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
        break;
      case ping_rst:
        exp_val = 0;
        break;
      case ping_done:
        break;
      default:
        cout << "Unknown op: " << msg.op << endl;
    }

  } while(msg.op != ping_done);

}

int main(int argc, char **argv) {

  int rc = 0;
  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);


  bootstrap::Start(Configuration(""), opbox::bootstrap);
  faodel::nodeid_t myid = opbox::GetMyID();

  cout << "NODE " << mpi_rank << ": ID is " << myid.GetHex() << endl;

  //Share ids with everyone
  std::vector<faodel::nodeid_t> allids(mpi_size);
  allids[mpi_rank] = myid;
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR,
                allids.data(), sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);


  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);


  cout << "Id is " << opbox::net::GetMyID().GetHex() << endl;

  if(mpi_rank == 0) {
    cout << "Tester begins. Known ids:\n";
    for(int i = 0; i<mpi_size; i++)
      cout << "[" << i << "] " << allids[i].GetHex() << endl;
    rc = RUN_ALL_TESTS();
    cout << "Tester completed all tests.\n";
  } else {
    cout << "Target pausing\n";
    targetLoop(mpi_rank);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  bootstrap::Finish();
  MPI_Finalize();
  return rc;
}
