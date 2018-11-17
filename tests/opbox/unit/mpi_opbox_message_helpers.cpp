// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// Test:    mpi_opbox_message_helpers.cpp
// Purpose: Test helper functions that convert incoming messages to outgoing
// Note:    needs mpi for node id to be initialized

#include <limits.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-common/SerializationHelpers.hh"

#include "webhook/Server.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

#include "opbox/common/MessageHelpers.hh"

#include <stdio.h>


using namespace std;
using namespace faodel;
using namespace opbox;

#include "support/default_config_string.hh"

class OpBoxMessageHelpersTest : public testing::Test {
protected:
  virtual void SetUp() {
    faodel::Configuration config(multitest_config_string);


    //Force this to an mpi implementation to make running easier
    config.Append("net.transport.name", "mpi");
    bootstrap::Start(config, opbox::bootstrap);

    src_node = faodel::nodeid_t(0x1975, iuo);
    dst_node = faodel::nodeid_t(0x1976, iuo);
    src_text = "Hello this is a test message";
    dst_text = "This is the reply";
    my_node = opbox::GetMyID();

  }

  virtual void TearDown() {
    bootstrap::FinishSoft();
  }

  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bool ok;

  faodel::nodeid_t my_node;
  faodel::nodeid_t src_node;
  faodel::nodeid_t dst_node;
  string src_text;
  string dst_text;

};


TEST_F(OpBoxMessageHelpersTest, StandardMessage) {

  lunasa::DataObject ldo;

  AllocateStandardMessage(ldo, dst_node, 100, 2112, 0x1234);
  EXPECT_EQ(1, ldo.internal_use_only.GetRefCount());
  EXPECT_EQ(sizeof(message_t), ldo.GetDataSize());

  auto *msg = ldo.GetDataPtr<message_t *>();
  EXPECT_TRUE(msg != nullptr);
  EXPECT_EQ(dst_node, msg->dst);
  EXPECT_EQ(100, msg->src_mailbox);
  EXPECT_EQ(MAILBOX_UNSPECIFIED, msg->dst_mailbox); //Should be new mailbox
  EXPECT_EQ(2112, msg->op_id);
  EXPECT_EQ(0x1234, msg->user_flags);
  EXPECT_EQ(0, msg->body_len);
}

TEST_F(OpBoxMessageHelpersTest, StringMessage) {

  lunasa::DataObject ldo;

  AllocateStringMessage(ldo, src_node, dst_node, 100, 101, 2112, 0x1234, src_text);
  EXPECT_EQ(1, ldo.internal_use_only.GetRefCount());
  EXPECT_EQ(sizeof(message_t) + src_text.size(), ldo.GetDataSize());

  auto msg = ldo.GetDataPtr<message_t *>();
  EXPECT_TRUE(msg != nullptr);
  EXPECT_EQ(src_node, msg->src);
  EXPECT_EQ(dst_node, msg->dst);
  EXPECT_EQ(100, msg->src_mailbox);
  EXPECT_EQ(101, msg->dst_mailbox);
  EXPECT_EQ(2112, msg->op_id);
  EXPECT_EQ(0x1234, msg->user_flags);
  EXPECT_EQ(src_text.size(), msg->body_len);

  auto s = UnpackStringMessage(msg);
  EXPECT_EQ(src_text, s);
}


TEST_F(OpBoxMessageHelpersTest, BigStringMessage) {

  lunasa::DataObject ldo;
  string s1 = string((64*1024) - 1, 'x');

  AllocateStringMessage(ldo, src_node, dst_node, 100, 101, 2112, 0x1234, s1);
  EXPECT_EQ(1, ldo.internal_use_only.GetRefCount());
  EXPECT_EQ(sizeof(message_t) + 65535, ldo.GetDataSize());

  auto *msg = ldo.GetDataPtr<message_t *>();
  EXPECT_TRUE(msg != nullptr);
  EXPECT_EQ(src_node, msg->src);
  EXPECT_EQ(dst_node, msg->dst);
  EXPECT_EQ(100, msg->src_mailbox);
  EXPECT_EQ(101, msg->dst_mailbox);
  EXPECT_EQ(2112, msg->op_id);
  EXPECT_EQ(0x1234, msg->user_flags);
  EXPECT_EQ(65535, msg->body_len);

  auto s = UnpackStringMessage(msg);
  EXPECT_EQ(s1, s);
  EXPECT_EQ(65535, s.size());
}

TEST_F(OpBoxMessageHelpersTest, BadStringMessage) {

  lunasa::DataObject ldo;
  string s1 = string((64*1024), 'x');

  EXPECT_ANY_THROW(AllocateStringMessage(ldo, src_node, dst_node, 100, 101, 2112, 0x1234, s1));
}


TEST_F(OpBoxMessageHelpersTest, StringRequestReply) {

  lunasa::DataObject ldo;

  AllocateStringRequestMessage(ldo, dst_node, 100, 2112, 0x1234, src_text);
  EXPECT_EQ(1, ldo.internal_use_only.GetRefCount());
  EXPECT_EQ(sizeof(message_t) + src_text.size(), ldo.GetDataSize());

  auto *req_msg = ldo.GetDataPtr<message_t *>();
  EXPECT_TRUE(req_msg != nullptr);
  EXPECT_EQ(my_node, req_msg->src); //Note: src is updated with myid when message generated
  EXPECT_EQ(dst_node, req_msg->dst);
  EXPECT_EQ(100, req_msg->src_mailbox);
  EXPECT_EQ(MAILBOX_UNSPECIFIED, req_msg->dst_mailbox); //Should be new mailbox
  EXPECT_EQ(2112, req_msg->op_id);
  EXPECT_EQ(0x1234, req_msg->user_flags);
  EXPECT_EQ(src_text.size(), req_msg->body_len);

  auto s = UnpackStringMessage(req_msg);
  EXPECT_EQ(src_text, s);


  //Turn it around and send back
  lunasa::DataObject ldo2;
  AllocateStringReplyMessage(ldo2, req_msg, 0x5678, dst_text);
  auto *reply_msg = ldo2.GetDataPtr<message_t *>();
  EXPECT_TRUE(reply_msg != nullptr);
  EXPECT_EQ(my_node, reply_msg->src); //Note: src is updated with myid when this is generated
  EXPECT_EQ(my_node, reply_msg->dst); //Note: dst is copied from original message, which was updated when generated
  EXPECT_EQ(MAILBOX_UNSPECIFIED, reply_msg->src_mailbox);
  EXPECT_EQ(100, reply_msg->dst_mailbox); //Should be new mailbox
  EXPECT_EQ(2112, reply_msg->op_id);
  EXPECT_EQ(0x5678, reply_msg->user_flags);
  EXPECT_EQ(dst_text.size(), reply_msg->body_len);

  auto s2 = UnpackStringMessage(reply_msg);
  EXPECT_EQ(dst_text, s2);
}


// We need to run MPI init once for all tests.. Does not need to be mpirun though
int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);


  int rc = RUN_ALL_TESTS();

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return rc;
}
