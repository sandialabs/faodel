// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// This test is here just to verify that we don't screw up
// refcounts when we pass ldo's through backburner.

#include <atomic>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-services/BackBurner.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"


using namespace std;
using namespace faodel;
using namespace lunasa;


string default_config_string = R"EOF(

#bootstrap.debug true
#webhook.debug true
#lunasa.debug true

# Must use simple malloc for multiple start/stop tests
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc


)EOF";

class BackBurnerLDO : public testing::Test {
protected:
  void SetUp() override {
    backburner::bootstrap();
    bootstrap::Start(faodel::Configuration(default_config_string), lunasa::bootstrap);
  }

  void TearDown() override {
    bootstrap::FinishSoft();
  }
};

// Purpose:  Just demonstrate that we can issue a backburner op and block
//           until it finishes
TEST_F(BackBurnerLDO, SimpleSend) {

  atomic<int> aflag(0);

  backburner::AddWork([&aflag]() {
      aflag++;
      return 0;
  });
  while(aflag == 0) {}
  EXPECT_EQ(1, aflag);

}

// Purpose:  Pass a copy of an LDO into a backburner and watch it's refcounts
TEST_F(BackBurnerLDO, PassLDO) {
  atomic<int> aflag(0);

  lunasa::DataObject ldo1(0, 1024, lunasa::DataObject::AllocatorType::eager);
  int *x = (int *) ldo1.GetDataPtr();
  for(int i = 0; i<1024/sizeof(int); i++)
    x[i] = i;

  cout << "Launch: Ldo1 size " << ldo1.GetDataSize() << " refcount " << ldo1.internal_use_only.GetRefCount() << endl;

  //Create a copy and destroy it to verify refcount goes up and down
  {
    DataObject ldo2 = ldo1;
    cout << "new ldo2 inside refcount is " << ldo1.internal_use_only.GetRefCount() << endl;
    EXPECT_EQ(2, ldo1.internal_use_only.GetRefCount());
  }
  EXPECT_EQ(1, ldo1.internal_use_only.GetRefCount());
  cout << "After ldo2 refcount is " << ldo1.internal_use_only.GetRefCount() << endl;


  //Launch a task to see what ref count looks like
  backburner::AddWork([ldo1, &aflag]() {
      while(aflag == 0) {}
      cout << "BackBurner lambda count: " << ldo1.internal_use_only.GetRefCount()
           << " size is " << ldo1.GetDataSize() << endl;
      int *x = (int *) ldo1.GetDataPtr();
      for(int i = 0; i<8; i++) {
        cout << i << " " << x[i] << endl;
        x[i] = 100 - i;
      }
      aflag = 2;
      return 0;
  });

  //Backburner launched, but it's stalled, waiting for us to set aflag
  cout << "Pre Backburner count is " << ldo1.internal_use_only.GetRefCount() << endl;
  EXPECT_EQ(2, ldo1.internal_use_only.GetRefCount());
  aflag = 1;

  //Wait for backburner to finish
  while(aflag != 2) {}
  cout << "Post Backburner count is " << ldo1.internal_use_only.GetRefCount() << endl;

  //NOTE: At this point the backburner is done, but it may not have destroyed its ldo yet

  //Stall until bburner destroys its copy of the ldo
  while(ldo1.internal_use_only.GetRefCount() != 1) {
    cout << "Post backburner still waiting for bb to destroy its ldo\n";
  }

  for(int i = 0; i<8; i++) {
    EXPECT_EQ(100 - i, x[i]);
    cout << "Result: " << x[i] << endl;
  }


}


// Purpose:  See if we can copy the ldo in backburner
TEST_F(BackBurnerLDO, PassAndCopyLDO) {
  atomic<int> aflag(0);

  DataObject ldo1(0, 1024, lunasa::DataObject::AllocatorType::eager);
  EXPECT_EQ(1, ldo1.internal_use_only.GetRefCount());



  //Launch a task to see what ref count looks like
  backburner::AddWork([ldo1, &aflag]() {

      cout << "BackBurner lambda count: " << ldo1.internal_use_only.GetRefCount()
           << " size is " << ldo1.GetDataSize() << endl;
      EXPECT_EQ(2, ldo1.internal_use_only.GetRefCount());

      DataObject ldo2 = ldo1;
      EXPECT_EQ(3, ldo2.internal_use_only.GetRefCount());

      aflag = 2;
      return 0;
  });


  //Wait for backburner to finish
  while(aflag != 2) {}
  cout << "Post Backburner count is " << ldo1.internal_use_only.GetRefCount() << endl;

  //NOTE: At this point the backburner is done, but it may not have destroyed its ldo yet

  //Stall until bburner destroys its copy of the ldo
  while(ldo1.internal_use_only.GetRefCount() != 1) {
    cout << "Post backburner still waiting for bb to destroy its ldo\n";
  }


}
