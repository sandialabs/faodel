// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <vector>
#include <sstream>

#include <stddef.h>

#include "gtest/gtest.h"
#include "faodel-common/Common.hh"
#include "faodel-common/SerializationHelpersBoost.hh"



using namespace std;
using namespace faodel;

class SerializationTest : public testing::Test {
protected:
  void SetUp() override {

  }
  internal_use_only_t iuo;

};

#include <boost/serialization/vector.hpp>

class A {
 public:
  A() {} //Need empty ctor for serialization
  int i;
  float f;
  string s;
  vector<int> vnums;
  A(int ii, float ff, string ss, vector<int> vv) {
    i=ii; f=ff; s=ss; vnums=vv;
  }

 //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & i;
    ar & f;
    ar & s;
    ar & vnums; //NOTE: Make sure you include boost serialization's vector
  }
};



TEST_F(SerializationTest, SimpleByHand) {

  vector<int>v = {1, 3, 5, 7};

  A a1(10,100.0, "one hundred", v);

  string s = BoostPack<A>(a1);

  A b1 = BoostUnpack<A>(s);


  EXPECT_EQ(10, b1.i);
  EXPECT_EQ(100.0, b1.f);
  EXPECT_EQ("one hundred", b1.s);
  int sum=0;
  for(int i=0; i<4; i++) {
    EXPECT_EQ(v[i], b1.vnums[i]);
    sum+=b1.vnums[i];
  }
  EXPECT_EQ(16, sum);


}

struct MyNodeStruct {
  nodeid_t root;
  vector<NameAndNode> nodes;

  //Boost-ish serializing
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & root;
    ar & nodes;
  }
};

TEST_F(SerializationTest, NameAndNode) {

  vector<string> names={"joe","bob","frank","sally","joejoe"};

  MyNodeStruct nans;
  nans.root = nodeid_t(0x36, iuo);
  for(size_t i=0; i<names.size(); i++)
    nans.nodes.push_back( NameAndNode(names[i], nodeid_t(100+i,iuo)));

  string packed = BoostPack<MyNodeStruct>(nans);

  MyNodeStruct nans2 = BoostUnpack<MyNodeStruct>(packed);

  EXPECT_EQ(nans.root, nans2.root);
  EXPECT_EQ(nans.nodes.size(), nans2.nodes.size());
  for(size_t i=0; i<nans.nodes.size(); i++) {
    if(i<nans2.nodes.size()) {
      EXPECT_EQ( nans.nodes[i], nans2.nodes[i]);
      EXPECT_EQ( nans.nodes[i].name, nans2.nodes[i].name);
      EXPECT_EQ( nans.nodes[i].node, nans2.nodes[i].node);
    }
  }
}

// This structure provides an example of how to pass around a (void*) blob.
// You need to split the archive into separate load/save functions, and
// then manually convert the blob item to a string (which boost understands
// how to pack).
//
// Note: the below approach is inefficient because you're sending the
//       blob length *twice*: once in blob_len, another time in the
//       string's length. A more efficient way to do this would be to
//       send a bool flag that says whether the blob has data or not.

struct B {
  int x;
  vector<string> y;
  char *blob;
  int   blob_len;

  B() : blob(nullptr), blob_len(0) {}


 template<class Archive>
  void save(Archive& ar, const unsigned version) const {
   ar & x;
   ar & y;
   ar & blob_len;
   if(blob_len) {
     std::string tmp(blob, blob_len);
     ar & tmp;
   }


  }

  template<class Archive>
  void load(Archive& ar, const unsigned version) {
    ar & x;
    ar & y;
    ar & blob_len;
    if(blob_len) {
      std::string tmp;
      ar & tmp;
      blob = (char *)malloc(blob_len);
      memcpy(blob, tmp.c_str(), blob_len);
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()

};
typedef struct {
  //Just some fake data to imitate a binary blob
  int x[16];
  int y[16];
  int z[16];
  int w;
} foo_t;

TEST_F(SerializationTest, BlobPointer) {

  vector<string> names={"joe","bob","frank","sally","joejoe"};

  B b1;
  b1.x = 100;
  for(auto &s : names)
    b1.y.push_back(s);

  //Create a fake binary object and assign to B
  foo_t *foo = (foo_t *)malloc(sizeof(foo_t));
  for(int i=0; i<16; i++) {
    foo->x[i] = 100+i;
    foo->y[i] = 200+i;
    foo->z[i] = 300+i;
  }
  foo->w = 2112;
  b1.blob_len = sizeof(foo_t);
  b1.blob = (char *)foo;


  //Pack into a string
  string packed = BoostPack<B>(b1);

  //Unpack string into struct
  B b2 = BoostUnpack<B>(packed);

  EXPECT_EQ(b1.x, b2.x);
  EXPECT_EQ(b1.y.size(), b2.y.size());
  EXPECT_EQ(b1.blob_len, b2.blob_len);

  for(size_t i=0; i<b1.y.size(); i++) {
    if(i<b2.y.size()) {
      EXPECT_EQ( b1.y[i], b2.y[i]);
    }
  }

  //Recast the blob and check the vals
  foo_t *foo2 = (foo_t *)b2.blob;
  EXPECT_NE(foo2, foo);
  for(int i=0;i<16;i++) {
    EXPECT_EQ((100+i), foo2->x[i] );
    EXPECT_EQ((200+i), foo2->y[i] );
    EXPECT_EQ((300+i), foo2->z[i] );
  }
  EXPECT_EQ(2112, foo2->w);

  //Get rid of the blob
  b1.blob = b2.blob = nullptr;
  b1.blob_len = b2.blob_len = 0;
  free(foo2);
  free(foo);

}

struct FakeKey {
  FakeKey() =default;
  FakeKey(int a_, int b_) : a(StringZeroPad(a_,255)), b(StringZeroPad(b_,255)) {}
  string a;
  string b;
  bool operator== (const FakeKey &other) const { return   (a == other.a) && (b == other.b);  }

  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & a;
    ar & b;
  }
};


TEST_F(SerializationTest, LargeNames) {

  //Example to make sure serialization can handle a larger number of objects
  vector<FakeKey> bignames;
  for(int i=0; i<128; i++) {
    for(int j=0; j<128; j++) {
      bignames.push_back( FakeKey(i,j) );
    }
  }

  string packed = BoostPack<vector<FakeKey>>(bignames);
  cout <<"Packed size is "<<packed.size()<<endl;
  vector<FakeKey> bignames2 = BoostUnpack<vector<FakeKey>>(packed);
  EXPECT_EQ(bignames.size(), bignames2.size());
  if(bignames.size()==bignames2.size()){
    for(int i=0; i<bignames.size(); i++ ) {
      EXPECT_EQ(bignames.at(i), bignames2.at(i));
    }
  }

}