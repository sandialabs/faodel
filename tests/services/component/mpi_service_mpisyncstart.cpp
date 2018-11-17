// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


// This test tests mpisync start to see if multiple nodes
// will fire up, resolve their ids, and update their
// configs.
//
// In each test, rank 0 sends off commands to all other nodes
// telling them what they're supposed to do next.

#include <mpi.h>

#include <stdexcept>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-common/BootstrapInterface.hh"
#include "faodel-common/DirectoryInfo.hh"
#include "faodel-services/MPISyncStart.hh"
#include "webhook/Server.hh"


using namespace std;
using namespace faodel;




const int CMD_NEW_WEBHOOK_START =  1; //Here's a config, launch with plain webhook
const int CMD_NEW_MPISYNC_START =  2; //Here's a config, launch with webhook+mpisync
const int CMD_TEARDOWN          =  3;
const int CMD_KILL              = -1;

typedef struct {
  int command;
  int message_length;
  char message[4*1024];
} test_command_t;

void test_bcastConfig(int cmd, string s){
  test_command_t msg;
  assert(s.size()<sizeof(msg.message));
  msg.command = cmd;
  msg.message_length = s.size();
  memcpy(&msg.message[0], s.c_str(), s.size());
  MPI_Bcast(&msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);
}
void test_bcastCommand(int cmd){
  test_command_t msg;
  msg.command = cmd;
  MPI_Bcast(&msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);
}

//Debug class: Lets us get a copy of the config that is being passed on to everyone
class GetConfig
        : public faodel::bootstrap::BootstrapInterface,
          public faodel::LoggingInterface {
public:
  GetConfig() :LoggingInterface("getconfig") {}
  void Init(const faodel::Configuration &config) override {
    ConfigureLogging(config);
    myconfig = config;
  }
  void Start() override {}
  void Finish() override {}
  void GetBootstrapDependencies(std::string &name,
                                std::vector<std::string> &requires,
                                std::vector<std::string> &optional) const override {
    name = "getconfig";
    requires = {"mpisyncstart"};
  }
  faodel::Configuration myconfig;
};

namespace test_mpisync {

static GetConfig get_config; //For capturing configured info

std::string bootstrap() {

  mpisyncstart::bootstrap();
  faodel::bootstrap::RegisterComponent(&get_config, true);
  return "getconfig";
}
}




class BootstrapMPITest : public testing::Test {
protected:
  void SetUp() override {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  }

  void TearDown() override {
    //cout <<"Server teardown\n";
    test_bcastCommand(CMD_TEARDOWN);
    bootstrap::Finish();
    MPI_Barrier(MPI_COMM_WORLD);
  }
  int mpi_rank, mpi_size;
};

TEST_F(BootstrapMPITest, NoConfig){
  //cout <<"Starting from "<<mpi_rank<<" of "<<mpi_size<<endl;
  string c1 = R"EOF(
dirman.type centralized
#bootstrap.debug true
#webhook.debug true
)EOF";

  test_bcastConfig(CMD_NEW_WEBHOOK_START,c1);
  bootstrap::Start(Configuration(c1), webhook::bootstrap);

}
TEST_F(BootstrapMPITest, Config1){
  //cout <<"Starting from "<<mpi_rank<<" of "<<mpi_size<<endl;
  string c1 = R"EOF(
dirman_root_mpi 0
dirman.type centralized
#bootstrap.debug true
#webhook.debug true
)EOF";

  test_bcastConfig(CMD_NEW_WEBHOOK_START, c1);
  bootstrap::Start(Configuration(c1), webhook::bootstrap);

}


TEST_F(BootstrapMPITest, MPISyncStart) {
  string c1 = R"EOF(
mpisyncstart.enable  true

#bootstrap.debug      true
#webhook.debug        true
#mpisyncstart.debug   true
)EOF";

  test_bcastConfig(CMD_NEW_MPISYNC_START, c1);
  bootstrap::Start(Configuration(c1), mpisyncstart::bootstrap);

}


//This one submits static resources, and expects mpisyncstart to resolve them
TEST_F(BootstrapMPITest, MPISyncStartMPI) {
  string c1 = R"EOF(


mpisyncstart.enable true

dirman.root_node_mpi 0
dirman.resources_mpi[] dht:/my/all&info="booya"   ALL
dirman.resources_mpi[] dht:/my/single&info="single" 0
dirman.resources_mpi[] dht:/my/double&info="single" 0-middle

)EOF";

  test_bcastConfig(CMD_NEW_MPISYNC_START, c1);
  bootstrap::Start(Configuration(c1), test_mpisync::bootstrap /*mpisyncstart::bootstrap*/);

  Configuration c = test_mpisync::get_config.myconfig;
  int num1, num2;
  vector<string> urls_orig, urls_mod;
  num1 = c.GetStringVector(&urls_orig, "dirman.resources_mpi");
  num2 = c.GetStringVector(&urls_mod, "dirman.resources");
  EXPECT_EQ(num1, num2);
  EXPECT_EQ(3, num2);
  if (num2 <= 3) {
    ResourceURL urls[3];
    DirectoryInfo dir_info[3];
    for (int i = 0; i < 3; i++) {
      urls[i] = ResourceURL(urls_mod[i]);
      dir_info[i] = DirectoryInfo(urls[i]);
      EXPECT_EQ("dht", urls[i].resource_type);
      EXPECT_EQ("/my", urls[i].path);
    }
    EXPECT_EQ("all", urls[0].name);
    EXPECT_EQ("single", urls[1].name);
    EXPECT_EQ("double", urls[2].name);
  }
}


// All non-zero ranks just run in this loop, waiting for orders
// that tell them what to do new next. The commands are:
//
//   NEW_WEBHOOK_START: Here's a config, start without mpisync
//   NEW_MPISYNC_START: Here's a config, start with mpisync
//   TEARDOWN: End of the config,  finish the bootstrap
//   KILL: all tests are done, exit out of targetloop
//
void targetLoop(){
  test_command_t msg;
  bool keep_going=true;
  do {
    MPI_Bcast( &msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);
    switch(msg.command) {
      case CMD_NEW_WEBHOOK_START: {
        string cstr = string(msg.message, msg.message_length);
        bootstrap::Start(Configuration(cstr), webhook::bootstrap);
        break;
      }
      case CMD_NEW_MPISYNC_START: {
        string cstr = string(msg.message, msg.message_length);
        //bootstrap::Start(Configuration(cstr), mpisyncstart::bootstrap);
        bootstrap::Start(Configuration(cstr), test_mpisync::bootstrap);
        break;
      }
      case CMD_TEARDOWN: {
        //cout <<"Client Teardown\n";
        bootstrap::Finish();
        MPI_Barrier(MPI_COMM_WORLD);
        break;
      }
      case CMD_KILL:
        //cout <<"Kill\n";
        keep_going=false;
        break;
      default:
        throw std::runtime_error("Unknown target loop command? id:" + to_string(msg.command));
    }

  } while(keep_going);
}

int main(int argc, char **argv){

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if(mpi_rank==0){
    cout << "Tester begins.\n";
    rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";
    test_bcastCommand(CMD_KILL);
    sleep(1);
  } else {
    //cout <<"Target Running\n";
    targetLoop();
    sleep(1);
  }

  MPI_Finalize();
  if(mpi_rank==0) cout <<"All complete. Exiting\n";
  return rc;
}
