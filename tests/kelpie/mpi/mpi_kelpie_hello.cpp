// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_kelpie_hello
//  Purpose: This is just a hello example to demo how we can launch a few nodes
//           with mpi and do a gtest from node 0 that just pings the other
//           nodes. This examples starts up the bootstrap services, but does
//           not use them for anything. This is just a sanity check to make
//           sure mpi apps can still work with all of our code.



#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "kelpie/Kelpie.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;


//Globals holds mpi info and manages connections (see ping example for info)
Globals G;


string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server
# default to using mpi, but allow override in config file pointed to by FAODEL_CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

tester.webhook.port 1991
rooter.webhook.port 1992
server.webhook.port 2000


dirman.root_role rooter

target.dirman.host_root
target.dirman.write_to_file ./dirman.txt

dirman.type centralized

# MPI tests will need to have a standard networking base
kelpie.type standard

#bootstrap.debug true
#webhook.debug true
opbox.debug true
#dirman.debug true
#kelpie.debug true

)EOF";


class MPIHelloTest : public testing::Test {
protected:
  virtual void SetUp(){}
  virtual void TearDown(){}
};

TEST_F(MPIHelloTest, SimplePing){

}

void targetLoop(){
  G.dump();
}

int main(int argc, char **argv){

  ::testing::InitGoogleTest(&argc, argv);

  //Insert any Op registrations here

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config);


  if(G.mpi_rank==0){
    cout << "Tester begins.\n";
    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";
    sleep(1);
  } else {
    cout <<"Target Running\n";
    targetLoop();
    sleep(1);
  }

  G.StopAll();
}
