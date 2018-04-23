// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include "gtest/gtest.h"
#include "common/Common.hh"

using namespace std;
using namespace faodel;

class BucketTest : public testing::Test {
protected:
  virtual void SetUp() {
  }
  internal_use_only_t iuo;
};
TEST_F(BucketTest, SimpleByHand) {

  bucket_t b;





}


TEST_F(BucketTest, Sizes) {
  bucket_t b1;
  bucket_t b2[10];

  EXPECT_EQ(4, sizeof(b1));
  EXPECT_EQ(sizeof(uint32_t), sizeof(b1));
  EXPECT_EQ(sizeof(uint32_t)*10, sizeof(b2));
}

TEST_F(BucketTest, Compares) {
  bucket_t bs[10];
  for(int i=0; i<10; i++)
    bs[i] = bucket_t(i, iuo);

  for(int i=1; i<10; i++) {
    EXPECT_TRUE( bs[i].Valid() );
    EXPECT_FALSE( bs[i].Unspecified() );
    EXPECT_TRUE( bs[i-1].bid <  bs[i].bid );
    EXPECT_TRUE( bs[i-1].bid <= bs[i].bid );
    EXPECT_TRUE( bs[i].bid   >  bs[i-1].bid );
    EXPECT_TRUE( bs[i].bid   >= bs[i-1].bid );

    EXPECT_TRUE( bs[i-1].GetID() <  bs[i].bid );
    EXPECT_TRUE( bs[i-1].GetID() <= bs[i].bid );
    EXPECT_TRUE( bs[i].GetID()   >  bs[i-1].bid );
    EXPECT_TRUE( bs[i].GetID()   >= bs[i-1].bid );


    EXPECT_FALSE( bs[i].bid == bs[i-1].bid );
    EXPECT_TRUE( bs[i].bid  != bs[i-1].bid );
  }
}

TEST_F(BucketTest, Copies) {
  string name="This is the string";
  bucket_t bsrc(name);
  bucket_t bdst;

  //Shouldn't be equal
  EXPECT_NE(bsrc.bid,      bdst.bid);
  EXPECT_NE(bsrc.GetHex(), bdst.GetHex());
  EXPECT_NE(bsrc,          bdst);

  //Should be the same
  bdst = bsrc;
  EXPECT_EQ(bsrc.bid,      bdst.bid);
  EXPECT_EQ(bsrc.GetHex(), bdst.GetHex());
  EXPECT_EQ(bsrc,          bdst);
}



TEST_F(BucketTest, BadHexNode) {
  uint64_t x;
  string s;

  //If we already computed the hash, we can pass it around
  //as a hex string. The string has to start with 0x and
  //contain less than 8 hex digits to be valid.
  string bad_vals[] = {   "0xf000000000000000",
                          "0x123456789",
                          "0x123G5678",
                          "0x1234567x",
                          "0x123 5678",
                          ""};

  for(int i=0; bad_vals[i]!=""; i++) {
    //cout <<"Trying "<<bad_vals[i]<<endl;
    try {
      bucket_t b(bad_vals[i]);
      cout <<b.GetHex()<<endl;
      EXPECT_TRUE(false); //Should be a problem
    } catch(exception e) {
      EXPECT_TRUE(true);
    }
  }

}

