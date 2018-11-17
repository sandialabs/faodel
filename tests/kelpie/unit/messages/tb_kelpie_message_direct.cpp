// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


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
  void SetUp() override {
    faodel::Configuration config;
    if(enable_debug){
      config.Append("bootstrap.debug","true");
      config.Append("webhook.debug", "true");
      config.Append("lunasa.debug", "true");
      config.Append("opbox.debug","true");
    }
    //Force this to an mpi implementation to make running easier
    config.Append("net.transport.name","mpi");
    bootstrap::Start(config, kelpie::bootstrap);
  }

  void TearDown() override {
    bootstrap::Finish();
  }
  faodel::internal_use_only_t iuo;


};

TEST_F(MsgDirectTest, SimplePub){

  bucket_t bucket(0x2112, iuo);
  Key k1("This is the row","This is the Column");

  lunasa::DataObject ldo_data(36,4000, lunasa::DataObject::AllocatorType::eager);
  lunasa::DataObject ldo_msg;
  bool exceeds = msg_direct_buffer_t::Alloc(ldo_msg, OpKelpiePublish::op_id, DirectFlags::CMD_PUBLISH, NODE_LOCALHOST,
                                            0x2064, opbox::MAILBOX_UNSPECIFIED, bucket, k1, 0x1971, PoolBehavior::TODO,
                                            &ldo_data);

  auto *msg = ldo_msg.GetDataPtr<msg_direct_buffer_t *>();
  //Fake an argument
  //OpArgs args(0, &msg->hdr);


  auto cmd = msg->GetCommand();
  EXPECT_EQ(static_cast<uint16_t>(DirectFlags::CMD_PUBLISH), cmd);

  Key k2= msg->ExtractKey();
  EXPECT_EQ(k1,k2);
  EXPECT_EQ("This is the row",    k2.K1());
  EXPECT_EQ("This is the Column", k2.K2());
  EXPECT_EQ(4036, msg->meta_plus_data_size); //36+4000
  EXPECT_EQ(15, msg->k1_size);
  EXPECT_EQ(18, msg->k2_size);
  EXPECT_EQ(0x2112, msg->bucket.bid);
  string s = string(msg->key_data, 15+18);
  EXPECT_EQ("This is the rowThis is the Column", s);
  EXPECT_EQ(0x1971, msg->iom_hash);
  EXPECT_EQ(0x00, msg->behavior_flags); //Currently TODO
  EXPECT_EQ(0x90, msg->hdr.user_flags); //update if op changes
  EXPECT_EQ(0x2064, msg->hdr.src_mailbox);

  cout <<"Ldo stuff "<<ldo_data.GetMetaSize()<<" "<<ldo_data.GetDataSize() << " "<<ldo_data.GetHeaderSize()<<" "<< ldo_data.GetRawAllocationSize()<<" "<<ldo_data.GetLocalHeaderSize()<<endl;

}
