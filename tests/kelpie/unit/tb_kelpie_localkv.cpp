// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include <vector>
#include <algorithm>
#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;


string default_config = R"EOF(

kelpie.core_type nonet
#kelpie.debug true
#kelpie.lkv.debug true
#kelpie.lkv.max_capacity 32M

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class LocalKVTest : public testing::Test {
protected:
  virtual void SetUp() {

    config.Append(default_config);
    lunasa::Init(config); //Setup lunasa
    lkv.Init(config); //Setup local kv

    DIM=32;
    for(int i=0; i<DIM; i++)
      for(int j=0; j<DIM; j++)
        ids.push_back(pair<int,int>(i,j));
    random_shuffle(ids.begin(), ids.end());

  }
  virtual void TearDown() {
    lkv.wipeAll(iuo); //Get rid of all the entries (ie, all lunasa items freed)
    lunasa::Finish();
  }

  internal_use_only_t iuo;
  vector<pair<int,int>> ids;
  int DIM;
  Configuration config;
  LocalKV lkv;


};
void setBuf(int *buf, int num, int owner, int x, int y){
  for(int i=0; i<num; i++)
    buf[i]=(x<<24) | (owner<<16) | (y<<8) | i;
}
int checkBuf(int *buf, int num, int owner, int x, int y){
  int bad=0;
  //printf("Checking for %d/ %d %d\n", num, x,y);
  for(int i=0; i<num; i++){
    //printf("Checking %d %x vs %x\n", i, buf[i], ((x<<24) | (y<<16) | i) );
    if(buf[i]!= ((x<<24) | (owner<<16) | (y<<8) | i))
      bad++;
  }
  return bad;
}

TEST_F(LocalKVTest, basics){

  int bufsize = 1024;
  bucket_t owner = bucket_t(36, iuo);
  int buf[bufsize];
  int rc;

  //Store things in random order
  random_shuffle(ids.begin(), ids.end());
  for(vector<pair<int,int>>::iterator it=ids.begin(); it!=ids.end(); ++it){
    setBuf(buf, bufsize, owner.bid, it->first, it->second);

    lunasa::DataObject ldo(0, bufsize*sizeof(int), lunasa::DataObject::AllocatorType::eager);

    memcpy(ldo.GetDataPtr(), buf, bufsize*sizeof(int));
    Key key=KeyGen<int,int>(it->first, it->second);
    rc = lkv.put(owner, key, ldo, nullptr, nullptr);
    EXPECT_EQ(KELPIE_OK,rc);
  }

  memset(buf, 4, sizeof(buf));
  //cout << "lkv is\n" << lkv.str(10); //Dumps all info about table out


  //Pull things out in a different random order
  random_shuffle(ids.begin(), ids.end());
  for(vector<pair<int,int>>::iterator it=ids.begin(); it!=ids.end(); ++it){
    size_t ret_size=0;

    rc = lkv.getData(owner, KeyGen<int,int>(it->first,it->second), (void *)buf, sizeof(buf), &ret_size, nullptr, nullptr);
    EXPECT_EQ(KELPIE_OK,rc);
    EXPECT_EQ(ret_size, sizeof(buf));

    rc = checkBuf(buf, bufsize, owner.bid, it->first, it->second);
    EXPECT_EQ(0,rc);

  }
}
TEST_F(LocalKVTest, ldoRefCount){
  //Verify that ref count goes up by one when we insert into lkv

  bucket_t owner = bucket_t(36, iuo);
  int blob_ints=1024;
  int blob_bytes=blob_ints*sizeof(int);
  int ref_count=-1;
  int rc;

  //Create an object, verify there is only one copy
  lunasa::DataObject ldo1(0, blob_bytes, lunasa::DataObject::AllocatorType::eager);

  int *x = ldo1.GetDataPtr<int *>();
  for(int i=0; i<blob_ints; i++) x[i]=i;
  ref_count = ldo1.internal_use_only.GetRefCount();          EXPECT_EQ(1, ref_count);

  //Create a second pointer to the object, verify ref count is 2
  lunasa::DataObject ldo1_copy = ldo1;
  ref_count = ldo1.internal_use_only.GetRefCount();          EXPECT_EQ(2, ref_count);
  ref_count = ldo1_copy.internal_use_only.GetRefCount();     EXPECT_EQ(2, ref_count);

  //Insert into lkv, verify ref count is 3
  rc = lkv.put(owner, Key("booya"), ldo1, nullptr,nullptr); EXPECT_EQ(KELPIE_OK,rc);
  ref_count = ldo1.internal_use_only.GetRefCount();        EXPECT_EQ(3, ref_count);
  ref_count = ldo1_copy.internal_use_only.GetRefCount();   EXPECT_EQ(3, ref_count);

  //Modify the original, should affect everyone
  x[0] = 2112;
  int *x2 = static_cast<int *>(ldo1.GetDataPtr());
  EXPECT_EQ(2112, x2[0]);
  EXPECT_EQ(x, x2); //Points should match

  //Get a reference from lkv. That makes 4 references
  lunasa::DataObject ldo3_lkv;
  rc = lkv.get(owner, Key("booya"), &ldo3_lkv, nullptr, nullptr); EXPECT_EQ(KELPIE_OK, rc);
  int *x3 = static_cast<int *>(ldo3_lkv.GetDataPtr());
  ref_count = ldo1.internal_use_only.GetRefCount();
  EXPECT_EQ(x3[0], x[0]);
  EXPECT_EQ(x, x3);
  EXPECT_EQ(4, ref_count);

  //Drop the entry for lkv.. That should free up a reference
  rc = lkv.drop(owner, Key("booya"));
  ref_count = ldo1.internal_use_only.GetRefCount();
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(3, ref_count);
}

TEST_F(LocalKVTest, access){

  bucket_t owners[] = { bucket_t(100,iuo),
                        bucket_t(101,iuo),
                        bucket_t(102,iuo),
                        BUCKET_UNSPECIFIED };

  int bufsize = 1024;
  int buf[bufsize];
  int rc;

  //Store a bunch of things with same names under different owners
  for(int i=0; owners[i]!=BUCKET_UNSPECIFIED; i++){
    for(int r=0; r<10; r++){
      for(int c=0; c<10; c++){

        lunasa::DataObject ldo(0, bufsize*sizeof(int), lunasa::DataObject::AllocatorType::eager);
        setBuf(ldo.GetDataPtr<int *>(), bufsize, owners[i].bid, r,c);

        rc = lkv.put(owners[i], KeyGen<int,int>(r,c), ldo, nullptr, nullptr);
        EXPECT_EQ(KELPIE_OK,rc);
      }
    }
  }
  //Make sure they're all ok
  for(int i=0; owners[i]!=BUCKET_UNSPECIFIED; i++){
    for(int r=0; r<10; r++){
      for(int c=0; c<10; c++){
        size_t ret_size=0;

        rc = lkv.getData(owners[i], KeyGen<int,int>(r,c), (void *)buf, sizeof(buf), &ret_size, nullptr, nullptr);
        EXPECT_EQ(KELPIE_OK,rc);
        EXPECT_EQ(ret_size, sizeof(buf));

        rc = checkBuf(buf,bufsize, owners[i].bid, r,c);
        EXPECT_EQ(0,rc);
      }
    }
  }

  //Try getting at things via different owners
  bucket_t b;
  for(int i=90; i<99; i++){
    b=bucket_t(i,iuo);
    for(int r=0; r<10; r++)
      for(int c=0; c<10; c++){
        size_t ret_size=0;
        rc = lkv.getData(b, KeyGen<int,int>(r,c), (void *)buf, sizeof(buf), &ret_size, nullptr,nullptr);
        EXPECT_EQ(KELPIE_ENOENT,rc);
      }
  }
}
