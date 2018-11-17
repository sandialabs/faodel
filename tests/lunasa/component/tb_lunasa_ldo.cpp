// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
//#include <mpi.h>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config = R"EOF(

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

# IMPORTANT: this test won't work with tcmalloc implementation because it
# starts/finishes bootstrap multiple times.

lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
)EOF";

class LunasaDataObjectTest : public testing::Test {
protected:
  virtual void SetUp() {
    Configuration config(default_config);
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();
  }
  virtual void TearDown() {
    bootstrap::Finish();
  }
};

TEST_F(LunasaDataObjectTest, simpleSetups) {
  DataObject defaulted;

  EXPECT_EQ((void*)0, defaulted.GetDataPtr());
  EXPECT_EQ(0, defaulted.GetDataSize());
  EXPECT_FALSE(defaulted.isPinned());

  DataObject zeroUnpinned(0, 0, DataObject::AllocatorType::lazy);
  EXPECT_NE((void*)0, zeroUnpinned.GetDataPtr());
  EXPECT_EQ(0, zeroUnpinned.GetDataSize());
  EXPECT_FALSE(zeroUnpinned.isPinned());

  DataObject zeroPinned(0, 0, DataObject::AllocatorType::eager);
  EXPECT_NE((void*)0, zeroPinned.GetDataPtr());
  EXPECT_EQ(0, zeroPinned.GetDataSize());
  EXPECT_TRUE(zeroPinned.isPinned()); //Note: needs an actual net allocator to work

  DataObject oneUnpinned(0, 1, DataObject::AllocatorType::lazy);
  EXPECT_NE((void*)0, oneUnpinned.GetDataPtr());
  EXPECT_EQ(1, oneUnpinned.GetDataSize());
  EXPECT_FALSE(oneUnpinned.isPinned());
}

TEST_F(LunasaDataObjectTest, shallowCopy) {

  DataObject doubleUnpinned(0, sizeof(double), DataObject::AllocatorType::lazy);
  DataObject doubleCopy(doubleUnpinned);

  double testValue = 3.14159;
  memcpy(doubleUnpinned.GetDataPtr(), &testValue, sizeof(double));
  EXPECT_EQ(testValue, *(double*)doubleCopy.GetDataPtr());
  EXPECT_EQ(doubleCopy.GetDataPtr(), doubleUnpinned.GetDataPtr());
}

TEST_F(LunasaDataObjectTest, deepCopy) {

  DataObject doubleUnpinned(0, sizeof(double), DataObject::AllocatorType::lazy);

  double testValue = 1.61803;
  memcpy(doubleUnpinned.GetDataPtr(), &testValue, sizeof(double));

  DataObject doublePinned;
  doublePinned.deepcopy(doubleUnpinned);
  EXPECT_EQ(testValue, *(double*)doublePinned.GetDataPtr());
}

void MoveOrCopyTest(DataObject ldo, int expected_refs){
  EXPECT_EQ(expected_refs, ldo.internal_use_only.GetRefCount());
}

//Move semantics added October 2017 to help us avoid exta refcounting when
//passing data from one enity to another
TEST_F(LunasaDataObjectTest, moveLDO) {

  //Doing a plain copy should increase refcount
  DataObject ldo1a(0, 1024, DataObject::AllocatorType::eager);
  EXPECT_EQ(1, ldo1a.internal_use_only.GetRefCount());
  DataObject ldo1b = ldo1a;
  EXPECT_EQ(2, ldo1a.internal_use_only.GetRefCount());
  EXPECT_EQ(2, ldo1b.internal_use_only.GetRefCount());

  //A move should preserve refcount on new variable and set nullptr on the other
  DataObject ldo2a(0, 1024, DataObject::AllocatorType::eager);
  DataObject ldo2b = std::move(ldo2a);
  EXPECT_EQ(1, ldo2b.internal_use_only.GetRefCount());
  EXPECT_EQ(0, ldo2a.internal_use_only.GetRefCount());
  EXPECT_EQ(nullptr, ldo2a.GetDataPtr());

  //Moving an empty ldo should keep both at zero refcount and nullptrs
  DataObject ldo3a;
  EXPECT_EQ(0, ldo3a.internal_use_only.GetRefCount());
  DataObject ldo3b = std::move(ldo3a);
  EXPECT_EQ(0, ldo3b.internal_use_only.GetRefCount());
  EXPECT_EQ(0, ldo3a.internal_use_only.GetRefCount());
  EXPECT_EQ(nullptr, ldo3a.GetDataPtr());
  EXPECT_EQ(nullptr, ldo3b.GetDataPtr()); 

  //Create object and hand a copy to a function. Ref count changes going in and coming out
  DataObject ldo4(0, 1024, DataObject::AllocatorType::eager);
  MoveOrCopyTest(ldo4, 2);
  EXPECT_EQ(1, ldo4.internal_use_only.GetRefCount());
  
  //Create an object and move it to a function. Ref count should transfer and go to zero outside
  DataObject ldo5(0, 1024, DataObject::AllocatorType::eager);
  MoveOrCopyTest(std::move(ldo5), 1);
  EXPECT_EQ(0, ldo5.internal_use_only.GetRefCount());
  EXPECT_EQ(nullptr, ldo5.GetDataPtr());
}

#if 0
// DISABLE for now.  The use case for these string-initialized LDOs is still a bit hazy. [sll]
TEST_F(LunasaDataObjectTest, metaTest) {

  DataObject ldo_src = lunasa.AllocStruct("metaData", sizeof(double),1992);
  double testValue = 1.41421;
  memcpy(ldo_src.dataPtr(), &testValue, sizeof(double));
  uint32_t raw_length = ldo_src.rawSize();


  // Copy to a contiguous, raw buffer to simulate transfer to network
  void *raw_buf = malloc(raw_length);
  memcpy(raw_buf, ldo_src.rawPtr(), raw_length);

  // Create an ldo to hold the incoming data and copy it in
  DataObject ldo_dst = lunasa.Alloc(raw_length);
  memcpy(ldo_dst.rawPtr(), raw_buf, raw_length);

  EXPECT_EQ(ldo_src.rawSize(), ldo_dst.rawSize());
  EXPECT_EQ(8,                 ldo_dst.metaSize());
  EXPECT_EQ(sizeof(double),    ldo_dst.dataSize());
  EXPECT_EQ(1992,              ldo_dst.metaTag());
  EXPECT_EQ("metaData",        ldo_dst.meta());
  EXPECT_EQ(testValue,         *((double *)ldo_dst.dataPtr()));

  free(raw_buf);
}
#endif

#if 0
// DISABLE for now.  The use case for these string-initialized LDOs is still a bit hazy. [sll]
TEST_F(LunasaDataObjectTest, nullMetaTest) {
  DataObject nullMeta = lunasa.AllocStruct(std::string(5, '\0'), 0, 2112);
  EXPECT_EQ(0, nullMeta.meta()[0]);
  EXPECT_EQ(0, nullMeta.meta()[1]);
  EXPECT_EQ(0, nullMeta.meta()[2]);
  EXPECT_EQ(0, nullMeta.meta()[3]);
  EXPECT_EQ(0, nullMeta.meta()[4]);

  uint32_t raw_length = nullMeta.rawSize();

  // Copy to a contiguous, raw buffer to simulate transfer to network
  void *raw_buf = malloc(raw_length);
  memcpy(raw_buf, nullMeta.rawPtr(), raw_length);


  // Create an ldo to hold the incoming data and copy it in
  DataObject ldo_dst = lunasa.Alloc(raw_length);
  memcpy(ldo_dst.rawPtr(), raw_buf, raw_length);

  EXPECT_EQ(0, ldo_dst.meta()[0]);
  EXPECT_EQ(0, ldo_dst.meta()[1]);
  EXPECT_EQ(0, ldo_dst.meta()[2]);
  EXPECT_EQ(0, ldo_dst.meta()[3]);
  EXPECT_EQ(0, ldo_dst.meta()[4]);

  EXPECT_EQ(5,    ldo_dst.metaSize());
  EXPECT_EQ(0,    ldo_dst.dataSize());
  EXPECT_EQ(2112, ldo_dst.metaTag());


  free(raw_buf);
  
}
#endif

#if 0
// DISABLE for now.  The use case for these string-initialized LDOs is still a bit hazy. [sll]
TEST_F(LunasaDataObjectTest, resize) {

  DataObject ldo1 = lunasa.Alloc(2*1024);
  EXPECT_EQ(0, ldo1.metaSize());
  EXPECT_EQ(2*1024, ldo1.dataSize());

  //Make sure data is valid, but meta is nullptr
  uint8_t *ptr1, *ptr2;
  ptr1 = static_cast<uint8_t *>(ldo1.metaPtr());
  ptr2 = static_cast<uint8_t *>(ldo1.dataPtr());
  EXPECT_EQ(nullptr, ptr1);
  memset(ptr2, 0, 2*1024); // wipe it
  for(uint32_t i=0; i<255; i++)
    ptr2[i]=i;
  ptr2[1024]=0x36; //Special marker at 1KB offset

  //Make data smaller, but still no meta
  ldo1.Resize(1*1024);
  EXPECT_EQ(0, ldo1.metaSize());
  EXPECT_EQ(1*1024, ldo1.dataSize());
  ptr1 = static_cast<uint8_t *>(ldo1.metaPtr());
  ptr2 = static_cast<uint8_t *>(ldo1.dataPtr());
  EXPECT_EQ(nullptr, ptr1);
  for(uint32_t i=0; i<255; i++)
    EXPECT_EQ(i, ptr2[i]);

  uint8_t *ptrd_prv = ptr2;
  //Resize it with meta being non-zero
  ldo1.Resize(1*1024, 1*1024);
  EXPECT_EQ(1*1024, ldo1.metaSize());
  EXPECT_EQ(1*1024, ldo1.dataSize());
  ptr1 = static_cast<uint8_t *>(ldo1.metaPtr());
  ptr2 = static_cast<uint8_t *>(ldo1.dataPtr());
  EXPECT_EQ(ptrd_prv, ptr1); //used to be where data was pointing
  for(uint8_t i=0; i<255; i++)
    EXPECT_EQ(i, ptr1[i]);  //Meta should be whatever data was holding before
  EXPECT_EQ(0x36, ptr2[0]); //Read the special marker

  //Give some bad resizes
  try{
    ldo1.Resize(2*1024); //Meta is nonzero, so all space is gone
    EXPECT_TRUE(false);
  } catch(ResizeCapacity e) {}

  try{
    ldo1.Resize(1*1024, 1*1024+1); //too big
    EXPECT_TRUE(false);
  } catch(ResizeCapacity e) {}

  try{
    ldo1.Resize(1*1024+1, 1*1024); //too big
    EXPECT_TRUE(false);
  } catch(ResizeCapacity e) {}
  
}
#endif
