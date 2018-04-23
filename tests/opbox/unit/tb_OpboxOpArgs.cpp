// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

string default_config_string = R"EOF(
# for finishSoft
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc
)EOF";

class OpBoxOpArgsTest : public testing::Test {
protected:
  virtual void SetUp(){
    faodel::Configuration config(default_config_string);
    bootstrap::Start(config, opbox::bootstrap);
    header.op_id=0x1234;
  }
  virtual void TearDown(){
    bootstrap::FinishSoft();
  }
  message_t header;

  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bool ok;
  int rc;
};


TEST_F(OpBoxOpArgsTest, TypeChecks){

  OpArgs args_start(UpdateType::start);
  OpArgs args_success(UpdateType::send_success);
  OpArgs args_msg(0, &header);

  try { args_start.VerifyTypeOrDie(UpdateType::start, "tst"); EXPECT_TRUE(true);
  } catch(exception e){ EXPECT_FALSE(true); }

  try { args_success.VerifyTypeOrDie(UpdateType::send_success, "tst"); EXPECT_TRUE(true);
  } catch(exception e){ EXPECT_FALSE(true); }

  try { args_start.VerifyTypeOrDie(UpdateType::send_success, "tst"); EXPECT_FALSE(true);
  } catch(exception e){ EXPECT_TRUE(true); }

}

TEST_F(OpBoxOpArgsTest, Recasts){

  OpArgs args_start(UpdateType::start);
  OpArgs args_success(UpdateType::send_success);
  OpArgs  args_msg(0, &header);
  OpArgs *args_msg_ptr = &args_msg;

  OpArgs *args;
  EXPECT_FALSE(args_start.IsIncomingMessage());
  EXPECT_TRUE(args_msg.IsIncomingMessage());

}


int main(int argc, char **argv){
  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int rc = RUN_ALL_TESTS();

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
}
