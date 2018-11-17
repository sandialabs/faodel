// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config = R"EOF(

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
)EOF";

class LunasaBasic : public testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}

  uint64_t offset;
};


TEST_F(LunasaBasic, inits) {
  EXPECT_TRUE(Lunasa::SanityCheck());
  EXPECT_EQ(0, Lunasa::TotalAllocated());
}

TEST_F(LunasaBasic, simpleAlloc) {

  //Allocate simple data and free it
  size_t num_bytes = 100*sizeof(int);
  DataObject obj(0, num_bytes, DataObject::AllocatorType::eager);

  EXPECT_TRUE(Lunasa::SanityCheck());
  EXPECT_LE(num_bytes, Lunasa::TotalAllocated());

  //Try writing data to make sure we can use it.
  int *memi = static_cast<int *>(obj.GetDataPtr());
  for(int i = 0; i<100; i++)
    memi[i] = i;

  for(int i = 0; i<100; i++)
    EXPECT_EQ(i, memi[i]);

  const char *filename = "tb_LunasaTest1.out";
  obj.writeToFile(filename);

  DataObject read_obj(0, num_bytes, DataObject::AllocatorType::eager);
  read_obj.readFromFile(filename);
  EXPECT_EQ(0, memcmp(obj.internal_use_only.GetHeaderPtr(),
                      read_obj.internal_use_only.GetHeaderPtr(),
                      obj.GetWireSize()));

  obj = DataObject();
  read_obj = DataObject();
  EXPECT_EQ(0, Lunasa::TotalAllocated());
}


TEST_F(LunasaBasic, multipleAllocs) {

  int sizes[] = {16, 81, 92, 48, 54, 64, 112, 3, 12}; //Small chunks, should sum < 1024
  vector<pair<int, DataObject>> mems;
  int spot = 0;
  int tot_bytes = 0;
  for(auto s : sizes) {
    cerr << "allocating " << s << endl;
    DataObject obj(0, s, DataObject::AllocatorType::eager);
    mems.push_back(pair<int, DataObject>(s, obj));
    tot_bytes += s;
  }

  //cerr <<"Total Allocated: "<<lunasa_instance.TotalAllocated()<<endl;

  //Allocate simple data and free it
  EXPECT_TRUE(Lunasa::SanityCheck());
  EXPECT_LE(tot_bytes, Lunasa::TotalAllocated());

  random_shuffle(mems.begin(), mems.end());
  for(auto &id_ptr : mems) {
    id_ptr.second = DataObject();
    tot_bytes -= id_ptr.first;
    EXPECT_LE(tot_bytes, Lunasa::TotalAllocated());
  }
  EXPECT_EQ(0, Lunasa::TotalAllocated());
}


const int ALLOCATIONS = 1000;
const int THREADS = 4;

int main(int argc, char **argv) {
  int rc = 0;

  ::testing::InitGoogleTest(&argc, argv);
  int mpi_rank = 0, mpi_size;

  /* INITIALIZE Lunasa */
  bootstrap::Init(Configuration(default_config), lunasa::bootstrap);
  bootstrap::Start();

  rc = RUN_ALL_TESTS();

  /* FINALIZE Lunasa */
  bootstrap::Finish();

  return rc;
}
