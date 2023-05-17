// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <algorithm>
#include "gtest/gtest.h"


#include "faodel-common/Common.hh"
#include "kelpie/Key.hh"

#include "kelpie/ops/direct/msg_direct.hh"
#include "kelpie/ops/direct/OpKelpiePublish.hh"


using namespace std;
using namespace faodel;
using namespace kelpie;

bool enable_debug=false; //True turns on logging in doc

class MsgDirectTest : public testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
  faodel::internal_use_only_t iuo;


};

#define byte_offset(a,b) (((uint64_t) &(a->b)) - ((uint64_t) a))

TEST_F(MsgDirectTest, Sizes) {
  bucket_t bucket(0x2112, iuo);
  Key k1("x","y");
  lunasa::DataObject ldo_data(36,4000, lunasa::DataObject::AllocatorType::eager);
  lunasa::DataObject ldo_msg_simple, ldo_msg_buffer;
  msg_direct_simple_t::Alloc(ldo_msg_simple, OpKelpiePublish::op_id, DirectFlags::CMD_PUBLISH, NODE_LOCALHOST,
                             0x2064, opbox::MAILBOX_UNSPECIFIED, bucket, k1, 0x1971, PoolBehavior::TODO,
                             "myfunction","myargs");

  bool exceeds = msg_direct_buffer_t::Alloc(ldo_msg_buffer, OpKelpiePublish::op_id, DirectFlags::CMD_PUBLISH, NODE_LOCALHOST,
                                            0x2064, opbox::MAILBOX_UNSPECIFIED, bucket, k1, 0x1971, PoolBehavior::TODO,
                                            &ldo_data);


  cout <<"Size of opbox message hdr : "<<sizeof(opbox::message_t)<<endl;
  cout <<"Size of netbuffer remote  : "<<sizeof(net::NetBufferRemote)<<endl;
  cout <<"Size of msg_direct_simple : "<<sizeof(msg_direct_simple_t)<<endl;
  cout <<"Size of msg_direct_buffer : "<<sizeof(msg_direct_buffer_t)<<endl;
  cout <<"Size of the SDB ldo       : "<<ldo_msg_simple.GetUserSize()<<endl;
  cout <<"Size of the SDB ldo's meta: "<<ldo_msg_simple.GetMetaSize()<<endl;
  cout <<"Size of the SDB ldo's data: "<<ldo_msg_simple.GetDataSize()<<endl;
  cout <<"Size of the MDB ldo       : "<<ldo_msg_buffer.GetUserSize()<<endl;
  cout <<"Size of the MDB ldo's meta: "<<ldo_msg_buffer.GetMetaSize()<<endl;
  cout <<"Size of the MDB ldo's data: "<<ldo_msg_buffer.GetDataSize()<<endl;
  msg_direct_buffer_t *x;

  cout <<"Offsets into MDB structure:\n";
  cout << "hdr:\t"<<byte_offset(x,hdr)<<endl;
  cout << "nbr:\t"<< byte_offset(x,net_buffer_remote)<<endl;
  cout << "klen1:\t"<< byte_offset(x,k1_size)<<endl;
  cout << "klen2:\t"<< byte_offset(x,k2_size)<<endl;
  cout << "bucket:\t"<< byte_offset(x,bucket)<<endl;
  cout << "iom:\t"<< byte_offset(x,iom_hash)<<endl;
  cout << "bflags:\t"<< byte_offset(x,behavior_flags)<<endl;
  cout << "strngs:\t"<< byte_offset(x,string_data[0])<<endl;

//  cout << "fname:\t"<< byte_offset(x,function_name_size)<<endl;
//  cout << "fargs:\t"<< byte_offset(x,function_args_size)<<endl;
}

TEST_F(MsgDirectTest, SimplePub) {

  bucket_t bucket(0x2112, iuo);
  Key k1("This is the row","This is the Column");

  lunasa::DataObject ldo_data(36,4000, lunasa::DataObject::AllocatorType::eager);
  lunasa::DataObject ldo_msg;
  bool exceeds = msg_direct_simple_t::Alloc(ldo_msg, OpKelpiePublish::op_id, DirectFlags::CMD_PUBLISH, NODE_LOCALHOST,
                                            0x2064, opbox::MAILBOX_UNSPECIFIED, bucket, k1, 0x1971, PoolBehavior::TODO,
                                            "cheese","burger");

  auto *msg = ldo_msg.GetDataPtr<msg_direct_simple_t *>();
  //Fake an argument
  //OpArgs args(0, &msg->hdr);


  auto cmd = msg->GetCommand();
  EXPECT_EQ(static_cast<uint16_t>(DirectFlags::CMD_PUBLISH), cmd);

  Key k3;
  string s1,s2;
  msg->ExtractComputeArgs(&k3, &s1,&s2);
  EXPECT_EQ(k1,k3);
  EXPECT_EQ("cheese", s1);
  EXPECT_EQ("burger", s2);
  EXPECT_EQ("This is the row",    k3.K1());
  EXPECT_EQ("This is the Column", k3.K2());
  EXPECT_EQ(0, msg->meta_plus_data_size); //36+4000
  EXPECT_EQ(15, msg->k1_size);
  EXPECT_EQ(18, msg->k2_size);
  EXPECT_EQ(0x2112, msg->bucket.bid);
  string s = string(msg->string_data, 15+18);
  EXPECT_EQ("This is the rowThis is the Column", s);
  EXPECT_EQ(0x1971, msg->iom_hash);
  EXPECT_EQ(0x00, msg->behavior_flags); //Currently TODO
  EXPECT_EQ(0x90, msg->hdr.user_flags); //update if op changes
  EXPECT_EQ(0x2064, msg->hdr.src_mailbox);

  cout <<"Ldo stuff "<<ldo_data.GetMetaSize()<<" "<<ldo_data.GetDataSize() << " "<<ldo_data.GetHeaderSize()<<" "<< ldo_data.GetRawAllocationSize()<<" "<<ldo_data.GetLocalHeaderSize()<<endl;

}

TEST_F(MsgDirectTest, BufferPub) {

  bucket_t bucket(0x2112, iuo);
  Key k1("This is the row","This is the Column");

  lunasa::DataObject ldo_data(36,4000, lunasa::DataObject::AllocatorType::eager);
  lunasa::DataObject ldo_msg;
  bool exceeds = msg_direct_buffer_t::Alloc(ldo_msg, OpKelpiePublish::op_id, DirectFlags::CMD_PUBLISH, NODE_LOCALHOST,
                                            0x2064, opbox::MAILBOX_UNSPECIFIED, bucket, k1, 0x1971, PoolBehavior::TODO,
                                            &ldo_data);

  auto *msg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();


  auto cmd = msg->GetCommand();
  EXPECT_EQ(static_cast<uint16_t>(DirectFlags::CMD_PUBLISH), cmd);

  Key k3;
  string s1,s2;
  k3 = msg->ExtractKey();
  EXPECT_EQ(k1,k3);
  EXPECT_EQ("This is the row",    k3.K1());
  EXPECT_EQ("This is the Column", k3.K2());
  EXPECT_EQ(4036, msg->meta_plus_data_size); //36+4000
  EXPECT_EQ(15, msg->k1_size);
  EXPECT_EQ(18, msg->k2_size);
  EXPECT_EQ(0x2112, msg->bucket.bid);
  string s = string(msg->string_data, 15+18);
  EXPECT_EQ("This is the rowThis is the Column", s);
  EXPECT_EQ(0x1971, msg->iom_hash);
  EXPECT_EQ(0x00, msg->behavior_flags); //Currently TODO
  EXPECT_EQ(0x90, msg->hdr.user_flags); //update if op changes
  EXPECT_EQ(0x2064, msg->hdr.src_mailbox);

  cout <<"Ldo stuff "<<ldo_data.GetMetaSize()<<" "<<ldo_data.GetDataSize() << " "<<ldo_data.GetHeaderSize()<<" "<< ldo_data.GetRawAllocationSize()<<" "<<ldo_data.GetLocalHeaderSize()<<endl;

}

int main(int argc, char **argv){
  int rc=0;

  ::testing::InitGoogleTest(&argc, argv);

  Configuration config;
  config.Append("dirman.type none");
  if(enable_debug){
    config.Append("bootstrap.debug","true");
    config.Append("whookie.debug", "true");
    config.Append("lunasa.debug", "true");
    config.Append("opbox.debug","true");
  }
  //Force this to an mpi implementation to make running easier
  //config.Append("net.transport.name","mpi");
  bootstrap::Start(config, kelpie::bootstrap);

  rc = RUN_ALL_TESTS();

  bootstrap::Finish();

  return rc;
}

