// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_opbox_ping
//  Purpose: Test our ability to ping different nodes using opbox


#include <mpi.h>

#include <iostream>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"

#include "opbox/ops/OpCount.hh"
#include "opbox/ops/OpPing.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

tester.whookie.port 1991
rooter.whookie.port 1992
server.whookie.port 2000


dirman.root_role rooter

target.dirman.host_root
target.dirman.write_to_file ./dirman.txt

dirman.type centralized

#bootstrap.debug true
#whookie.debug true
#opbox.debug true
#dirman.debug true

)EOF";



class MPIPingTest : public testing::Test {
protected:
  void SetUp() override {  }

  void TearDown() override {  }
};

TEST_F(MPIPingTest, LocalExecute){
  using namespace opbox;

  Op *op1 = new OpPing(G.peers[1], "this is the ping");
  future<string> fut1 = static_cast<OpPing *>(op1)->GetFuture();
  opbox::LaunchOp(op1);

  OpPing *op2 = new OpPing(G.peers[1], "this is the other ping");
  future<string> fut2 = op2->GetFuture();
  opbox::LaunchOp(op2);


  string res1=fut1.get();
  string res2=fut2.get();
  EXPECT_EQ("THIS IS THE PING", res1);
  EXPECT_EQ("THIS IS THE OTHER PING", res2);

//  cout <<"Done\n";

}

TEST_F(MPIPingTest, MultiTarget){
  using namespace opbox;

  std::vector<Op*>            outstanding_ops(G.mpi_size);
  std::vector<future<string>> outstanding_futures(G.mpi_size);

  for (int i=1;i<G.mpi_size;i++) {
      outstanding_ops[i-1]     = new OpPing(G.peers[i], "this is the ping");
      outstanding_futures[i-1] = static_cast<OpPing *>(outstanding_ops[i - 1])->GetFuture();
      opbox::LaunchOp(outstanding_ops[i-1]);
  }

  for (int i=1;i<G.mpi_size;i++) {
      string result = outstanding_futures[i-1].get();
      EXPECT_EQ("THIS IS THE PING", result);
  }

//  cout <<"Done\n";

}

TEST_F(MPIPingTest, MultiPing){
  using namespace opbox;

  int pings_per_rank=10;
  std::vector<Op*>            outstanding_ops(G.mpi_size * pings_per_rank);
  std::vector<shared_future<string>> outstanding_futures(G.mpi_size * pings_per_rank);

  for(int i=1;i<G.mpi_size;i++) {
    for(int j=0;j<pings_per_rank;j++) {
      int index = ((i-1)*pings_per_rank) + j;
      outstanding_ops[index]     = new OpPing(G.peers[i], "this is the ping"+std::to_string(index));
      outstanding_futures[index] = static_cast<OpPing *>(outstanding_ops[index])->GetFuture();
      opbox::LaunchOp(outstanding_ops[index]);
    }
  }

  for(int i=0;i<((G.mpi_size-1) * pings_per_rank);i++) {
    string result = outstanding_futures[i].get();
    string expected = "THIS IS THE PING"+std::to_string(i);
    EXPECT_EQ(expected, result);
  }


//  cout <<"Done\n";

}

void targetLoop(){
  G.dump();
}



int main(int argc, char **argv){

  int rc=0;

  ::testing::InitGoogleTest(&argc, argv);


  //Set the configuration for the two types of nodes (tester and target)
  opbox::RegisterOp<OpPing>();
  faodel::Configuration config("");
  G.StartAll(argc, argv, config);

  if (G.mpi_size < 2) {
      std::cerr << "This test requires at least two ranks.  Aborting..." << std::endl;
      exit(-1);
  }

  //Split the work into two sections: the tester (node 0) and the targets
  if(G.mpi_rank==0){
    //cout <<"Tester begins\n";
    rc = RUN_ALL_TESTS();
    //cout << "Tester completed all tests.\n";
    sleep(1);
  } else {
    //cout <<"Target running\n";
    targetLoop();
    sleep(1);
  }

  G.StopAll();

  return rc;
}
