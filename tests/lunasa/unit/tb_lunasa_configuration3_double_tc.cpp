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


string invalid_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server

# Attempts to create multiple instances of tcmalloc-based allocator (should fail)
lunasa.eager_memory_manager tcmalloc
lunasa.lazy_memory_manager tcmalloc
)EOF";

class LunasaCfgTest : public testing::Test {
protected:
  virtual void SetUp () {}
  virtual void TearDown () {}
};


// TEST: failure with invalid configuration (multiple instances of tcmalloc allocator)
TEST_F(LunasaCfgTest, invalidCfgTest) {
  cout << "[ TEST INVALID CONFIGURATION: multiple instances of tcmalloc allocator ]" << endl;

  EXPECT_THROW(bootstrap::Init(Configuration(invalid_config), lunasa::bootstrap),
               lunasa::LunasaConfigurationException);
}
