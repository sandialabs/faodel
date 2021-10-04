// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
//#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"
using namespace std;
using namespace faodel;
using namespace lunasa;
string valid_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.mutex_type   rwlock

node_role server
lunasa.eager_memory_manager tcmalloc

lunasa.debug true
lunasa.allocator.debug true
)EOF";

class LunasaCfgTest : public testing::Test {
protected:
  void SetUp () override {}

  void TearDown () override {}
};


// TEST: success with valid configuration
// note: because tcmalloc is used, you can't pack multiple runs in one test
TEST_F(LunasaCfgTest, validCfgTest) {
  try {
    bootstrap::Init(Configuration(valid_config), lunasa::bootstrap);
    bootstrap::Start();
    bootstrap::Finish();

    #ifndef Faodel_ENABLE_TCMALLOC
      FAIL() << "Should have thrown exception during LUNASA initialization, due to no tcmalloc support.";
    #endif

  } catch(...) {
    #ifdef Faodel_ENABLE_TCMALLOC
      FAIL() << "Exception during LUNASA initialization (with tcmalloc support)";
    #else
      cout <<"Lunasa properly threw an exception when request for tcmalloc and lacked build support\n";
    #endif
  }
}

