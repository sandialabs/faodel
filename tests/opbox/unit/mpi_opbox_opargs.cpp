// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// Test:    mpi_opbox_opargs
// Purpose: Test passing arg objects around (verify types and recast)
// Note:    needs mpi for node id to be initialized

#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

#include "support/default_config_string.hh"

class OpBoxOpArgsTest : public testing::Test {
protected:
  void SetUp() override {
    faodel::Configuration config(multitest_config_string);
    bootstrap::Start(config, opbox::bootstrap);
    header.op_id = 0x1234;
  }

  void TearDown() override {
    bootstrap::FinishSoft();
  }

  message_t header;

  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bool ok;
  int rc;
};


TEST_F(OpBoxOpArgsTest, TypeChecks) {

  OpArgs args_start(UpdateType::start);
  OpArgs args_success(UpdateType::send_success);
  OpArgs args_msg(0, &header);

  EXPECT_NO_THROW(args_start.VerifyTypeOrDie(UpdateType::start, "tst"));
  EXPECT_NO_THROW(args_success.VerifyTypeOrDie(UpdateType::send_success, "tst"));
  EXPECT_ANY_THROW(args_start.VerifyTypeOrDie(UpdateType::send_success, "tst"));

}

TEST_F(OpBoxOpArgsTest, Recasts) {

  OpArgs args_start(UpdateType::start);
  OpArgs args_success(UpdateType::send_success);
  OpArgs args_msg(0, &header);

  EXPECT_FALSE(args_start.IsIncomingMessage());
  EXPECT_TRUE(args_msg.IsIncomingMessage());

}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int rc = RUN_ALL_TESTS();

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return rc;
}
