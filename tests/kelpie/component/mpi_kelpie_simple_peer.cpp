// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_simple_peer
//  Purpose: This is set of simple tests to see if a pair of nodes can
//           communicate properly



#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
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

)EOF";


class MPISimplePeerTest : public testing::Test {
protected:
  virtual void SetUp(){}

  void TearDown() override {}
  int rc;
};

lunasa::DataObject generateLDO(int num_words, uint32_t start_val){
  lunasa::DataObject ldo(0, num_words*sizeof(int), lunasa::DataObject::AllocatorType::eager);
  int *x = static_cast<int *>(ldo.GetDataPtr());
  for(int i=0; i <num_words; i++)
    x[i]=start_val+i;
  return ldo;
}

TEST_F(MPISimplePeerTest, Setup) {
  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);
  EXPECT_EQ(kelpie::pool_behavior_t(kelpie::PoolBehavior::DefaultRemote), dht.GetBehavior());
  EXPECT_TRUE(dht.Valid());

  kelpie::Pool dht2 = kelpie::Connect(ResourceURL("/mydht"));
  EXPECT_EQ(kelpie::pool_behavior_t(kelpie::PoolBehavior::DefaultRemote), dht.GetBehavior());
  EXPECT_TRUE(dht2.Valid());

  //Try connecting with something not valid. Should get an error pool
  //handle back that tells us we're not valid. If we call pool function
  //on an invalid pool, it should throw an exception.
  kelpie::Pool dhtx = kelpie::Connect(ResourceURL("/not-here"));
  EXPECT_FALSE(dhtx.Valid());
  string s;
  dhtx.Valid(&s);
  EXPECT_FALSE(s.empty());
  EXPECT_ANY_THROW(dhtx.Info(kelpie::Key("dummy"),nullptr));
}

TEST_F(MPISimplePeerTest, BasicDHTCreateAndPublish){

  kelpie::Key k1("obj1");
  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);
  DirectoryInfo dir_info = dht.GetDirectoryInfo();
  EXPECT_EQ(G.mpi_size-1, dir_info.members.size());


  //First: Verify remote doesn't know what this item is
  kelpie::object_info_t col_info;
  rc = dht.Info(k1, &col_info);
  EXPECT_NE(0, rc);


  //Create an ldo to send
  auto ldo1 = generateLDO(1024, 0);
  EXPECT_EQ(0, ldo1.GetMetaSize());
  EXPECT_EQ(1024*sizeof(int), ldo1.GetMetaSize()+ldo1.GetDataSize());

  //Publish the object out to destination
  kelpie::object_info_t oi;
  rc = dht.Publish(k1, ldo1, &oi);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1024*sizeof(int), ldo1.GetMetaSize()+ldo1.GetDataSize()); //Sanity check

  EXPECT_EQ(kelpie::Availability::InRemoteMemory, oi.col_availability);

  //See if we can get the info back. Give it five tries
  int count=5;
  do{
    //cout <<"Fetch attempts left: "<<count<<endl;
    rc = dht.Info(k1, &col_info);
    if(rc!=0) sleep(1);
    count--;
  } while((rc!=0)&&(count>=0));

  //if(rc==0){ cout <<"Got info :"<<col_info.str()<<endl;
  //} else {   cout <<"No Info\n"; }

  EXPECT_EQ(0, rc);
  if(rc==0){
    //cout <<"Got info :"<<col_info.str()<<endl;
    EXPECT_EQ(1024*sizeof(int), col_info.col_user_bytes);
    EXPECT_EQ(kelpie::Availability::InRemoteMemory, col_info.col_availability);
  }
}
TEST_F(MPISimplePeerTest, BlockWildcardPubWants) {

  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);

  vector<kelpie::Key> bad_keys = { kelpie::Key("foo*"),
                                   kelpie::Key("foo","bar*"),
                                   kelpie::Key("foo*","bar*") };

  auto nop_pub = [](kelpie::rc_t result, kelpie::object_info_t &info) {};
  auto nop_want = [](bool success, kelpie::Key key, lunasa::DataObject user_ldo, const kelpie::object_info_t &info) {};
  lunasa::DataObject ldo;
  kelpie::ResultCollector res(1);
  kelpie::object_info_t info;
  for(auto bad_key : bad_keys) {
    EXPECT_ANY_THROW(dht.Publish(bad_key, nop_pub));
    EXPECT_ANY_THROW(dht.Publish(bad_key, ldo));
    EXPECT_ANY_THROW(dht.Publish(bad_key, ldo, &info));
    EXPECT_ANY_THROW(dht.Publish(bad_key, ldo, res));
    EXPECT_ANY_THROW(dht.Want(bad_key, nop_want));
    EXPECT_ANY_THROW(dht.Want(bad_key, 100, nop_want));
    EXPECT_ANY_THROW(dht.Want(bad_key, res));
    EXPECT_ANY_THROW(dht.Want(bad_key, 100, res));
  }

}


TEST_F(MPISimplePeerTest, BasicPublishGetBounded){

  kelpie::Key k2("obj2");
  int num_words=1024;

  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);
  DirectoryInfo dir_info = dht.GetDirectoryInfo();

  //Create an ldo to send
  auto ldo2 = generateLDO(num_words, 1);
  rc = dht.Publish(k2, ldo2);
  EXPECT_EQ(0, rc);


  //Do a bounded Get with Correct size
  lunasa::DataObject ldo2b;
  rc = dht.Need(k2, num_words*sizeof(int), &ldo2b);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(ldo2.GetDataSize(), ldo2b.GetDataSize());
  EXPECT_EQ(4096, ldo2.GetDataSize());
  int *x = static_cast<int *>(ldo2.GetDataPtr());
  int *y = static_cast<int *>(ldo2b.GetDataPtr());
  for(int i=0; i<ldo2.GetDataSize()/sizeof(int); i++){
    EXPECT_EQ(x[i], y[i]);
    //cout <<i<<" "<<x[i]<<" vs "<<y[i]<<endl;
  }
}

TEST_F(MPISimplePeerTest, BasicPublishGetUnbounded){

  kelpie::Key k3("obj3");
  int num_words=1024;

  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);
  DirectoryInfo dir_info = dht.GetDirectoryInfo();

  //Create an ldo to send
  auto ldo3 = generateLDO(num_words, 1);
  rc = dht.Publish(k3, ldo3);
  EXPECT_EQ(0, rc);

  //Do an unbounded get
  lunasa::DataObject ldo3b;
  rc = dht.Need(k3, &ldo3b);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(ldo3.GetDataSize(), ldo3b.GetDataSize());
  EXPECT_EQ(4096, ldo3.GetDataSize());
  int *x = static_cast<int *>(ldo3.GetDataPtr());
  int *y = static_cast<int *>(ldo3b.GetDataPtr());
  for(int i=0; i<ldo3.GetDataSize()/sizeof(int); i++){
    EXPECT_EQ(x[i], y[i]);
    //cout <<i<<" "<<x[i]<<" vs "<<y[i]<<endl;
  }

}

TEST_F(MPISimplePeerTest, BasicWantUnbounded){

  int num_words=1024;
  kelpie::Key k4("obj4");
  kelpie::object_info_t col_info;

  ResourceURL url("dht:/mydht");
  kelpie::Pool dht = kelpie::Connect(url);
  kelpie::Pool local = kelpie::Connect("local:");


  //First, make sure it isn't here yet
  rc = local.Info(k4, &col_info); EXPECT_EQ(kelpie::KELPIE_ENOENT, rc);
  rc =   dht.Info(k4, &col_info); EXPECT_EQ(kelpie::KELPIE_ENOENT, rc);

  //Tell dht we want a specific key. This should leave some info
  //in the destination
  rc = dht.Want(k4);              EXPECT_EQ(kelpie::KELPIE_OK, rc);

  //Info requests should reveal that we're waiting for the value
  rc = local.Info(k4, &col_info); EXPECT_EQ(kelpie::KELPIE_WAITING, rc);
  rc =   dht.Info(k4, &col_info); EXPECT_EQ(kelpie::KELPIE_WAITING, rc);



  //Create an ldo and do a blocking publish to DHT
  auto ldo4 = generateLDO(num_words, 1);
  rc = dht.Publish(k4, ldo4);  EXPECT_EQ(0, rc);

  //Prove that the want we sent resulted in the data being dropped here
  //when it was published.
  bool success=false;
  for(int i=0; i<5; i++){
    rc = local.Info(k4, &col_info);
    if(rc!=kelpie::KELPIE_OK) {
      sleep(1);
    } else {
      EXPECT_EQ(1024*sizeof(int), col_info.col_user_bytes);
      EXPECT_EQ(kelpie::Availability::InLocalMemory, col_info.col_availability);
      success=true;
      break;
    }
  }
  EXPECT_TRUE(success);


  //Do a need operation to get the actual data. It should be available
  //already.
  lunasa::DataObject ldo4b;
  rc = dht.Need(k4, &ldo4b);  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(ldo4.GetDataSize(), ldo4b.GetDataSize());
  EXPECT_EQ(4096, ldo4.GetDataSize());
  int *x = static_cast<int *>(ldo4.GetDataPtr());
  int *y = static_cast<int *>(ldo4b.GetDataPtr());
  for(int i=0; i<ldo4.GetDataSize()/sizeof(int); i++){
    EXPECT_EQ(x[i], y[i]);
    //cout <<i<<" "<<x[i]<<" vs "<<y[i]<<endl;
  }


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
