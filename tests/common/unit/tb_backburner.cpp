// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "gtest/gtest.h"


#include "common/Common.hh"


using namespace std;
using namespace faodel;

void sleepUS(int microseconds) {
  std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}


string default_config = R"EOF(
config.additional_files.env_name.if_defined   FAODEL_CONFIG
#backburner.debug true
#backburnerWorker.debug true
node_role server
backburner.threads 4
)EOF";


class FaodelBackBurner : public testing::Test {
protected:
  virtual void SetUp() {
    Configuration config(default_config);
    config.AppendFromReferences();
    bb = new faodel::backburner::internal::BackBurner();
    bb->Init(config);
    bb->Start();
  }
  virtual void TearDown() {
    bb->Finish();
    delete bb;
  }
  faodel::backburner::internal::BackBurner *bb;
};

TEST_F(FaodelBackBurner, simple) {
  std::atomic<int> val;
  val=0;

  bb->AddWork( [&val] () { val++; return 0; });
  while(val==0) {}
  EXPECT_EQ(1, val);
}

TEST_F(FaodelBackBurner, multiple) {

  std::atomic<int>  count;
  std::atomic<bool> done;
  int num;

  count = 0;
  done  = false;
  num   = 1000;

  for(int i=0; i<num; i++) {
    bb->AddWork( [&count] () { count++; return 0; });
  }
  bb->AddWork( [&done] () { done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(1000, count);


  //Redo with delay to allow multiple requests to stack up
  done=false;
  for(int i=0; i<num; i++) {
    bb->AddWork( [&count] () { sleepUS(5); count++; return 0; });
  }
  bb->AddWork( [&done] () { done=true; return 0;});
  while(!done) {}
  EXPECT_EQ(2000, count);
}

TEST_F(FaodelBackBurner, tags) {

  std::atomic<int>  count;
  std::atomic<bool> done;
  int num;

  count = 0;
  done  = false;
  num   = 1000;

  for(int i=0; i<num; i++) {
    bb->AddWork( i, [&count] () { count++; return 0; });
  }
  bb->AddWork( [ &count, &num, &done] () { while ( count != num ); done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(1000, count);

  //Redo with delay to allow multiple requests to stack up
  done=false;
  for(int i=0; i<num; i++) {
    bb->AddWork( i, [&count] () { sleepUS(5); count++; return 0; });
  }
  bb->AddWork( [ &count, &num, &done] () { while ( count != 2*num ); done=true; return 0; });
  while(!done) {}
  EXPECT_EQ(2000, count);
}
