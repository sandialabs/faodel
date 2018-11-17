// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


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