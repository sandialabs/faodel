// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// Test: tb_kelpie_nonet_iompio
//
// This test sets up a no-net kelpie so we can test writing into a few different
// pools with pio drivers. The ioms point to locally created temp directories.
// It uses the Info, List, and Drop command to make sure objects get reported
// back in the their proper locations.

#include <iostream>
#include <vector>
#include <algorithm>
#include <atomic>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/ioms/IomRegistry.hh"
#include "kelpie/ioms/IomPosixIndividualObjects.hh"



using namespace std;
using namespace faodel;
using namespace kelpie;

//The configuration used in this example
std::string default_config_string = R"EOF(

# For local testing, tell kelpie to use the nonet implementation
kelpie.type nonet
dirman.type none


default.kelpie.iom.type  PosixIndividualObjects
default.kelpie.ioms      myiom1;myiom2;myiom3;myenv1
# note: additional iom info like path is filled in during SetUp()

# Uncomment these options to get debug info for each component
#bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.debug    true

#kelpie.pool.debug true
#kelpie.pool.logging_level debug
#kelpie.pool_registry.debug true
#kelpie.iom_registry.debug true
#kelpie.iom.debug true
#kelpie.lkv.debug true



# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

# enable when debugging:
#bootstrap.halt_on_shutdown true

)EOF";


class IomPosixIOSimple : public testing::Test {
protected:
  void SetUp() override {

    char p1[] = "/tmp/gtestXXXXXX";
    char p2[] = "/tmp/gtestXXXXXX";
    char p3[] = "/tmp/gtestXXXXXX";
    char p4[] = "/tmp/gtextXXXXXX";

    string path1 = mkdtemp(p1);
    string path2 = mkdtemp(p2);
    string path3 = mkdtemp(p3);
    string path4 = mkdtemp(p4);

    config.Append(default_config_string);
    config.Append("kelpie.iom.myiom1.path", p1);
    config.Append("kelpie.iom.myiom2.path", p2);
    config.Append("kelpie.iom.myiom3.path", p3);
    config.Append("kelpie.iom.myenv1.path.env_name", "MY_ENV_VAR");

    setenv("MY_ENV_VAR", "/tmp/bogyz" /*path4.c_str()*/, 1);

    bootstrap::Start(config, kelpie::bootstrap);

  }
  virtual void TearDown() {
    bootstrap::Finish(); //Previously this was FinishSoft?
  }

  rc_t rc;
  internal_use_only_t iuo;
  Configuration config;

};

struct test_data_t {
  uint32_t block_id;
  uint32_t data_bytes;
  char name[256];
  uint8_t data[0];
};

lunasa::DataObject createLDO(int id, string name, int data_bytes) {
  
  lunasa::DataObject ldo(sizeof(test_data_t),
                         sizeof(test_data_t)+data_bytes,
                         lunasa::DataObject::AllocatorType::eager);

  auto mptr = ldo.GetMetaPtr<test_data_t *>();
  auto dptr = ldo.GetDataPtr<test_data_t *>();

  mptr->block_id = id;
  mptr->data_bytes=0;

  memset(mptr->name, 0, 256);
  memset(dptr->name, 0, 256);

  string meta_name = "id-"+std::to_string(id);
  strncpy(mptr->name, meta_name.c_str(), 255);
  strncpy(dptr->name, name.c_str(), 255);

  dptr->block_id = id;
  dptr->data_bytes=data_bytes;
  for(uint32_t i=0; i<data_bytes; i++)
    dptr->data[i] = (i&0x0FF);

  return ldo;
}

bool checkLDO(const lunasa::DataObject &ldo, int id) {
  EXPECT_EQ(sizeof(test_data_t), ldo.GetMetaSize());
  auto mptr = ldo.GetMetaPtr<test_data_t *>();
  auto dptr = ldo.GetDataPtr<test_data_t *>();

  EXPECT_EQ(id, mptr->block_id);
  EXPECT_EQ(id, dptr->block_id);
  EXPECT_EQ(0,  mptr->data_bytes);

  string mid = string(mptr->name);
  EXPECT_EQ("id-"+std::to_string(id), mid);

  int bad_count=0;
  for(int i=0; i<dptr->data_bytes; i++)
    if( dptr->data[i] != (i&0x0FF) ) bad_count++;
  EXPECT_EQ(0, bad_count);

  return true;
}


TEST_F(IomPosixIOSimple, BasicIOMWrite) {

  int num_items=10;
  object_info_t ci;
  atomic<int> replies_left;

  kelpie::Pool plocal0 = kelpie::Connect("local:[my_bucket0]");
  kelpie::Pool plocal2 = kelpie::Connect("local:[my_bucket2]");

  kelpie::Pool piom1   = kelpie::Connect("[my_bucket1]/local/iom/myiom1");
  kelpie::Pool piom2   = kelpie::Connect("[my_bucket2]/local/iom/myiom2");
  kelpie::Pool piom3   = kelpie::Connect("[my_bucket3]/local/iom/myiom3");


  EXPECT_TRUE(plocal0.Valid());
  EXPECT_EQ(uint8_t(PoolBehavior::DefaultLocal),     plocal0.GetBehavior());
  EXPECT_EQ(uint8_t(PoolBehavior::DefaultLocalIOM),  piom2.GetBehavior());



  //Create a bunch of k/vs
  vector<pair<Key,lunasa::DataObject>> kvs;
  for(int i=0; i<num_items; i++){
    kvs.push_back( { Key("mybigitem",std::to_string(i)),
                     createLDO(i, "bozo-"+to_string(i), i*2) } );
  }

  auto fn_countdown = [&replies_left](rc_t result, object_info_t &ci) {
                     //cout <<"Published piom2 "<<ci.str()<<endl;
                     replies_left--;
                   };
  
  //Publish to iom2, which plocal0 is aliased to
  replies_left=num_items + (4*3);
  for(int i=0; i<num_items; i++) {
    piom2.Publish( kvs[i].first, kvs[i].second, fn_countdown);
  }

  for(int i=0; i<4; i++) {
    for(int j=0; j<3; j++) {
      piom2.Publish(Key("Something_" + to_string(i), "Other_" + to_string(j)),
                    createLDO(i, "stuff", 10),
                    fn_countdown);
    }
  }

  while(replies_left!=0) { sched_yield(); }


  //Check reads. Should only be available in bucket2 locations
  for(int i=0; i<num_items; i++) {
    plocal0.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable,   ci.col_availability);
    plocal2.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
    piom1.Info(   kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable,   ci.col_availability);
    piom2.Info(   kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
  }


  //Drop items and see if they're on disk via info commands
  for(int i=0; i<num_items; i++) {
    plocal2.Drop( kvs[i].first);
  }
  for(int i=0; i<num_items; i++) {
    plocal2.Info(kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable, ci.col_availability);
    piom2.Info(  kvs[i].first, &ci); EXPECT_EQ(Availability::InDisk,      ci.col_availability);
  }

  //See if the original items are on disk

  { //Test1: search for ("mybigitem","*")
    ObjectCapacities oc;
    piom2.List(Key("mybigitem", "*"), &oc);
    EXPECT_EQ(num_items, oc.Size());
    //cout <<"num items=" <<oc.keys.size()<<endl;
  }

  { //Test2: search for ("Something*","Other*")
    ObjectCapacities oc;
    piom2.List(Key("Something*", "*"), &oc);
    EXPECT_EQ(12, oc.Size());
    //cout <<"num items=" <<oc.keys.size()<<endl;
  }

  //Try a few different searches we have 1x12 mybigitems and 4x3 Somethings
  Key ksearch[7]; int expected_num[7];
  ksearch[0] = Key("mybigitem", "*");          expected_num[0] = num_items;
  ksearch[1] = Key("Something*", "*");         expected_num[1] = 12;
  ksearch[2] = Key("Something_1", "Other_2");  expected_num[2] = 1;
  ksearch[3] = Key("Something_1", "*");        expected_num[3] = 3;
  ksearch[4] = Key("Something_*", "Other_1");  expected_num[4] = 4;
  ksearch[5] = Key("Something_*", "Other_X*"); expected_num[5] = 0;
  ksearch[6] = Key("SomethingX*", "Other_*");  expected_num[6] = 0;

  for(int i=0; i<7; i++) {
    ObjectCapacities oc;
    piom2.List(ksearch[i], &oc);
    EXPECT_EQ(expected_num[i], oc.Size());
    //cout <<"search for "<<ksearch[i].str()<<"\tExpected: "<<expected_num[i]<<" Actual=" <<oc.keys.size()<<endl;
    //cout <<oc.str();
  }


}


TEST_F(IomPosixIOSimple, write_direct) {

  int num_items=1;
  object_info_t ci;
  atomic<int> replies_left;
   
  kelpie::Pool plocal0 = kelpie::Connect("local:[my_bucket0]");
  kelpie::Pool plocal2 = kelpie::Connect("local:[my_bucket2]");

  kelpie::Pool piom1   = kelpie::Connect("[my_bucket1]/local/iom/myiom1");
  kelpie::Pool piom2   = kelpie::Connect("[my_bucket2]/local/iom/myiom2");

  vector<pair<Key,lunasa::DataObject>> kvs;
  for(int i=0; i<num_items; i++){
    kvs.push_back( { Key("mybigitem",std::to_string(i)),
                     createLDO(i, "bozo-"+to_string(i), i*2) } );
  }

  //Step 1: Local Memory -----------------------------------------------------------------

  replies_left=num_items;
  //Write to local
  for(int i=0; i<num_items; i++) {
    plocal0.Publish( kvs[i].first, kvs[i].second,
                    [&replies_left] (rc_t result, object_info_t &ci) {
                      //cout <<"Published plocal0 "<<ci.str()<<endl;
                      replies_left--;
                    });
  }
  while(replies_left!=0) { sched_yield(); } //Block until really committed
  
  //Check reads. Should only be available in local memory
  for(int i=0; i<num_items; i++) {
    plocal0.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
    piom1.Info(  kvs[i].first, &ci);  EXPECT_EQ(Availability::Unavailable,   ci.col_availability);
    piom2.Info(  kvs[i].first, &ci);  EXPECT_EQ(Availability::Unavailable,   ci.col_availability);
  }
  //Drop from local
  for(int i=0; i<num_items; i++) {
    plocal0.Drop( kvs[i].first);
    plocal0.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable, ci.col_availability);
    piom1.Info(  kvs[i].first, &ci);  EXPECT_EQ(Availability::Unavailable, ci.col_availability);
    piom2.Info(  kvs[i].first, &ci);  EXPECT_EQ(Availability::Unavailable, ci.col_availability);
  }

  //Step 2: Using iom1 --------------------------------------------------------------------
 
  //Write to iom1
  replies_left=num_items;
  for(int i=0; i<num_items; i++) {
    piom1.Publish( kvs[i].first, kvs[i].second,
                    [&replies_left] (rc_t result, object_info_t &ci) {
                     //cout <<"Published piom1 "<<ci.str()<<endl;
                      replies_left--;
                    });
  }
  while(replies_left!=0) { sched_yield(); }
  
  //Check reads. Should only be available in local memory
  for(int i=0; i<num_items; i++) {
    plocal0.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable,  ci.col_availability);
    piom1.Info(  kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
    piom2.Info(  kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable,   ci.col_availability);
  }
  
  //Drop from memory
  for(int i=0; i<num_items; i++) {
    rc = piom1.Drop( kvs[i].first);  EXPECT_EQ(KELPIE_OK, rc);
    plocal0.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable, ci.col_availability);
    piom1.Info(  kvs[i].first, &ci); EXPECT_EQ(Availability::InDisk,      ci.col_availability);
    piom2.Info(  kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable, ci.col_availability);
  }


  //Step 3: Make sure plocal2 and iom2 use same lkv, but plocal2 doesn't go to disk

  //Write to iom2
  replies_left=num_items;
  for(int i=0; i<num_items; i++) {
    piom2.Publish( kvs[i].first, kvs[i].second,
                    [&replies_left] (rc_t result, object_info_t &oi) {
                     //cout <<"Published piom2 "<<ci.str()<<endl;
                      replies_left--;
                    });
  }
  while(replies_left!=0) { sched_yield(); }


  //Check reads. Should only be available in local memory
  for(int j=0; j<3; j++) {
    for(int i=0; i<num_items; i++) {
      plocal2.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
      piom2.Info(   kvs[i].first, &ci); EXPECT_EQ(Availability::InLocalMemory, ci.col_availability);
    }
  }
 

  //Drop from memory
  for(int i=0; i<num_items; i++) {
    //cout <<"Working on "<<kvs[i].first.str()<<endl;
    rc = piom2.Drop( kvs[i].first); EXPECT_EQ(KELPIE_OK, rc);
    plocal2.Info( kvs[i].first, &ci); EXPECT_EQ(Availability::Unavailable, ci.col_availability); //Bad?
    piom2.Info(   kvs[i].first, &ci); EXPECT_EQ(Availability::InDisk,      ci.col_availability);
  }  

  //cout <<"All done\n";

}


int main(int argc, char **argv) {

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  #ifdef Faodel_ENABLE_MPI_SUPPORT
    //If we don't do this, lower layers may try start/stopping mpi each time they run, causing ops after Finalize
    int mpi_rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    if(mpi_rank==0)
      rc = RUN_ALL_TESTS();
    MPI_Finalize();
  #else
    rc = RUN_ALL_TESTS();
  #endif

  return rc;
}
