// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <string>
#include <vector>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;


string default_config = R"EOF(

#bootstrap.debug true

# This test doesn't use dirman, thus needs to disable
dirman.type none

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class MultipleHiddenInits : public testing::Test {

protected:
  virtual void SetUp() {}
  void TearDown() override {}

};

class MyThing {

public:
  MyThing(string name) : name(name) {
    Configuration config(default_config);
    bootstrap::Start(config, kelpie::bootstrap);
    pool = kelpie::Connect("local:");
  }

  void put(string key_name, string val) {
    lunasa::DataObject ldo(val.size());
    memcpy(ldo.GetDataPtr<char *>(), val.c_str(), val.length());
    pool.Publish(kelpie::Key(key_name), ldo);
  }

  string get(string key_name) {
    lunasa::DataObject ldo;
    pool.Need(kelpie::Key(key_name), &ldo);
    string s(ldo.GetDataPtr<char *>(), ldo.GetDataSize());
    return s;
  }
  ~MyThing() {
    bootstrap::Finish();
  }

  string name;
  kelpie::Pool pool;


};


TEST_F(MultipleHiddenInits, basics) {

  //Each thing starts up, but only the first one should actually bootstrap
  MyThing *a = new MyThing("A");
  MyThing *b = new MyThing("B");

  EXPECT_EQ(2, bootstrap::GetNumberOfUsers());

  a->put("thing1","mydata1");
  a->put("thing2","mydata2");
  string s1 = b->get("thing1");
  EXPECT_EQ("mydata1", s1);

  delete a;

  string s2 = b->get("thing2");
  EXPECT_EQ("mydata2", s2);

  delete b;

  EXPECT_EQ(0, bootstrap::GetNumberOfUsers());

}

TEST_F(MultipleHiddenInits, classWins) {

  Configuration config(default_config);
  bootstrap::Start(config, kelpie::bootstrap);

  //Each thing starts up, but neither should bootstrap since we already started
  MyThing *a = new MyThing("A");
  MyThing *b = new MyThing("B");

  EXPECT_EQ(3, bootstrap::GetNumberOfUsers());

  a->put("thing1","mydata1");
  a->put("thing2","mydata2");
  string s1 = b->get("thing1");
  EXPECT_EQ("mydata1", s1);

  delete a;
  EXPECT_EQ(2, bootstrap::GetNumberOfUsers());

  bootstrap::Finish();
  EXPECT_EQ(1, bootstrap::GetNumberOfUsers());

  string s2 = b->get("thing2");
  EXPECT_EQ("mydata2", s2);

  delete b;

  EXPECT_EQ(0, bootstrap::GetNumberOfUsers());

}


TEST_F(MultipleHiddenInits, missingKelpie) {

  //Throw an error if someone starts bootstrap, then tries to register/start
  //higher level services

  Configuration config(default_config);
  bootstrap::Start(config, lunasa::bootstrap);

  //Bootstrap was started w/ Lunasa. MyThing asks for Kelpie, which isn't there.
  EXPECT_ANY_THROW(  MyThing *a = new MyThing("A") );

  EXPECT_EQ(1, bootstrap::GetNumberOfUsers());

  bootstrap::Finish();

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
