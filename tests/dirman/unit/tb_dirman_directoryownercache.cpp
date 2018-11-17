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
#include "opbox/services/dirman/common/DirectoryOwnerCache.hh"

#include <stdio.h>

using namespace std;
using namespace faodel;
using namespace opbox;

bool ENABLE_DEBUG_MESSAGES=false; //True turns on logging in doc

class DirectoryOwnerCacheTest : public testing::Test {
protected:
  virtual void SetUp(){
    def_bucket_name="mine";
    def_bucket = bucket_t(def_bucket_name);
    stringstream ss;
    ss<< "config.additional_files.env_name.if_defined   FAODEL_CONFIG\n";
    if(ENABLE_DEBUG_MESSAGES) {
      ss<< "directory.ownercache.debug true\n";
    }
    Configuration config = Configuration(ss.str());
    config.AppendFromReferences();
    doc.Init(config, "none","test");
  }
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bucket_t def_bucket;
  string   def_bucket_name;
  DirectoryOwnerCache doc;
};

TEST_F(DirectoryOwnerCacheTest, SimpleByHand){

  bool ok;
  ok=doc.Register( ResourceURL("ref:<0x2>[my_bucket]/a/b/c&a=1&b=2") );
  EXPECT_TRUE(ok);

  nodeid_t node(NODE_UNSPECIFIED);

  //Wrong bucket
  ok=doc.Lookup( ResourceURL("[NOT_my_bucket]/a/b/c"), &node);
  EXPECT_FALSE(ok);
  EXPECT_EQ(NODE_UNSPECIFIED, node);

  //Wrong path
  ok=doc.Lookup( ResourceURL("[my_bucket]/a/b"), &node);
  EXPECT_FALSE(ok);
  EXPECT_EQ(NODE_UNSPECIFIED, node);

  //Right bucket and path
  ok=doc.Lookup( ResourceURL("[my_bucket]/a/b/c"), &node);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0x2, node.nid);

  //Wrong bucket. Make sure node set to UNSPECIFIED
  ok=doc.Lookup( ResourceURL("[MY_BUCKET]/a/b/c"), &node);
  EXPECT_FALSE(ok);
  EXPECT_EQ(NODE_UNSPECIFIED, node);


}
TEST_F(DirectoryOwnerCacheTest, BucketSeparation){

  bool ok;
  nodeid_t node;

  ok=doc.Register( ResourceURL("ref:<0x2>[my_bucket]/a/b/c") );
  ok=doc.Register( ResourceURL("ref:<0x3>[MY_BUCKET]/a/b/c") );
  ok=doc.Register( ResourceURL("ref:<0x4>[0x2112]/a/b/c") );
  //Overwrite
  ok=doc.Register( ResourceURL("ref:<0x5>[my_bucket1]/a/b/c") );
  ok=doc.Register( ResourceURL("ref:<0x6>[my_bucket1]/a/b/c") );

  //Check for bucket separation
  ok=doc.Lookup( ResourceURL("[my_bucket]/a/b/c"), &node);
  EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x2,iuo), node);

  ok=doc.Lookup( ResourceURL("[MY_BUCKET]/a/b/c"), &node);
  EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x3,iuo), node);

  ok=doc.Lookup( ResourceURL("[0x2112]/a/b/c"), &node);
  EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x4,iuo), node);

  //Check for overwrite
  ok=doc.Lookup( ResourceURL("[my_bucket1]/a/b/c"), &node);
  EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x6,iuo), node);

}
