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


string invalid_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.mutex_type   rwlock

node_role server

# Attempts to create multiple instances of tcmalloc-based allocator (should fail)
lunasa.eager_memory_manager tcmalloc
lunasa.lazy_memory_manager tcmalloc


# This line causes bootstrap to throw an exception instead of handling it and erroring out!
bootstrap.exit_on_errors false
lunasa.debug true
bootstrap.debug true


)EOF";

class LunasaCfgTest : public testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};


// TEST: failure with invalid configuration (multiple instances of tcmalloc allocator)
TEST_F(LunasaCfgTest, invalidCfgTest) {

  cout << "\n\ntb_LunasaConfiguration3 note: This test tries to use tcmalloc in different\n"
       << "  allocators, which is illegal. You should see an error message below about\n"
       << "  multiple instances of tcmalloc. It's ok!\n\n";

  #ifdef Faodel_ENABLE_TCMALLOC
    EXPECT_ANY_THROW(bootstrap::Init(Configuration(invalid_config), lunasa::bootstrap));
  #else
    //No tcmalloc support should throw its own exception
    EXPECT_ANY_THROW(bootstrap::Init(Configuration(invalid_config), lunasa::bootstrap));
  #endif

  cout << "\n ^--- Expect an error message above. It's ok!! ----^\n\n";

}

