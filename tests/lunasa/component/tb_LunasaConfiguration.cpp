// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
//#include <mpi.h>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"
using namespace std;
using namespace faodel;
using namespace lunasa;
string valid_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
lunasa.eager_memory_manager tcmalloc

lunasa.debug true
lunasa.allocator.debug true
)EOF";

class LunasaCfgTest : public testing::Test {
protected:
  virtual void SetUp () {}
  virtual void TearDown () {}
};


// TEST: success with valid configuration
// note: because tcmalloc is used, you can't pack multiple runs in one test
TEST_F(LunasaCfgTest, validCfgTest) {
  try {
    bootstrap::Init(valid_config, lunasa::bootstrap);
    bootstrap::Start();
    bootstrap::Finish();
  } catch(...) {
    FAIL() << "Exception during LUNASA initialization";
  }
}

