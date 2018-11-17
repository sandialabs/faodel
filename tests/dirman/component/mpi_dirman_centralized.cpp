// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_dirman_centralized
//  Purpose: Test our ability to use central store for resource info


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-services/MPISyncStart.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"

//#include "../../opbox/mpi/support/Globals.hh"

using namespace std;
using namespace faodel;




string default_config_string = R"EOF(

# Use mpi sync start to make it easier to plug in info
mpisyncstart.enable true

# Set last node in mpi job to be dirman
dirman.root_node_mpi LAST
dirman.type centralized

# Plug in some static resources that mpisyncstart can resolve at boot
dirman.resources_mpi[] dht:/static/all&info="EVERYONE" ALL
dirman.resources_mpi[] dht:/static/node0&info="Node0"  0
dirman.resources_mpi[] dht:/static/root_node&info="RootNode"  LAST



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
  nodeid_t root_node;
  DirectoryInfo dir_info;

  //Lookup the root node. We had mpisyncstart figure this out in configuration
  ok = dirman::GetDirectoryInfo(ResourceURL("ref:/static/root_node"), &dir_info); EXPECT_TRUE(ok);
  EXPECT_EQ(1, dir_info.children.size());
  if(dir_info.children.size()==1) {
    root_node = dir_info.children.at(0).node;
    //cout <<"------>Root node is "<<dir_info.children.at(0).node.GetHex()<<endl;
  }

  //Centralized should always point to root node
  ok = dirman::Locate(ResourceURL("ref:/something/that/is/missing"), &ref_node);   EXPECT_TRUE(ok); EXPECT_EQ(root_node, ref_node);
  ok = dirman::Locate(ResourceURL("ref:/nothing"), &ref_node);                     EXPECT_TRUE(ok); EXPECT_EQ(root_node, ref_node);

  //For network testing, we'd better not be the root
  EXPECT_NE(myid, ref_node);



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

  int rc = 0;
  int mpi_rank;
  ::testing::InitGoogleTest(&argc, argv);

  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  mpisyncstart::bootstrap();

  //Set the configuration for the two types of nodes (tester and target)
  bootstrap::Start(faodel::Configuration(default_config_string), dirman::bootstrap);



  //Split the work into two sections: the tester (node 0) and the targets
  if(mpi_rank==0){
    //cout << "Tester begins.\n";
    rc = RUN_ALL_TESTS();
    //sleep(1);
  } else {
    targetLoop();
    //sleep(1);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  //One last start/finish, this time with a real teardown
  //bootstrap::Start(Configuration(""), opbox::bootstrap);
  bootstrap::Finish(); //Deletes the bootstrap item with all the hooks
  MPI_Finalize();


  return rc;
}
