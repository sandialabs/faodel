// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <limits.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"

#include "dirman/DirMan.hh"
#include "dirman/ops/OpDirManCentralized.hh"
#include "dirman/ops/msg_dirman.hh"

#include "opbox/OpBox.hh"

#include <stdio.h>
using namespace std;
using namespace faodel;
using namespace dirman;


string default_config_string = R"EOF(

# IMPORTANT: This test starts/finishes bootstrap multiple times. Lunasa's
# tcmalloc memory manager doesn't support this feature, so we have to
# switch to malloc instead. If your external CONFIG file sets these
# back to tcmalloc, you'll get an error

lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class OpDirManCreateTest : public testing::Test {
protected:
  virtual void SetUp(){
    faodel::Configuration config(default_config_string);
    config.AppendFromReferences();
    bootstrap::Start(config, dirman::bootstrap);

    //Create a default DI
    di = new DirectoryInfo("ref:<0x100>/a/b/c&info=This is the thing");
    di->Join(nodeid_t(200, iuo), "d");
    di->Join(nodeid_t(201, iuo), "e");
    di->Join(nodeid_t(202, iuo), "f");

    my_id = opbox::GetMyID();

  }
  virtual void TearDown(){
    if(di!=nullptr) delete di;
    bootstrap::FinishSoft();
  }
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bucket_t def_bucket;
  string   def_bucket_name;
  DirectoryInfo *di;
  bool ok;
  nodeid_t n;
  nodeid_t my_id;
};

TEST_F(OpDirManCreateTest, SimpleSerializeDirInfo){

  //Double check original is right
  EXPECT_EQ("This is the thing",di->info);
  EXPECT_EQ("/a/b", di->url.path);
  EXPECT_EQ("c", di->url.name);
  EXPECT_EQ(3, ((int)di->children.size()));

  ok = di->GetChildReferenceNode("d", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(200,iuo), n);
  ok = di->GetChildReferenceNode("e", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(201,iuo), n);
  ok = di->GetChildReferenceNode("f", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(202,iuo), n);

  lunasa::DataObject ldo;
  faodel::nodeid_t dst_node(1990,iuo);

  bool exceeds = msg_dirman::AllocateRequest( ldo,
                                              OpDirManCentralized::RequestType::HostNewDir,
                                              dst_node, 2001,
                                              *di);
  EXPECT_FALSE(exceeds);


  //Fake an incoming message
  OpArgs args(0, ldo.GetDataPtr<message_t *>());
  uint32_t ldo_len = ldo.GetDataSize();

  //Inspect the header and make sure it looks right
  auto args_msg = args.ExpectMessage<message_t *>();
  EXPECT_EQ( my_id,                         args_msg->src);
  EXPECT_EQ( dst_node,                      args_msg->dst);
  EXPECT_EQ( 2001ULL,                       args_msg->src_mailbox);
  EXPECT_EQ( MAILBOX_UNSPECIFIED,           args_msg->dst_mailbox);
  EXPECT_EQ( OpDirManCentralized::op_id,    args_msg->op_id);
  EXPECT_EQ( ldo_len-sizeof(message_t),     args_msg->body_len);
  EXPECT_EQ( static_cast<uint16_t>(OpDirManCentralized::RequestType::HostNewDir), args_msg->user_flags);


  //Unpack the message
  DirectoryInfo di2;
  di2 = msg_dirman::ExtractDirInfo(args_msg);

  EXPECT_EQ(*di, di2); //Should look like the original

  //Get rid of original
  delete di;  di=nullptr;

  EXPECT_EQ("This is the thing",di2.info);
  EXPECT_EQ("/a/b", di2.url.path);
  EXPECT_EQ("c", di2.url.name);
  EXPECT_EQ(3, ((int)di2.children.size()));

  ok = di2.GetChildReferenceNode("d", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(200,iuo), n);
  ok = di2.GetChildReferenceNode("e", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(201,iuo), n);
  ok = di2.GetChildReferenceNode("f", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(202,iuo), n);

}


TEST_F(OpDirManCreateTest, SimpleSerializeResourceURL){


  //Create a url we can ship
  ResourceURL url1("dht:[0x2112]<0x1234>/a/b/c&info=nacho_cheese");
  EXPECT_EQ("dht",                 url1.resource_type);
  EXPECT_EQ(nodeid_t(0x1234,iuo),  url1.reference_node);
  EXPECT_EQ(bucket_t(0x2112,iuo),  url1.bucket);
  EXPECT_EQ("/a/b",                url1.path);
  EXPECT_EQ("c",                   url1.name);
  EXPECT_EQ("info=nacho_cheese",   url1.options);


  //Pack a message
  lunasa::DataObject ldo;
  faodel::nodeid_t dst_node(1990,iuo);


  bool exceeds = msg_dirman::AllocateRequest( ldo,
                                              OpDirManCentralized::RequestType::GetInfo,
                                              dst_node, 2001,
                                              url1);
  EXPECT_FALSE(exceeds);


  //Fake an incoming message
  OpArgs args(0, ldo.GetDataPtr<message_t *>());
  uint32_t ldo_len = ldo.GetDataSize();
  auto args_msg = args.ExpectMessage<message_t *>();

  //Inspect the header and make sure it looks right
  EXPECT_EQ( my_id,                         args_msg->src);
  EXPECT_EQ( dst_node,                      args_msg->dst);
  EXPECT_EQ( 2001ULL,                       args_msg->src_mailbox);
  EXPECT_EQ( MAILBOX_UNSPECIFIED,           args_msg->dst_mailbox);
  EXPECT_EQ( OpDirManCentralized::op_id,    args_msg->op_id);
  EXPECT_EQ( ldo_len-sizeof(message_t),     args_msg->body_len);
  EXPECT_EQ( static_cast<uint16_t>(OpDirManCentralized::RequestType::GetInfo), args_msg->user_flags);


  //Unpack the message
  ResourceURL url2;
  url2 = msg_dirman::ExtractURL(args_msg);
  EXPECT_EQ(url1, url2); //Should look like the original


  EXPECT_EQ("dht",                 url2.resource_type);
  EXPECT_EQ(nodeid_t(0x1234,iuo),  url2.reference_node);
  EXPECT_EQ(bucket_t(0x2112,iuo),  url2.bucket);
  EXPECT_EQ("/a/b",                url2.path);
  EXPECT_EQ("c",                   url2.name);
  EXPECT_EQ("info=nacho_cheese",   url2.options);

}



//While we're not really using MPI, we need to do an MPI Init here so
//that we can startup/teardown multiple runs using bootstrap and nnti.
int main(int argc, char **argv){

  int rc=0;
  int mpi_rank;
  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  if(mpi_rank==0){
    rc = RUN_ALL_TESTS();
  }

  //One last start/finish, this time with a real teardown
  bootstrap::Start(Configuration(""), opbox::bootstrap);
  bootstrap::Finish(); //Deletes the bootstrap item with all the hooks
  MPI_Finalize();
  
  return rc;
}

