// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

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

TEST_F(LunasaBasic, saveLoad) {

  struct mymeta_t {
    uint32_t a;
    uint32_t b;
    uint64_t c;
  };

  DataObject ldo(sizeof(mymeta_t),1024*sizeof(int), DataObject::AllocatorType::eager, 0x88);
  mymeta_t *meta = ldo.GetMetaPtr<mymeta_t *>();
  meta->a=2001;
  meta->b=2003;
  meta->c=2005;
  int *data = ldo.GetDataPtr<int *>();
  for(int i=0; i<1024; i++)
    data[i]=1000+i;

  //Allocate simple data and free it
  size_t num_bytes = sizeof(mymeta_t) + 1024*sizeof(int);

  EXPECT_TRUE(Lunasa::SanityCheck());
  EXPECT_LE(num_bytes, Lunasa::TotalAllocated());
  EXPECT_EQ(0x88, ldo.GetTypeID());

  const char *filename = "tb_LunasaTest2.out";
  ldo.writeToFile(filename);


  //Load 1: Manually create the object
  DataObject read_obj1(0, num_bytes, DataObject::AllocatorType::eager);
  read_obj1.readFromFile(filename);



  EXPECT_EQ(0, memcmp(ldo.internal_use_only.GetHeaderPtr(),
                      read_obj1.internal_use_only.GetHeaderPtr(),
                      ldo.GetWireSize()));

  if(ldo.GetDataSize() == read_obj1.GetDataSize()) {
    EXPECT_EQ( 0, memcmp(ldo.GetDataPtr(), read_obj1.GetDataPtr(), 1024*sizeof(int)));
  }

  EXPECT_NE(0, read_obj1.GetMetaSize());
  EXPECT_EQ(ldo.GetMetaSize(), read_obj1.GetMetaSize());
  EXPECT_EQ(ldo.GetDataSize(), read_obj1.GetDataSize());
  EXPECT_EQ(ldo.GetTypeID(), read_obj1.GetTypeID());

  auto *meta2 = read_obj1.GetMetaPtr<mymeta_t *>();
  EXPECT_EQ(2001, meta2->a);
  EXPECT_EQ(2003, meta2->b);
  EXPECT_EQ(2005, meta2->c);


  //Load 2: Do the automated load
  auto read_obj2 = lunasa::LoadDataObjectFromFile(string(filename));

  EXPECT_EQ(0, memcmp(ldo.internal_use_only.GetHeaderPtr(),
                      read_obj2.internal_use_only.GetHeaderPtr(),
                      ldo.GetWireSize()));

  if(ldo.GetDataSize() == read_obj2.GetDataSize()) {
    EXPECT_EQ( 0, memcmp(ldo.GetDataPtr(), read_obj2.GetDataPtr(), 1024*sizeof(int)));
  }

  EXPECT_NE(0, read_obj2.GetMetaSize());
  EXPECT_EQ(ldo.GetMetaSize(), read_obj2.GetMetaSize());
  EXPECT_EQ(ldo.GetDataSize(), read_obj2.GetDataSize());
  EXPECT_EQ(ldo.GetTypeID(), read_obj2.GetTypeID());

  auto *meta3 = read_obj1.GetMetaPtr<mymeta_t *>();
  EXPECT_EQ(2001, meta3->a);
  EXPECT_EQ(2003, meta3->b);
  EXPECT_EQ(2005, meta3->c);

  EXPECT_ANY_THROW(auto bad_obj = lunasa::LoadDataObjectFromFile("/blah/blah/blah/not/a/real/file"));


  //Wipe out all objects so total allocated goes out
  ldo = DataObject();
  read_obj1 = DataObject();
  read_obj2 = DataObject();
  EXPECT_EQ(0, Lunasa::TotalAllocated());

}



TEST_F(LunasaBasic, multipleAllocs) {

  int sizes[] = {16, 81, 92, 48, 54, 64, 112, 3, 12}; //Small chunks, should sum < 1024
  vector<pair<int, DataObject>> mems;
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
  //int mpi_rank = 0, mpi_size;

  /* INITIALIZE Lunasa */
  bootstrap::Init(Configuration(default_config), lunasa::bootstrap);
  bootstrap::Start();

  rc = RUN_ALL_TESTS();

  /* FINALIZE Lunasa */
  bootstrap::Finish();

  return rc;
}
