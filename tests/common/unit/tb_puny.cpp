// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include "gtest/gtest.h"

#include "common/StringHelpers.hh"

using namespace std;
using namespace faodel;

TEST(PunyCode, Basics) {
  const string src="This is the input/output that I ~want to store!!";
  string s="This is it";

  string enc1 = MakePunycode(src);
  string dec1 = ExpandPunycode(enc1);
  string enc2 = MakePunycode(dec1);
  string dec2 = ExpandPunycode(enc2);

  EXPECT_EQ(src,dec1);
  EXPECT_EQ(src,dec2);
  EXPECT_EQ(enc1,enc2);
  EXPECT_NE(src,enc1);
}
TEST(PunyCode, ZeroVals) {

  //Put a zero in the middle of it.
  //Needed because kelpie sometimes encodes a key's lengths into
  //a string as chars.
  string s="The end";
  s[3]=0;
  EXPECT_EQ("The%00end", MakePunycode(s));

}

TEST(PunyCode, RawData) {
  //Build a string with chars 1-255
  stringstream ss;
  for(unsigned int i=0;i<255; i++) {
    char x = (i+1)&0x0FF;
    ss << x;
  }
  string src = ss.str();
  string enc = MakePunycode(src);
  string dec = ExpandPunycode(enc);

  EXPECT_EQ(src,dec);
  EXPECT_NE(src,enc);
}
