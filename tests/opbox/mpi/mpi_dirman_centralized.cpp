// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_dirman_centralized
//  Purpose: Test our ability to use central store for


#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "opbox/OpBox.hh"

#include "opbox/ops/OpCount.hh"
#include "opbox/ops/OpPing.hh"
#include "opbox/services/dirman/DirectoryManager.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;


string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

tester.webhook.port 1991
rooter.webhook.port 1992
server.webhook.port 2000

dirman.root_role rooter
dirman.type centralized

#bootstrap.debug true
#webhook.debug true
#opbox.debug true
#dirman.debug true

)EOF";



class DirManCentralized : public testing::Test {
protected:
  virtual void SetUp(){  }
  virtual void TearDown(){  }
};

TEST_F(DirManCentralized, Simple){

  using namespace opbox;

  bool ok;
  nodeid_t myid = GetMyID();
  nodeid_t ref_node;

  //Centralized should always point to root node
  ok = dirman::Locate(ResourceURL("ref:/something/that/is/missing"), &ref_node);   EXPECT_TRUE(ok); EXPECT_EQ(G.dirman_root_nodeid, ref_node);
  ok = dirman::Locate(ResourceURL("ref:/nothing"), &ref_node);                     EXPECT_TRUE(ok); EXPECT_EQ(G.dirman_root_nodeid, ref_node);

  //For network testing, we'd better not be the root
  EXPECT_NE(myid, ref_node);



  //Check locally
  DirectoryInfo dir_info;

  //Search for a missing entry locally and remotely
  ok = dirman::GetLocalDirectoryInfo(ResourceURL("ref:/not/my/problem"), &dir_info); EXPECT_FALSE(ok);



  //Create a new, empty directory
  ok = dirman::HostNewDir(DirectoryInfo("/this/is/valid&info=MegaThing")); EXPECT_TRUE(ok);
  ok = dirman::GetLocalDirectoryInfo(ResourceURL("/this/is/valid"), &dir_info); EXPECT_TRUE(ok); //Fails if not local only
  EXPECT_EQ("MegaThing",dir_info.info);
  EXPECT_EQ(0, dir_info.children.size());

  //Create a brother
  ok = dirman::HostNewDir(DirectoryInfo("/this/is/nothing&info=MiniThing")); EXPECT_TRUE(ok);
  ok = dirman::GetLocalDirectoryInfo(ResourceURL("/this/is/nothing"), &dir_info); EXPECT_TRUE(ok); //Fails if not local only
  EXPECT_EQ("MiniThing",dir_info.info);
  EXPECT_EQ(0, dir_info.children.size());

  //Fetch the parent
  ok = dirman::GetDirectoryInfo(ResourceURL("/this/is"), &dir_info); EXPECT_TRUE(ok); //Fails if not local only
  EXPECT_EQ(2, dir_info.children.size());
  //cout <<"Parent is "<<dir_info.str()<<endl;







}

void targetLoop(){
  //G.dump();
}



int main(int argc, char **argv){


  ::testing::InitGoogleTest(&argc, argv);


  //Set the configuration for the two types of nodes (tester and target)
  opbox::RegisterOp<OpPing>();
  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config);


  //Split the work into two sections: the tester (node 0) and the targets
  if(G.mpi_rank==0){
    //cout << "Tester begins.\n";
    int rc = RUN_ALL_TESTS();
    //cout << "Tester completed all tests.\n";
    sleep(1);
  } else {
    //cout <<"Target pausing\n";
    targetLoop();
    sleep(1);
  }

  G.StopAll();

}
