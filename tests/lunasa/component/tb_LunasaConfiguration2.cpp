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


class LunasaCfgTest2 : public testing::Test {
protected:
  virtual void SetUp () {}
  virtual void TearDown () {}
};


//First time: Should pass, second time fail
TEST_F(LunasaCfgTest2, validCfgTest) {

  //First time pass
  EXPECT_NO_THROW(bootstrap::Init(valid_config, lunasa::bootstrap));
  bootstrap::Start();
  bootstrap::Finish();

  //Second time fail
  EXPECT_ANY_THROW(bootstrap::Init(valid_config, lunasa::bootstrap));
}
