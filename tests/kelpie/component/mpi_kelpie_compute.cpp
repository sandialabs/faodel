// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_compute
//  Purpose: Set up a simple dht to verify Compute functions to remote nodes work


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/common/Helpers.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"
#include "whookie/Server.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;


//Note: Additional info will automatically be pulled from FAODEL_CONFIG
string default_config_string = R"EOF(

dirman.root_role rooter
dirman.type centralized

target.dirman.host_root

# MPI tests will need to have a standard networking base
kelpie.type standard

#bootstrap.debug true
#whookie.debug true
#opbox.debug true
#dirman.debug true
#kelpie.debug true

#kelpie.op.compute.debug true

)EOF";

using namespace kelpie;

class MPIComputeTest : public testing::Test {
protected:
  virtual void SetUp(){
    ResourceURL url("dht:/mydht");
    dht = kelpie::Connect(url);
  }

  void TearDown() override {}
  int rc;
  kelpie::Pool dht;
};

TEST_F(MPIComputeTest, Setup) {

  ResultCollector res(4);
  for(auto &name : {"a","b1","b2","c"}) {
    Key k1("Stuff",name);
    lunasa::DataObject ldo = lunasa::AllocateStringObject(k1.str());
    dht.Publish(k1, ldo, res);
  }
  res.Sync();

  lunasa::DataObject ldo;

  //Pick first item in list
  rc = dht.Compute(Key("Stuff","*"), "pick", "first", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s4 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|a", s4);

  //Pick the last item in the list
  rc = dht.Compute(Key("Stuff","*"), "pick", "last", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s5 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|c", s5);

  //Pick the largest object. b1 and b2 are the largest, should pick the first
  rc = dht.Compute(Key("Stuff","*"), "pick", "largest", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s6 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|b1", s6);

  //Pick the smallest object. There are multiple here (a and c). It should pick the first
  rc = dht.Compute(Key("Stuff","*"), "pick", "smallest", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s7 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|a", s7);

}

//Same as previous, but switch out to using a result collector
TEST_F(MPIComputeTest, Collector) {

  ResultCollector res(4);
  for(auto &name : {"a","b1","b2","c"}) {
    Key k1("Stuff",name);
    lunasa::DataObject ldo = lunasa::AllocateStringObject(k1.str());
    dht.Publish(k1, ldo, res);
  }
  res.Sync();

  lunasa::DataObject ldo;

  ResultCollector res2(4);
  rc = dht.Compute(Key("Stuff","*"), "pick", "first", res2); EXPECT_EQ(KELPIE_OK, rc);
  rc = dht.Compute(Key("Stuff","*"), "pick", "last",  res2); EXPECT_EQ(KELPIE_OK, rc);
  rc = dht.Compute(Key("Stuff","*"), "pick", "largest", res2); EXPECT_EQ(KELPIE_OK, rc);
  rc = dht.Compute(Key("Stuff","*"), "pick", "smallest", res2); EXPECT_EQ(KELPIE_OK, rc);

  res2.Sync();

  string s4 = lunasa::UnpackStringObject(res2.results[0].ldo); EXPECT_EQ("Stuff|a", s4); //First
  string s5 = lunasa::UnpackStringObject(res2.results[1].ldo); EXPECT_EQ("Stuff|c", s5); //Last
  string s6 = lunasa::UnpackStringObject(res2.results[2].ldo); EXPECT_EQ("Stuff|b1", s6); //Largest
  string s7 = lunasa::UnpackStringObject(res2.results[3].ldo); EXPECT_EQ("Stuff|a", s7); //Smallest

}




void targetLoop(){
  //G.dump();
}

int main(int argc, char **argv){

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  //Insert any Op registrations here

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config);


  if(G.mpi_rank==0){
    //Register the dht
    DirectoryInfo dir_info("dht:/mydht","This is My DHT");
    for(int i=1; i<G.mpi_size; i++){
      dir_info.Join(G.nodes[i]);
    }
    dirman::HostNewDir(dir_info);

    faodel::nodeid_t n2 = net::GetMyID();

    rc = RUN_ALL_TESTS();
    sleep(1);
  } else {
    targetLoop();
    sleep(1);
  }
  G.StopAll();
  return rc;
}
