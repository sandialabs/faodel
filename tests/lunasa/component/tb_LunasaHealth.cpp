// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
//#include <mpi.h>
#include <iostream>
#include <algorithm>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

int num_tests=0;

string default_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
)EOF";

class LunasaHealth : public testing::Test {
protected:
  virtual void SetUp(){
    bootstrap::Init(Configuration(default_config), lunasa::bootstrap);
    bootstrap::Start();
    
    num_tests++;
  }
  virtual void TearDown(){
    bootstrap::Finish();
  }
};


TEST_F(LunasaHealth, inits){
  EXPECT_TRUE(Lunasa::SanityCheck());
}

int main(int argc, char **argv){

  //Yuck.  We don't need mpi, but nnti calls it for the default transport
  //and complains if you've done more than one test (irecv after fini).
  //Instead, start the mpiinit in the 
  ::testing::InitGoogleTest(&argc, argv);
  int mpi_rank = 0,mpi_size;
  // MPI_Init(&argc, &argv);
  // MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  // MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  // cout <<"MPI Hello from node "<<mpi_rank<<" of "<<mpi_size<<endl;
  
  if(mpi_rank==0){
    int rc =RUN_ALL_TESTS();
  }
  //sleep(30);
  cout <<"Done\n";
  
  // MPI_Finalize();

}

