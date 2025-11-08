// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <vector>
#include <algorithm>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "gtest/gtest.h"

#include "lunasa/common/Helpers.hh"
#include "faodel-common/ResourceURL.hh"
#include "kelpie/Kelpie.hh"

#include "kelpie/core/Singleton.hh" //For core access

using namespace std;
using namespace faodel;
using namespace kelpie;

//The configuration used in this example
std::string default_config_string = R"EOF(

# For local testing, tell kelpie to use the nonet implementation
kelpie.type nonet
dirman.type none

kelpie.debug true
kelpie.pool.debug true

# Uncomment these options to get debug info for each component
#bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.debug    true

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class KelpieCompute : public testing::Test {
protected:
  void SetUp() override {
    config.Append(default_config_string);
    bootstrap::Init(config, kelpie::bootstrap);
  }
  virtual void TearDown() {
    bootstrap::Finish();
  }

  rc_t rc;
  internal_use_only_t iuo;
  Configuration config;

};

rc_t fn_mystuff(faodel::bucket_t, const Key &key, const std::string &args,
                std::map<Key, lunasa::DataObject> ldos, lunasa::DataObject *ext_ldo) {

  cout<<"Got mystuff call for key "<<key.str()<<". Got "<<ldos.size()<<" hits.\n";

  stringstream ss;
  ss<<key.str()<<":"<<args;
  for(auto &k_v : ldos) {
    ss<<":"<<k_v.first.str();
  }
  *ext_ldo = lunasa::AllocateStringObject(ss.str());
  return KELPIE_OK;

}


TEST_F(KelpieCompute, Basics) {

  kelpie::RegisterComputeFunction("mystuff", fn_mystuff);
  bootstrap::Start();

  auto lpool = kelpie::Connect("lkv:");

  ResultCollector res(4);
  for(auto &name : {"a","b1","b2","c"}) {
    Key k1("Stuff",name);
    lunasa::DataObject ldo = lunasa::AllocateStringObject(k1.str());
    lpool.Publish(k1, ldo, res);
  }
  res.Sync();

  lunasa::DataObject ldo;
  //Get multiple cols
  rc =  lpool.Compute(Key("Stuff","b*"), "mystuff", "this_is_args_keys_follow_next", &ldo);
  string s = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ("Stuff|b*:this_is_args_keys_follow_next:Stuff|b1:Stuff|b2", s);
  cout <<"S is '"<<s<<"'\n";

  //Get single cols
  rc = lpool.Compute(Key("Stuff","b2"), "mystuff", "this_is_args_keys_follow_next", &ldo);
  string s2 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ("Stuff|b2:this_is_args_keys_follow_next:Stuff|b2", s2);

  //Look for missing item
  rc = lpool.Compute(Key("Stuff","NOPE"), "mystuff", "this_is_args_keys_follow_next", &ldo);
  string s3 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ(KELPIE_ENOENT, rc);
  EXPECT_EQ("Stuff|NOPE:this_is_args_keys_follow_next", s3); //Should have no keys

  //Pick first item in list
  rc = lpool.Compute(Key("Stuff","*"), "pick", "first", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s4 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|a", s4);

  //Pick the last item in the list
  rc = lpool.Compute(Key("Stuff","*"), "pick", "last", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s5 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|c", s5);

  //Pick the largest object. b1 and b2 are the largest, should pick the first
  rc = lpool.Compute(Key("Stuff","*"), "pick", "largest", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s6 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|b1", s6);

  //Pick the smallest object. There are multiple here (a and c). It should pick the first
  rc = lpool.Compute(Key("Stuff","*"), "pick", "smallest", &ldo);
  EXPECT_EQ(KELPIE_OK, rc);
  string s7 = lunasa::UnpackStringObject(ldo);
  EXPECT_EQ("Stuff|a", s7);

  cout <<"Done "<<rc<<"\n";

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
