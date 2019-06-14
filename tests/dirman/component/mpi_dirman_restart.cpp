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
#include "whookie/client/Client.hh"
#include "lunasa/Lunasa.hh"
#include "faodel-services/MPISyncStart.hh"

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

mpisyncstart.enable true

dirman.type centralized
dirman.root_node_mpi 1

dirman.resources_mpi[] dht:/my/thing1&info=first ALL
dirman.resources_mpi[] dht:/my/thing2&info=second ALL

bootstrap.debug         true
mpisyncstart.debug      true
dirman.debug            true
dirman.cache.mine.debug true
dirman.cache.others     true
dirman.cache.owners     true

bootstrap.status_on_shutdown true
#bootstrap.halt_on_shutdown true

)EOF";

int cmd;
const int CMD_START=1;
const int CMD_FINI=2;
const int CMD_KILL=3;

class OpDirManRestartTest : public testing::Test {
protected:
  virtual void SetUp(){

    cmd = CMD_START;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);

    faodel::Configuration config(default_config_string);
    mpisyncstart::bootstrap();
    bootstrap::Start(config, dirman::bootstrap);
    MPI_Barrier(MPI_COMM_WORLD);

    my_id = opbox::GetMyID();

  }
  virtual void TearDown(){
    cmd = CMD_FINI;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);
    bootstrap::Finish();
  }
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  int rc;
  bool ok;
  nodeid_t n;
  nodeid_t my_id;
};

TEST_F(OpDirManRestartTest, GetStatic1) {

  DirectoryInfo dir1, dir2;

  ok = dirman::GetDirectoryInfo(ResourceURL("ref:/my/thing1"), &dir1);  EXPECT_EQ(true, ok);
  ok = dirman::GetDirectoryInfo(ResourceURL("ref:/my/thing2"), &dir2);  EXPECT_EQ(true, ok);

  cout <<"dir1 is : "<<dir1.str()<<endl<<"dir2 is : "<<dir2.str()<<endl;

  EXPECT_EQ(2, dir1.members.size());
  EXPECT_EQ(2, dir2.members.size());

}

TEST_F(OpDirManRestartTest, GetStatic2) {

  string result;
  nodeid_t root = dirman::GetAuthorityNode();
  whookie::retrieveData(root, "/dirman/entry&name=[0xadd7ee83]/my/thing2&format=txt", &result);
  cout <<"Result is "<<result<<endl;

//  while(1){}
  DirectoryInfo dir1, dir2;

  ok = dirman::GetDirectoryInfo(ResourceURL("ref:/my/thing2"), &dir2);  EXPECT_EQ(true, ok);
  ok = dirman::GetDirectoryInfo(ResourceURL("ref:/my/thing1"), &dir1);  EXPECT_EQ(true, ok);

  cout <<"dir1 is : "<<dir1.str()<<endl<<"dir2 is : "<<dir2.str()<<endl;

  EXPECT_EQ(2, dir1.members.size());
  EXPECT_EQ(2, dir2.members.size());

}



//Test node just sits in a loop and starts/stops until told to shutdown
void testNodeLoop() {

  bool keep_going=true;
  do {
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);
    switch(cmd) {
      case CMD_START:
        mpisyncstart::bootstrap();
        bootstrap::Start(Configuration(default_config_string), dirman::bootstrap);
        MPI_Barrier(MPI_COMM_WORLD);
        break;
      case CMD_FINI: bootstrap::Finish();  break;
      case CMD_KILL: keep_going = false;   break;
      default: cerr <<"Unknwon command?\n"; keep_going = false; break;
    }

  } while(keep_going);
}

int main(int argc, char **argv) {

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size, provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  assert(mpi_size>1);

  if(mpi_rank==0) {
    rc = RUN_ALL_TESTS();
    cmd = CMD_KILL;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0 , MPI_COMM_WORLD);

  } else {
    testNodeLoop();
  }

  MPI_Finalize();
}
