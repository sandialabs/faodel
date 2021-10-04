// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <algorithm>
#include "gtest/gtest.h"
#include <arpa/inet.h> //htonl

#include "faodel-common/Common.hh"
#include "faodel-common/SerializationHelpersBoost.hh"

#include "kelpie/Key.hh"


using namespace std;
using namespace kelpie;

class KeyTest : public testing::Test {
protected:
  void SetUp() override {
    a1 = Key("Booya");
    b1 = Key("Booya");
    c1 = Key("Not Booya");

    a2 = Key("Booya",  "Shizzam");
    b2 = Key("Booya",  "Shizzam");
    c2 = Key("Booya",  "Shizzamduh");
    d2 = Key("Booya2", "Shizzam");


  }

  Key a1,b1,c1,a2,b2,c2,d2;
};

TEST_F(KeyTest, Compare){

  //External string dump
  EXPECT_EQ(a1.str(), a1.str());
  EXPECT_EQ(a1.str(), b1.str());
  EXPECT_NE(b1.str(), c1.str());

  //Direct comparisons 1d
  EXPECT_EQ(a1, a1);
  EXPECT_EQ(a1, b1);
  EXPECT_NE(b1, c1);

  //1D vs 2D
  EXPECT_NE(a1, a2);
  EXPECT_NE(b1, a2);

  //2d to 2d
  EXPECT_EQ(a2, a2);
  EXPECT_EQ(a2, b2);
  EXPECT_NE(b2, c2);
  EXPECT_NE(a2, d2);
}

TEST_F(KeyTest, Templated){
  //Try inserting some numbers and make sure they go in as expected
  Key a("21.12","19.77");
  Key b("21","19");
  Key f,i;
  f.TemplatedK1<float>(21.12);
  f.TemplatedK2<float>(19.77);
  i.TemplatedK1<int>(21.12);
  i.TemplatedK2<int>(19.77);
  EXPECT_EQ(f,a);
  EXPECT_EQ(f.str(), a.str());
  EXPECT_NE(f,b);
  EXPECT_EQ(i,b);
  EXPECT_EQ(i.str(), b.str());
  EXPECT_NE(i,a);
}

TEST_F(KeyTest, Sorting){

  string labels[] = { "zed","bob", "frank", "joe", "fish"};
  int num_labels = 5;
  string prv1,prv2;

  vector<Key> keys;
  for(int i=0; i<num_labels; i++)
    keys.push_back(Key(labels[i]));

  sort(keys.begin(), keys.end());

  prv1="";
  for(vector<Key>::iterator ii = keys.begin(); ii!=keys.end(); ++ii){
    bool correct = prv1 <= ii->K1();
    EXPECT_TRUE(correct);
    //cout <<ii->K1()<<endl;
    prv1=ii->K1();
  }

  keys.clear();
  for(int i=0; i<num_labels; i++){
    for(int j=0; j<10; j++){
      keys.push_back(Key(labels[i%num_labels], labels[j%num_labels]));
    }
  }

  sort(keys.begin(), keys.end());
  prv1=prv2="";
  for(vector<Key>::iterator ii=keys.begin(); ii!=keys.end(); ++ii){
    bool correct = (prv1 < ii->K1()) || ((prv1==ii->K1()) && (prv2 <= ii->K2()));
    EXPECT_TRUE(correct);
    //cout << ii->K1() <<" "<<ii->K2()<<endl;
    prv1=ii->K1();
    prv2=ii->K2();
  }

}


TEST_F(KeyTest, Binary){

  //Note: When storing binary data, remember it will sort on
  // the underlying byte array- not the unpacked val. When
  // you pack an int for example, it will sort on the little
  // endian bytes. So in this test, don't think of it as an
  // int, think of it as a 4 byte sequence. Use htonl to reinterpret
  // what the sorted vals actually are.

  int num_int = 16;
  int *ids = new int[num_int+1];
  Key *keys = new Key[num_int];
  int prv1, prv2;

  for(int i=0; i<num_int+1; i++) ids[i]=rand();

  //1D Test
  for(int i=0; i<num_int; i++){
    keys[i] = Key(&ids[i], sizeof(int));
  }

  for(int i=0; i<num_int; i++){
    string s = keys[i].K1();
    EXPECT_EQ(sizeof(int), s.length());

    //Unpack to an int
    int unpacked;
    memcpy(&unpacked, s.c_str(), sizeof(int));
    EXPECT_EQ(ids[i], unpacked);
    //cout<<hex<<ids[i]<<" "<<unpacked<<endl;
  }

  sort(keys, keys+num_int);
  prv1=0;
  for(int i=0; i<num_int; i++){
    int unpacked1;
    memcpy(&unpacked1, keys[i].K1().c_str(), sizeof(int));
    bool correct = (htonl(unpacked1)>htonl(prv1));
    EXPECT_TRUE(correct);
    prv1=unpacked1;
    //cout<<hex<<htonl(unpacked1)<<endl;
  }

  //2D Test
  for(int i=0; i<num_int; i++){
    keys[i] = Key(&ids[i], sizeof(int), &ids[i+1], sizeof(int));
  }

  for(int i=0; i<num_int; i++){
    string s1 = keys[i].K1();
    string s2 = keys[i].K2();
    EXPECT_EQ(sizeof(int), s1.length());
    EXPECT_EQ(sizeof(int), s2.length());

    int unpacked1, unpacked2;
    memcpy(&unpacked1, s1.c_str(), sizeof(int));
    memcpy(&unpacked2, s2.c_str(), sizeof(int));
    EXPECT_EQ(ids[i], unpacked1);
    EXPECT_EQ(ids[i+1], unpacked2);
    //cout<<hex<<ids[i]<<":"<<ids[i+1]<<"  "<<unpacked1<<":"<<unpacked2<<endl;
  }

  //See if keys get sorted as expected
  sort(keys, keys+num_int);
  prv1=prv2=0;
  for(int i=0; i<num_int; i++){
    int unpacked1, unpacked2;
    memcpy(&unpacked1, keys[i].K1().c_str(), sizeof(int));
    memcpy(&unpacked2, keys[i].K2().c_str(), sizeof(int));
    bool correct = (htonl(unpacked1)>htonl(prv1)) || ((unpacked1==prv1)&&(htonl(unpacked2)>=htonl(prv2)));
    EXPECT_TRUE(correct);
    //cout<<hex<<htonl(unpacked1)<<":"<<htonl(unpacked2)<<endl;
    prv1=unpacked1;
    prv2=unpacked2;
  }

  delete[] ids;
  delete[] keys;

}

TEST_F(KeyTest, Packing){

  Key k1("This is my first key", "This is my second key");

  string s = faodel::BoostPack<Key>(k1);
  Key k2 = faodel::BoostUnpack<Key>(s);
  EXPECT_EQ(k1,k2);

  int d1,d2;
  d1=0x12345678;
  d2=0xdeadbeef;
  k1=Key(&d1, sizeof(int), &d2, sizeof(int));
  s = faodel::BoostPack<Key>(k1);
  k2 = faodel::BoostUnpack<Key>(s);
  EXPECT_EQ(k1,k2);

  int u1,u2;
  memcpy(&u1, k2.K1().c_str(), sizeof(int));
  memcpy(&u2, k2.K2().c_str(), sizeof(int));

  EXPECT_EQ(d1,u1);
  EXPECT_EQ(d2,u2);


}

TEST_F(KeyTest, Wildcards) {

  Key k[4];
  k[0] = Key("MyRowName",  "MyColName"); EXPECT_FALSE( k[0].IsRowWildcard() ); EXPECT_FALSE( k[0].IsColWildcard());
  k[1] = Key("MyRowWild*", "MyColName"); EXPECT_TRUE(  k[1].IsRowWildcard() ); EXPECT_FALSE( k[1].IsColWildcard());
  k[2] = Key("MyRowName",  "MyColWild*");EXPECT_FALSE( k[2].IsRowWildcard() ); EXPECT_TRUE(  k[2].IsColWildcard());
  k[3] = Key("MyRowWild*", "MyColWild*");EXPECT_TRUE(  k[3].IsRowWildcard() ); EXPECT_TRUE(  k[3].IsColWildcard());
  Key kall("*", "*");                    EXPECT_TRUE(  kall.IsRowWildcard() ); EXPECT_TRUE(  kall.IsColWildcard());
  Key krow("MyRow*", "*");               EXPECT_TRUE(  krow.IsRowWildcard() ); EXPECT_TRUE(  krow.IsColWildcard());

  //Manual tests on a real key
  EXPECT_TRUE( k[0].Matches(k[0])); //Self test
  EXPECT_TRUE( k[0].Matches("MyRowName", "MyColName"));
  EXPECT_TRUE( k[0].Matches("MyRowName", "MyColName*"));
  EXPECT_TRUE( k[0].Matches("MyRowName", "MyCol*"));
  EXPECT_TRUE( k[0].Matches("MyRowName", "*"));
  EXPECT_TRUE( k[0].Matches("MyRowName*","*"));
  EXPECT_TRUE( k[0].Matches("MyRow*",    "MyColName"));
  EXPECT_TRUE( k[0].Matches("*",         "MyColName"));
  EXPECT_TRUE( k[0].Matches("*",         "MyCol*"));
  EXPECT_TRUE( k[0].Matches("*",         "*"));

  //Wrong case: tests should fail
  EXPECT_FALSE( k[0].Matches("MyRowName", "myColName"));
  EXPECT_FALSE( k[0].Matches("myRowName", "MyColName"));
  EXPECT_FALSE( k[0].Matches("myRowName", "myColName"));
  EXPECT_FALSE( k[0].Matches("MyRow*",    "myColName"));
  EXPECT_FALSE( k[0].Matches("MyRow*",    "my*"));
  EXPECT_FALSE( k[0].Matches("*",         "my*"));
  EXPECT_FALSE( k[0].Matches("myRow*",    "MyColName"));
  EXPECT_FALSE( k[0].Matches("myRowName", "*"));

  for(int i=0; i<4; i++) {
    EXPECT_TRUE(k[i].Matches(Key("*", "*")));
    EXPECT_TRUE(k[i].Matches(kall));
    EXPECT_TRUE(k[i].Matches(kall.K1(), kall.K2()));
    EXPECT_TRUE(k[i].Matches(krow)); //All have same rowname
  }

  //Above should have hit this code, but a few just in case
  EXPECT_TRUE( k[0].matchesPrefixString(false, "MyRowName", false, "MyColName"));
  EXPECT_TRUE( k[0].matchesPrefixString(true,  "My",        false, "MyColName"));
  EXPECT_TRUE( k[0].matchesPrefixString(false, "MyRowName", true,  "My"));
  EXPECT_TRUE( k[0].matchesPrefixString(true,  "My",        true,  "My"));

  EXPECT_FALSE(k[0].matchesPrefixString(false, "My",        false, "MyColName"));
  EXPECT_FALSE(k[0].matchesPrefixString(false, "MyRowName", false, "My"));
  EXPECT_FALSE(k[0].matchesPrefixString(false, "My",        false, "My"));

}