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

default.kelpie.core_type nonet

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
)EOF";

void EagerRegisterMemory(void *base_addr, size_t length, void *&pinned)
{
    // TODO: use t_->registerMemory(...);
    pinned = (void*)0xDEADBEEF;
}
void LazyRegisterMemory(void *base_addr, size_t length, void *&pinned)
{
    // TODO: use t_->registerMemory(...);
    pinned = (void*)0xBEEFDEAD;
}
void UnregisterMemory(void *&pinned)
{
    pinned = nullptr;
}

class LunasaDataObjectTest : public testing::Test {
protected:
  virtual void SetUp () {
    Configuration config;

    lunasa::Init(config);

    lunasa = GetInstance();
  }
  virtual void TearDown () {
    lunasa::Finish();
  }
  Lunasa lunasa;

};

TEST_F(LunasaDataObjectTest, eagerPinning) {
  lunasa::RegisterPinUnpin(EagerRegisterMemory, UnregisterMemory);

  DataObject eagerPinned = lunasa.Alloc(DataObject::defaultMetaCapacity, 0, eager_memory);
  EXPECT_EQ((void*)0, eagerPinned.dataPtr ());
  EXPECT_EQ(0, eagerPinned.capacity ());
  //EXPECT_TRUE(eagerPinned.isPinned ());
  EXPECT_EQ((void*)0xDEADBEEF, eagerPinned.getRdmaPtr());
}

TEST_F(LunasaDataObjectTest, lazyPinning) {
  lunasa::RegisterPinUnpin(LazyRegisterMemory, UnregisterMemory);

  DataObject lazyPinned = lunasa.Alloc(DataObject::defaultMetaCapacity, 0, lazy_memory);
  EXPECT_EQ((void*)0, lazyPinned.dataPtr ());
  EXPECT_EQ(0, lazyPinned.capacity ());
  //EXPECT_TRUE(lazyPinned.isPinned ());
  EXPECT_EQ((void*)0xBEEFDEAD, lazyPinned.getRdmaPtr());
}
