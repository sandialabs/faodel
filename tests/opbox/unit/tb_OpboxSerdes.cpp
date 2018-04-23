// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <limits.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "common/SerializationHelpers.hh"

#include "webhook/Server.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

#include <stdio.h>


using namespace std;
using namespace faodel;
using namespace opbox;

bool enable_debug=false; //True turns on logging in doc

class OpBoxSerdesTest : public testing::Test {
protected:
  virtual void SetUp(){
    faodel::Configuration config;
    if(enable_debug){
      config.Append("bootstrap.debug","true");
      config.Append("webhook.debug", "true");
      config.Append("lunasa.debug", "true");
      config.Append("opbox.debug","true");
    }
    //Force this to an mpi implementation to make running easier
    config.Append("nnti.transport.name","mpi");
    //bootstrap::Start(config, opbox::bootstrap);



  }
  virtual void TearDown(){
    //bootstrap::FinishSoft();
  }
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bool ok;

};


TEST_F(OpBoxSerdesTest, Constants){

  //Make sure numbers are within reason
  EXPECT_GT(MAX_NET_BUFFER_REMOTE_SIZE, 0);
  EXPECT_LT(MAX_NET_BUFFER_REMOTE_SIZE, 100);

}


TEST_F(OpBoxSerdesTest, SimpleSerialize){

  //NetBufferRemote:
  opbox::net::NetBufferRemote nbr1;

  for(int i=0; i<MAX_NET_BUFFER_REMOTE_SIZE; i++)
    nbr1.data[i]=i;

  string s = faodel::BoostPack(nbr1);
  //cout <<"NetBufferRemote size: "<<s.size()<<endl;

  opbox::net::NetBufferRemote nbr2 = faodel::BoostUnpack<opbox::net::NetBufferRemote>(s);

  for(char i=0; i<MAX_NET_BUFFER_REMOTE_SIZE; i++){
    EXPECT_EQ(i, nbr2.data[i]);
  }

}

