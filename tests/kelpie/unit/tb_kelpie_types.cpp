// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <algorithm>
#include "gtest/gtest.h"


#include "faodel-common/Common.hh"

#include "kelpie/common/Types.hh"

using namespace std;
using namespace kelpie;

class TypesTest : public testing::Test {
protected:
  void SetUp() override {
  }

};

TEST_F(TypesTest, ActionFlags){

  pool_behavior_t w2l = PoolBehavior::WriteToLocal;
  pool_behavior_t w2r = PoolBehavior::WriteToRemote;
  pool_behavior_t w2i = PoolBehavior::WriteToIOM;
  pool_behavior_t r2l = PoolBehavior::ReadToLocal;
  pool_behavior_t r2r = PoolBehavior::ReadToRemote;
  pool_behavior_t wa  = PoolBehavior::WriteAround;
  pool_behavior_t wall  = PoolBehavior::WriteToAll;


  EXPECT_EQ(w2l,     PoolBehavior::ParseString("WRITETOLOCAL") );
  EXPECT_EQ(w2l|r2l, PoolBehavior::ParseString("ReadToLocal_WRITETOLOCAL") );
  EXPECT_EQ(w2l,     PoolBehavior::ParseString("WRITETOLOCAL") );
  EXPECT_EQ(r2l|w2l, PoolBehavior::ParseString("writetolocal_readtolocal"));
  EXPECT_EQ(0,       PoolBehavior::ParseString(""));

  EXPECT_EQ((int)PoolBehavior::WriteToLocal,     PoolBehavior::ParseString("WRITETOLOCAL") );

  //Bad input
  EXPECT_ANY_THROW( PoolBehavior::ParseString("WRITETOLOCAL_bogus"));
  EXPECT_ANY_THROW( PoolBehavior::ParseString("WriteToLocal ReadToLocal"));


  EXPECT_EQ( w2l, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::WriteToRemote));
  EXPECT_EQ( w2l|w2i, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::WriteToAll));
  EXPECT_EQ( 0, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::ReadToNone));
  EXPECT_EQ( 0, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::WriteToLocal));
  EXPECT_EQ( 0, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::ReadToLocal));
  EXPECT_EQ( w2l|r2l, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::WriteToRemote | PoolBehavior::ReadToRemote));
  EXPECT_EQ( w2l|r2l|w2i, PoolBehavior::ChangeRemoteToLocal(PoolBehavior::WriteToRemote | PoolBehavior::ReadToRemote | PoolBehavior::WriteToIOM));

}

TEST_F(TypesTest, ObjectCapacities) {

  ObjectCapacities oc1,oc2,oc1b, oc2b;

  oc1.Append(Key("a"),1);
  oc1.Append(Key("c"),2);
  oc1.Append(Key("e"),3);
  oc1.Append(Key("g"),4);
  EXPECT_EQ(0, oc1.FindIndex(Key("a")));
  EXPECT_EQ(2, oc1.FindIndex(Key("e")));
  EXPECT_EQ(-1, oc1.FindIndex(Key("b")));
  EXPECT_EQ(4, oc1.keys.size());
  EXPECT_EQ(4, oc1.capacities.size());
  EXPECT_EQ(4, oc1.Size());

  oc2.Append(Key("b"),5);
  oc2.Append(Key("e"), -1);
  oc2.Append(Key("d"),6);
  oc2.Append(Key("f"),7);
  EXPECT_EQ(4, oc2.Size());

  //Make a copy
  oc1b = oc1;

  oc1.Merge(oc2); //Should not have duplicate e
  EXPECT_EQ(7, oc1.Size());

  oc1b.Append(oc2); //Should have duplicate e
  EXPECT_EQ(8, oc1b.Size());

  oc1.Wipe();
  EXPECT_EQ(0, oc1.Size());
  EXPECT_TRUE(oc1.keys.empty());
  EXPECT_TRUE(oc1.capacities.empty());

}
TEST_F(TypesTest, StructSizes) {

  EXPECT_EQ(1, sizeof(Availability));
  EXPECT_EQ(1, sizeof(pool_behavior_t));

  if(sizeof(object_info_t)!=24) {
    F_WARN("Size of kelpie's object_info_t has changed. This is ok for this platform, but may break heterogeneous runs")
  }
  EXPECT_EQ(24, sizeof(object_info_t));

}

TEST_F(TypesTest, ObjectInfo) {

  object_info_t oi;
  oi.row_user_bytes=-1;
  oi.row_num_columns=-2;
  oi.col_user_bytes=-3;
  oi.col_dependencies=-4;
  oi.col_availability=Availability::InLocalMemory;

  oi.ChangeAvailabilityFromLocalToRemote();
  EXPECT_EQ(Availability::InRemoteMemory, oi.col_availability);

  oi.Wipe();
  EXPECT_EQ(0,oi.row_user_bytes);
  EXPECT_EQ(0,oi.row_num_columns);
  EXPECT_EQ(0,oi.col_user_bytes);
  EXPECT_EQ(0,oi.col_dependencies);
  EXPECT_EQ(Availability::Unavailable,oi.col_availability);

  //cout<<sizeof(oi)<<endl;

}