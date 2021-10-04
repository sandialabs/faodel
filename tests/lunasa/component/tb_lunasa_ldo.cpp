// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
//#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config = R"EOF(

# IMPORTANT: this test won't work with tcmalloc implementation because it
#            starts/finishes bootstrap multiple times.

lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

#lkv settings for the server
server.mutex_type   rwlock

node_role server
)EOF";

class LunasaDataObjectTest : public testing::Test {
protected:
  void SetUp() override {
    Configuration config(default_config);
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();
  }

  void TearDown() override {
    bootstrap::Finish();
  }
};

//Make sure allocations wind up being right
TEST_F(LunasaDataObjectTest, StructSanityCheck) {

  //Plain allocation: should be tagged
  DataObject ldo1(1024);
  EXPECT_EQ(0,    ldo1.GetMetaSize());
  EXPECT_EQ(1024, ldo1.GetDataSize());
  EXPECT_EQ(1024, ldo1.GetUserSize());
  EXPECT_LE(1024, ldo1.GetUserCapacity()); //Capacity May be a +4 bytes for safety
  EXPECT_EQ(8,    ldo1.GetHeaderSize()); //Header should be fixed to uin16+uint16+uint32
  EXPECT_EQ(1032, ldo1.GetWireSize());   //Header+user
  //EXPECT_EQ(1072, ldo1.GetRawAllocationSize());  //Sanity check: copied to catch changes in alloc size

}

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

  //Make sure capacity allocations work as expected
  EXPECT_NO_THROW( DataObject ldo_ok(100,50,50,DataObject::AllocatorType::eager, 0) );
  EXPECT_ANY_THROW(DataObject ldo_bad(100,50,51,DataObject::AllocatorType::eager, 0) );
  EXPECT_ANY_THROW(DataObject ldo_bad(4294967295,10,4294967294,DataObject::AllocatorType::eager, 0) );
}

TEST_F(LunasaDataObjectTest, capcityChanges) {

  //Advanced users might want to move their data structures around after they've
  //stored everything.

  DataObject ldo1(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);
  EXPECT_LE(1024, ldo1.GetUserCapacity()); //May get more than asked for, fo alignment
  EXPECT_EQ(64,   ldo1.GetMetaSize());
  EXPECT_EQ(128,  ldo1.GetDataSize());
  EXPECT_EQ(192,  ldo1.GetUserSize());

  int rc;
  rc= ldo1.ModifyUserSizes(256, 512);  EXPECT_EQ(0, rc);
  rc= ldo1.ModifyUserSizes(512, 512);  EXPECT_EQ(0, rc);
  rc= ldo1.ModifyUserSizes(64,  102);  EXPECT_EQ(0, rc);
  rc= ldo1.ModifyUserSizes(512, 517);  EXPECT_EQ(-1, rc); //Note: capacity may be 1024+4 for alignment, try to validate
  rc= ldo1.ModifyUserSizes(517, 512);  EXPECT_EQ(-1, rc);

  //Make sure the sizes are still set to last valid setting
  EXPECT_EQ(64, ldo1.GetMetaSize());
  EXPECT_EQ(102, ldo1.GetDataSize());


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

TEST_F(LunasaDataObjectTest, DeepCompare) {

  //Empties
  DataObject empty1, empty2;
  EXPECT_TRUE( empty1==empty2);
  EXPECT_EQ(0, empty1.DeepCompare((empty1)));
  EXPECT_EQ(0, empty1.DeepCompare((empty2)));
  EXPECT_EQ(0, empty2.DeepCompare((empty1)));

  //Create two objects that are the same except capacity
  DataObject item1(8192, 1024,4096, DataObject::AllocatorType::eager, 0x1941);
  DataObject item2(8000, 1024,4096, DataObject::AllocatorType::eager, 0x1941);
  memset(item1.GetMetaPtr(), 0, item1.GetMetaSize());
  memset(item2.GetMetaPtr(), 0, item2.GetMetaSize());
  memset(item1.GetDataPtr(), 0, item1.GetDataSize());
  memset(item2.GetDataPtr(), 0, item2.GetDataSize());
  EXPECT_NE(item1, item2);
  EXPECT_EQ(0, item1.DeepCompare(item2));
  EXPECT_EQ(0, item2.DeepCompare(item1));

  //Change last byte in data
  auto dptr = item2.GetDataPtr<char *>();
  dptr[item2.GetDataSize()-1] = 0x01;
  EXPECT_EQ(-6, item1.DeepCompare(item2));
  EXPECT_EQ(-6, item2.DeepCompare(item1));

  //Change last byte in meta
  auto mptr = item2.GetMetaPtr<char *>();
  mptr[item2.GetMetaSize()-1] = 0x02;
  EXPECT_EQ(-5, item1.DeepCompare(item2));
  EXPECT_EQ(-5, item2.DeepCompare(item1));

  //Change data size
  item2.ModifyUserSizes(1024, 4095);
  EXPECT_EQ(-4, item1.DeepCompare(item2));
  EXPECT_EQ(-4, item2.DeepCompare(item1));

  //Change meta size
  item2.ModifyUserSizes(1023, 4096);
  EXPECT_EQ(-3, item1.DeepCompare(item2));
  EXPECT_EQ(-3, item2.DeepCompare(item1));

  //Screw up the
  item1.SetTypeID(0x1940);
  EXPECT_EQ(-2, item1.DeepCompare(item2));
  EXPECT_EQ(-2, item2.DeepCompare(item1));

  //Reset all things
  item1.SetTypeID(0x1941);
  item2.ModifyUserSizes(1024,4096);
  mptr[item2.GetMetaSize()-1] = 0x0;
  dptr[item2.GetDataSize()-1] = 0x0;
  EXPECT_EQ(0, item1.DeepCompare(item2));

  EXPECT_EQ(-1, item1.DeepCompare(empty1));
  EXPECT_EQ(-1, empty1.DeepCompare(item1));

}
