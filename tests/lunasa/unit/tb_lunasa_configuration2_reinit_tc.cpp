// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

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
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
lunasa.eager_memory_manager tcmalloc

lunasa.debug true
lunasa.allocator.debug true
)EOF";


class LunasaCfgTest2 : public testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};


//First time: Should pass, second time fail
TEST_F(LunasaCfgTest2, validCfgTest) {

  //First time request for tcmalloc
  #ifdef Faodel_ENABLE_TCMALLOC
    //If tcmalloc, this should pass just fine
    EXPECT_NO_THROW(bootstrap::Init(Configuration(valid_config), lunasa::bootstrap));
  #else
    //If no tcmalloc, we should get an exception because it's not available
    EXPECT_ANY_THROW(bootstrap::Init(Configuration(valid_config), lunasa::bootstrap));
    cout <<"No tcmalloc support. Only checked to make sure exception happened.\n";
    return;
  #endif

  bootstrap::Start();
  bootstrap::Finish();

  cout << "\n\ntb_LunasaConfiguration2 note: This test verifies a warning message is thrown\n"
       << "  when the bootstrap is run multiple times and mem manager is tcmalloc. You\n"
       << "  should see an error message below about tcmalloc. It's ok!\n\n";
  //Second time fail
  EXPECT_ANY_THROW(bootstrap::Init(Configuration(valid_config), lunasa::bootstrap));

  cout << "\n ^--- Expect an error message above. It's ok!!----^\n\n";
}
