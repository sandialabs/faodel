// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// This test verifies that behaviors are working right. It spins off a single dht
// node on either the
#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "faodel-services/MPISyncStart.hh"
#include "kelpie/Kelpie.hh"

#include "support/ExperimentLauncher.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

const int CMD_DUMP_RESOURCES = 1;
const int CMD_WRITE_PARTICLES = 2;
const int CMD_CHECK_PARTICLES = 3; //Check a list of keys to see if they match

bool ENABLE_DEBUG=false;


const int PARTICLE_BLOB_BYTES = 1024;


string default_config_string = R"EOF(

# Multiple runs need to be done with malloc
lunasa.lazy_memory_manager  malloc
lunasa.eager_memory_manager malloc

# Enable all debug by labeling this node's role as debug_node
debug_node.mpisyncstart.debug      true
debug_node.bootstrap.debug         true
debug_node.whookie.debug           true
debug_node.opbox.debug             true
debug_node.dirman.debug            true
debug_node.dirman.cache.mine.debug true
debug_node.dirman.cache.others     true
debug_node.dirman.cache.owners     true
debug_node.kelpie.debug            true
debug_node.kelpie.pool.debug       true
debug_node.lunasa.debug            true
debug_node.lunasa.allocator.debug  true


#bootstrap.status_on_shutdown true
#bootstrap.halt_on_shutdown true

bootstrap.sleep_seconds_before_shutdown 0

# All iom work is PIO and goes to faodel_data
default.kelpie.iom.type    PosixIndividualObjects
default.kelpie.iom.path    ./faodel_data

## All Tests must define any additional settings in this order:
##   mpisyncstart.enable  -- if mpi is filling in any info
##   default.kelpie.ioms  -- list of ioms everyone should have
##   (kelpie.iom.iomname.path)   -- a path for each iom's path, if not default
##   dirman.type          -- centralized or static
##   dirman.root_node     -- root id if you're centralized
##   dirman.resources     -- lists of all the dirman entries to use


)EOF";



class BehaviorsTest : public testing::Test {
protected:
  void SetUp() override {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if(mpi_size!=2) {
      cerr <<"This test must be run with exactly two ranks.\n";
      exit(-1);
    }
    //Enable debug mode
    s_config  = (!ENABLE_DEBUG)  ? default_config_string : default_config_string + "node_role debug_node\n";

    char p1[] = "/tmp/gtestXXXXXX";
    string path1 = mkdtemp(p1);

    s_config += "\nkelpie.iom.myiom.path " + path1;
  }

  void TearDown() override {
    //cout <<"Server teardown\n";
    el_bcastCommand(CMD_TEARDOWN);
    bootstrap::Finish();
    MPI_Barrier(MPI_COMM_WORLD);
  }

  string s_config;

  int mpi_rank, mpi_size;
  int rc;
};

TEST_F(BehaviorsTest, WriteRemote) {

  s_config += + R"EOF(
mpisyncstart.enable     true
default.kelpie.ioms     my_iom
dirman.type             centralized
dirman.root_node_mpi    0
dirman.resources_mpi[]  dht:/mydht&iom=my_iom   1

)EOF";

  //Share our config and start
  el_bcastConfig(CMD_NEW_KELPIE_START, s_config);
  mpisyncstart::bootstrap();
  bootstrap::Start(Configuration(s_config), kelpie::bootstrap);

  //The tester is also the root. It should find the pool and detect that myiom1 hash is associated with it
  auto pool = kelpie::Connect("ref:/mydht");
  EXPECT_EQ(faodel::hash32("my_iom"), pool.GetIomHash());
  EXPECT_EQ(kelpie::pool_behavior_t(kelpie::PoolBehavior::DefaultRemoteIOM), pool.GetBehavior());

  kelpie::object_info_t info;
  lunasa::DataObject ldo(1024);
  rc = pool.Publish(kelpie::Key("write-remote"), ldo, &info);
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  cout <<"row info: "<< info.str()  << endl;




}





int main(int argc, char **argv){

  return el_defaultMain(argc, argv);
}