// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

#include <mpi.h>

#include <gtest/gtest.h>

#include "faodel-common/Common.hh"
#include "faodel-services/BackBurner.hh"

using namespace std;
using namespace faodel;

void sleepUS(int microseconds) {
  std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config = R"EOF(
backburner.debug true
#backburner.worker.debug true
node_role server
backburner.threads 4

#backburner.notification_method polling

backburner.notification_method pipe

#backburner.notification_method sleep_polling
#backburner.sleep_polling_time 5s
)EOF";


class FaodelBackBurnerService : public testing::Test {
protected:
  void SetUp() override {
    Configuration config(default_config);
    config.AppendFromReferences();
    bootstrap::Init(config, backburner::bootstrap);
    bootstrap::Start();

  }

  void TearDown() override {
    bootstrap::Finish();
  }

};

TEST_F(FaodelBackBurnerService, simple) {
  std::atomic<int> val;
  val=0;

  backburner::AddWork( [&val] () { val++; return 0; });
  while(val==0) {}
  EXPECT_EQ(1, val);
}

TEST_F(FaodelBackBurnerService, multiple) {

  std::atomic<int>  count;
  std::atomic<bool> done;
  int num;

  count = 0;
  done  = false;
  num   = 1000;

  for(int i=0; i<num; i++) {
    backburner::AddWork( [&count] () { count++; return 0; });
  }
  backburner::AddWork( [&done] () { done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(1000, count);


  //Redo with delay to allow multiple requests to stack up
  done=false;
  for(int i=0; i<num; i++) {
    backburner::AddWork( [&count] () { sleepUS(5); count++; return 0; });
  }
  backburner::AddWork( [&done] () { done=true; return 0;});
  while(!done) {}
  EXPECT_EQ(2000, count);
}

TEST_F(FaodelBackBurnerService, tags) {

  std::atomic<int>  count;
  std::atomic<bool> done;
  int num;

  count = 0;
  done  = false;
  num   = 1000;

  for(int i=0; i<num; i++) {
    backburner::AddWork( i, [&count] () { count++; return 0; });
  }
  backburner::AddWork( [ &count, &num, &done] () { while ( count != num ); done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(1000, count);

  //Redo with delay to allow multiple requests to stack up
  done=false;
  for(int i=0; i<num; i++) {
    backburner::AddWork( i, [&count] () { sleepUS(5); count++; return 0; });
  }
  backburner::AddWork( [ &count, &num, &done] () { while ( count != 2*num ); done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(2000, count);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if(mpi_rank==0) cout <<"Beginning tests.\n";
  int rc = RUN_ALL_TESTS();

  MPI_Finalize();
  if(mpi_rank==0) cout <<"All complete. Exiting.\n";
  return rc;
}
