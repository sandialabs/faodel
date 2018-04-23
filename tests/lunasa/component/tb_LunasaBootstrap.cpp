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
string default_config = R"EOF(

node_role server
lunasa.eager_memory_manager tcmalloc

lunasa.debug           true
lunasa.allocator.debug true

)EOF";

class LunasaBootstrapTest : public testing::Test {
protected:
  virtual void SetUp () {
  }
  virtual void TearDown () {
  }
};

TEST_F(LunasaBootstrapTest, simpleSetups) {
  
  bootstrap::Init(default_config, lunasa::bootstrap);
  bootstrap::Start();
  bootstrap::Finish();
}

//TODO: Add more tests here. Right now a second test fails due to double bootstrap stuff
