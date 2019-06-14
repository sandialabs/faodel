// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// Purpose: Verify whookie works properly when sender/receiver shut down
//
// This test creates a client (rank 0) and multiple servers. It starts/stops
// the client/servers at different times to verify that requests complete
// properly when the server is up, and return an error code when they are down.
// This is also a sanity check that bootstrap starts/stops correctly.

#include <limits.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"

#include "whookie/client/Client.hh"
#include "whookie/Server.hh"

#include <stdio.h>
using namespace std;
using namespace faodel;

string default_config_string = R"EOF(

# Use your own $FAODEL_CONFIG file, or just set these manually
#bootstrap.debug              true
#bootstrap.status_on_shutdown true
#bootstrap.halt_on_shutdown   true

)EOF";


const int CMD_START=1;
const int CMD_FINI=2;
const int CMD_KILL=3;

string data_value="unset"; //The state variable that gets set by the "/getset_data" whookie

//Launch command to have servers start. We need to collect their ids
void startOthers(nodeid_t *nodes) {
  int cmd = CMD_START;
  MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);
  nodeid_t my_id = whookie::Server::GetNodeID();

  MPI_Allgather(&my_id, sizeof(faodel::nodeid_t), MPI_CHAR,
                nodes,  sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);
}



//Actual command to start server
void startSelf(nodeid_t *nodes){

  bootstrap::Start(faodel::Configuration(default_config_string), whookie::bootstrap);
  nodeid_t my_id = whookie::Server::GetNodeID();

  //Whookie: record value if passed. Return  new value
  whookie::Server::registerHook("/getset_data",
                                [] (const map<string,string> &args,
                                stringstream &results){
    auto new_val = args.find("newval");
    if(new_val != args.end()) {
      data_value = new_val->second;
    }
    results << "value=" << data_value << std::endl;
  });




  MPI_Allgather(&my_id, sizeof(faodel::nodeid_t), MPI_CHAR,
                nodes,  sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

}

//Notify servers that bootstrap should shutdown
void stopOthers() {
  int cmd = CMD_FINI;
  MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);
}

//Server nodes just sit in a loop and start/stop until told to shutdown
void ServerNodeLoop(int mpi_size) {
  int cmd;
  auto nodes = new faodel::nodeid_t[mpi_size];
  bool keep_going=true;
  do {
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0, MPI_COMM_WORLD);
    switch(cmd) {
      case CMD_START: startSelf(nodes); break;
      case CMD_FINI: bootstrap::Finish();  break;
      case CMD_KILL: keep_going = false;   break;
      default: cerr <<"Unknwon command?\n"; keep_going = false; break;
    }
  } while(keep_going);
  delete[] nodes;
}



class WhookieRestartTest : public testing::Test {
protected:
  virtual void SetUp() {
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    nodes = new nodeid_t[mpi_size];
    for(int i=0;i <mpi_size; i++){
      nodes[i]=faodel::NODE_UNSPECIFIED;
    }
  }
  virtual void TearDown(){
    stopOthers();
    bootstrap::Finish();
    delete[] nodes;
  }
  nodeid_t startHead() {
    bootstrap::Start(faodel::Configuration(default_config_string), whookie::bootstrap);
    return my_id = whookie::Server::GetNodeID();
  }
  void stopHead() {
    bootstrap::Finish();
  }


  int setRemoteValue(nodeid_t node, string val, string *result){
    return whookie::retrieveData(node, "/getset_data&newval="+val, result);
  }
  int getRemoteValue(nodeid_t node, string *result){
    return whookie::retrieveData(node, "/getset_data", result);
  }
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  int rc;
  bool ok;
  nodeid_t my_id;
  nodeid_t *nodes;
  int mpi_size;
};






TEST_F(WhookieRestartTest, NormalStartStop) {
  string val;

  startHead();
  startOthers(nodes);

  //Read back initial values
  for(int i=1; i<mpi_size; i++) {
    EXPECT_NE( NODE_UNSPECIFIED, nodes[i]);

    rc = getRemoteValue(nodes[i], &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value=unset\n", val);

    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  //Store first set of values
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(i);
    rc = setRemoteValue(nodes[i], tmp, &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);
    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  //Read first set of values
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(i);
    rc = getRemoteValue(nodes[i], &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);
    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  //All done. normal teardown w/ shutdown to others

}

//Write and read everything now that everyone has done a restart
TEST_F(WhookieRestartTest, AllRestart) {
  string val;

  startHead();
  startOthers(nodes);

  //Store initial values
  for(int i=1; i<mpi_size; i++) {
    EXPECT_NE( NODE_UNSPECIFIED, nodes[i]);
    string tmp = to_string(100+i);
    rc = setRemoteValue(nodes[i], tmp, &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);

    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  //Read first set of values
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(100+i);
    rc = getRemoteValue(nodes[i], &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);
    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  //All done. normal teardown w/ shutdown to others

}

//Restart ourselves. Verify we can still reach servers
TEST_F(WhookieRestartTest, SelfRestart) {
  string val;

  startHead();
  startOthers(nodes);

  //Store initial values
  for(int i=1; i<mpi_size; i++) {
    EXPECT_NE( NODE_UNSPECIFIED, nodes[i]);
    string tmp = to_string(200+i);
    rc = setRemoteValue(nodes[i], tmp, &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);

    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  stopHead();

  //Read first set of values
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(200+i);
    rc = getRemoteValue(nodes[i], &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);
    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  startHead(); //Just for shutdown

}

//Restart ourselves. Verify we can still reach servers
TEST_F(WhookieRestartTest, OthersRestart) {
  string val;

  startHead();
  startOthers(nodes);

  //Store initial values
  for(int i=1; i<mpi_size; i++) {
    EXPECT_NE( NODE_UNSPECIFIED, nodes[i]);
    string tmp = to_string(300+i);
    rc = setRemoteValue(nodes[i], tmp, &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);

    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

  stopOthers(); //shut servers down


  //Make sure we can't reach the servers. Sometimes it takes a while to
  //actually shut down, so if we get a response, sleep and try again.
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(300+i);

    rc=0;
    for(int retries=3; (rc!=-3)&&(retries>0); retries--) {
      rc = getRemoteValue(nodes[i], &val);
      //cout <<"Got rc "<<rc<<endl;
      if(rc != -3) sleep(2);
    }
    EXPECT_EQ(-3, rc);
    cout << i <<" Shutdown check should be -3. Got:  "<<rc<<endl;
  }

  startOthers(nodes); //Start everyone up again


  //Verify we can read everything back again
  for(int i=1; i<mpi_size; i++) {
    string tmp = to_string(300+i);
    rc = getRemoteValue(nodes[i], &val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ("value="+tmp+"\n", val);
    cout << i<<" "<<nodes[i].GetHttpLink()<<" "<<val; //includes eol
  }

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
    int cmd = CMD_KILL;
    MPI_Bcast(&cmd, sizeof(cmd), MPI_CHAR, 0 , MPI_COMM_WORLD);

  } else {
    ServerNodeLoop(mpi_size);
  }

  MPI_Finalize();
}