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
#include "opbox/services/dirman/core/DirManCoreCentralized.hh"

#include <stdio.h>

using namespace std;
using namespace faodel;
using namespace opbox;
using namespace opbox::dirman;
using namespace opbox::dirman::internal;



bool ENABLE_DEBUG_MESSAGES=false;




class DirManCoreCentralizedTest : public testing::Test {


protected:

  virtual void SetUp(){
    def_bucket_name="mine";
    def_bucket = bucket_t(def_bucket_name);

    stringstream ss;
    ss<<   "config.additional_files.env_name.if_defined   FAODEL_CONFIG\n"
      <<   "dirman.testing_mode.am_root  true\n"      
      <<   "security_bucket              "<<def_bucket_name<<"\n";

    if(ENABLE_DEBUG_MESSAGES) {
      ss<< "dirman.debug                 true\n"
        << "dirman.cache.mine.debug      true\n"
        << "dirman.cache.others.debug    true\n";
    }
    config = Configuration(ss.str());
    config.AppendFromReferences();
    dmcc = new DirManCoreCentralized(config);
  }

  virtual void TearDown(){
    delete dmcc;
  }
  DirManCoreCentralized *dmcc;
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bucket_t def_bucket;
  string   def_bucket_name;
  Configuration config;
};


TEST_F(DirManCoreCentralizedTest, Simple){

  bool ok;
  nodeid_t root_node = dmcc->GetRootNode();
  nodeid_t ref_node, tmp_node;
  ResourceURL url("/things/a");


  //Double check the config and make sure the bucket was set right. The
  //dmcc functions will fill in the default bucket from config if unspecified
  bucket_t tmp_bucket; string tmp_bname;
  config.GetDefaultSecurityBucket(&tmp_bname);
  config.GetDefaultSecurityBucket(&tmp_bucket);
  EXPECT_EQ(def_bucket_name, tmp_bname);
  EXPECT_EQ(def_bucket, tmp_bucket);

  //See if this thing exists yet
  ok = dmcc->Locate(url, &tmp_node);                      EXPECT_TRUE(ok);
  ok = dmcc->HostNewDir(DirectoryInfo("/things/a"));      EXPECT_TRUE(ok);
  ok = dmcc->Locate(ResourceURL("/things/a"), &ref_node); EXPECT_TRUE(ok);
  EXPECT_EQ(root_node, tmp_node);
  EXPECT_EQ(root_node, ref_node);

  //Get info. Should be empty since nobody has joined yet. Host should be root.
  DirectoryInfo di;
  ok = dmcc->GetDirectoryInfo(url, true, false, &di);
  EXPECT_TRUE(ok);
  EXPECT_EQ(def_bucket,      di.url.bucket);
  EXPECT_EQ(root_node,       di.url.reference_node);
  EXPECT_EQ("/things/a",     di.url.GetPathName());
  EXPECT_EQ("",              di.info);
  EXPECT_EQ(0,               di.children.size());


  //Register three named children to it
  DirectoryInfo di2;
  ok = dmcc->JoinDirWithName(ResourceURL("/things/a"), "b", &di2); EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithName(ResourceURL("<0x99>/things/a"), "c"); EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithName(ResourceURL("<0x88>/things/a"), "d", &di2); EXPECT_TRUE(ok);

  //Make sure all three are there
  nodeid_t n;
  ok = di2.GetChildReferenceNode("b", &n); EXPECT_TRUE(ok); EXPECT_EQ(root_node, n);
  ok = di2.GetChildReferenceNode("c", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x99,iuo), n);
  ok = di2.GetChildReferenceNode("d", &n); EXPECT_TRUE(ok); EXPECT_EQ(nodeid_t(0x88,iuo), n);
  ok = di2.GetChildReferenceNode("x", &n); EXPECT_FALSE(ok);
  EXPECT_EQ(3, di2.children.size());

  //Remove some of the children
  DirectoryInfo di3;
  ok = dmcc->LeaveDir(ResourceURL("/things/a/c"), &di3); EXPECT_TRUE(ok);  EXPECT_EQ(2, di3.children.size());
       ok = di3.GetChildReferenceNode("b"); EXPECT_TRUE(ok);
       ok = di3.GetChildReferenceNode("c"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("d"); EXPECT_TRUE(ok);


  ok = dmcc->LeaveDir(ResourceURL("/things/a/X"), &di3); //Fake leave
       EXPECT_FALSE(ok);
       EXPECT_EQ(2, di3.children.size());
       ok = di3.GetChildReferenceNode("b"); EXPECT_TRUE(ok);
       ok = di3.GetChildReferenceNode("c"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("d"); EXPECT_TRUE(ok);

  ok = dmcc->LeaveDir(ResourceURL("/things/a/b"), &di3);
       EXPECT_TRUE(ok);
       EXPECT_EQ(1, di3.children.size());
       ok = di3.GetChildReferenceNode("b"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("c"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("d"); EXPECT_TRUE(ok);

  ok = dmcc->LeaveDir(ResourceURL("/things/a/d"), &di3);
       EXPECT_TRUE(ok);
       EXPECT_EQ(0, di3.children.size());
       ok = di3.GetChildReferenceNode("b"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("c"); EXPECT_FALSE(ok);
       ok = di3.GetChildReferenceNode("d"); EXPECT_FALSE(ok);

}


TEST_F(DirManCoreCentralizedTest, JoinNoName){

  bool ok;
  nodeid_t root_node = dmcc->GetRootNode();
  nodeid_t ref_node, tmp_node;
  ResourceURL url("/things/a");
  DirectoryInfo di2;

  //See if this thing exists yet
  ok = dmcc->Locate(url, &tmp_node);                      EXPECT_TRUE(ok);
  ok = dmcc->HostNewDir(DirectoryInfo("/things/a"));      EXPECT_TRUE(ok);
  ok = dmcc->HostNewDir(DirectoryInfo("/things/b"));      EXPECT_TRUE(ok);

  ok = dmcc->Locate(ResourceURL("/things/a"), &ref_node); EXPECT_TRUE(ok);
  EXPECT_EQ(root_node, tmp_node);
  EXPECT_EQ(root_node, ref_node);

  //Try joining without naming ourselves
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x90>/things/a"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x91>/things/a"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x92>/things/a"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x93>/things/a"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x94>/things/a"), &di2); EXPECT_TRUE(ok);
  EXPECT_EQ(5, di2.children.size());

  //cout <<di2.to_string()<<endl;
  string child_name="dummy";

  for(int i=0x90; i<0x95; i++){
    ok = di2.GetChildNameByReferenceNode(nodeid_t(i,iuo), &child_name);
    EXPECT_TRUE(ok); EXPECT_NE("", child_name);
    //cout <<"Node "<<i<<" named: "<<child_name<<endl;
  }

  ok = di2.GetChildNameByReferenceNode(nodeid_t(0x99,iuo), &child_name);
  EXPECT_FALSE(ok); EXPECT_EQ("", child_name);

  //Try with some naming conflict
  ok = dmcc->JoinDirWithName(ResourceURL("<0x90>/things/b"),"AG1");  EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x91>/things/b"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x92>/things/b"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x93>/things/b"));       EXPECT_TRUE(ok);
  ok = dmcc->JoinDirWithoutName(ResourceURL("<0x94>/things/b"), &di2); EXPECT_TRUE(ok);
  EXPECT_EQ(5, di2.children.size());

  //cout <<di2.to_string()<<endl;
  child_name="dummy";

  for(int i=0x90; i<0x95; i++){
    ok = di2.GetChildNameByReferenceNode(nodeid_t(i,iuo), &child_name);
    EXPECT_TRUE(ok); EXPECT_NE("", child_name);
    //cout <<"Node "<<i<<" named: "<<child_name<<endl;
  }

  ok = di2.GetChildNameByReferenceNode(nodeid_t(0x99,iuo), &child_name);
  EXPECT_FALSE(ok); EXPECT_EQ("", child_name);


}



TEST_F(DirManCoreCentralizedTest, SimpleTree){

  bool ok;
  nodeid_t root_node = dmcc->GetRootNode();
  nodeid_t ref_node, tmp_node;

  vector<string> hosts {
      "/my",
      "/my/first",
      "/my/first/tree",
      "/my/second",
      "/my/second/tree"
  };

  //TODO
}
