// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
//#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "lunasa/common/DataObjectPacker.hh"

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


class LunasaDataObjectPacker : public testing::Test {
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
  bool ok;
  int rc;
};

const uint8_t T_CHAR = 1;
const uint8_t T_INT =2;
const uint8_t T_FLOAT = 3;
const uint8_t T_DOUBLE = 4;

void compareArrays(float *org, size_t org_bytes, float *tst, size_t tst_bytes) {
    EXPECT_EQ(org_bytes, tst_bytes);
    if(org_bytes == tst_bytes) {
      int bad_count=0;
      for(int i=0; i<org_bytes/sizeof(float); i++)
        if(org[i] != tst[i])
          bad_count++;
      EXPECT_EQ(0, bad_count);
    }
}

TEST_F(LunasaDataObjectPacker, Basics) {

  //Just some floats
  vector<string> names = { "a", "b", "c", "d", "e", "f" };
  vector<size_t> bytes = { 1024, 2048, 256, 100, 2000, 8 };
  vector<const void *> ptrs_flt;
  vector<uint8_t> types_flt;
  for(int i=0; i<names.size(); i++) {
    int num = bytes[i]/sizeof(float);
    float *fptr = new float[num];
    for(int j=0; j<num; j++)
      fptr[j]=(float)j;
    ptrs_flt.push_back(fptr);
    types_flt.push_back(T_FLOAT);
  }

  //Create the original
  DataObjectPacker gp1(names, ptrs_flt, bytes, types_flt, faodel::const_hash32("My Big Data"));

  //Create a new packer based on the ldo
  DataObjectPacker gp2(gp1.GetDataObject());

  EXPECT_TRUE( gp2.VerifyDataType(faodel::const_hash32("My Big Data")) );
  EXPECT_FALSE( gp2.VerifyDataType(faodel::const_hash32("Some Other Data")) );


  //Get all variables
  for(int i=0; i<names.size(); i++) {
    float *org_ptr = (float *)ptrs_flt[i];
    float *tst_ptr;
    size_t tst_bytes;
    uint8_t tst_type;

    //Make sure we can extract from original
    rc = gp1.GetVarPointer( names[i], (void **) &tst_ptr, &tst_bytes, &tst_type);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(T_FLOAT, tst_type);
    compareArrays(org_ptr, bytes[i], tst_ptr, tst_bytes);

    //
    rc = gp2.GetVarPointer( names[i], (void **) &tst_ptr, &tst_bytes, &tst_type);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(T_FLOAT, tst_type);
    compareArrays(org_ptr, bytes[i], tst_ptr, tst_bytes);

  }

  //Make sure we don't get a bad one
  rc = gp2.GetVarPointer( "not-here", nullptr, nullptr, nullptr);
  EXPECT_EQ(ENOENT, rc);

}

TEST_F(LunasaDataObjectPacker, LongNames) {

  vector<string> names;
  vector<size_t> bytes;
  vector<const void *> ptrs_flt;
  vector<uint8_t> types_flt;
  for(int i=0; i<5; i++) {
    int num_words = 1024*(i+1);
    float *fptr = new float[num_words];
    for(int j=0; j<num_words; j++)
      fptr[j]=(float)(i*10000 + j);
    bytes.push_back(num_words*sizeof(float));
    ptrs_flt.push_back(fptr);
    types_flt.push_back(T_FLOAT);

    //Create names of increasing size to make sure we don't find wrong name
    string name = string(i*100,'a') +"some suffix";
    names.push_back(name);
  }



  for(int version=1; version<=2; version++) {

    DataObjectPacker gp1(names, ptrs_flt, bytes, types_flt, const_hash32("My Stuff"), version);
    DataObjectPacker gp2(gp1.GetDataObject());

    for(int i=names.size()-1; i>=0; i--) {
      float *org_ptr = (float *) ptrs_flt[i];
      float *tst_ptr;
      size_t tst_bytes;
      uint8_t tst_type;

      //Search by name
      rc = gp2.GetVarPointer(names[i], (void **) &tst_ptr, &tst_bytes, &tst_type);
      EXPECT_EQ(0, rc);
      EXPECT_EQ(T_FLOAT, tst_type);
      compareArrays((float *) ptrs_flt[i], bytes[i], tst_ptr, tst_bytes);

      tst_ptr=nullptr;
      tst_bytes=0;
      tst_type=0;

      //Search by hash
      uint32_t hash = faodel::hash32(names[i]);
      rc = gp2.GetVarPointer(hash, (void **) &tst_ptr, &tst_bytes, &tst_type);
      switch(version) {
        case 1:
          EXPECT_EQ(EINVAL, rc);
          break;
        case 2:
          EXPECT_EQ(0, rc);
          EXPECT_EQ(T_FLOAT, tst_type);
          compareArrays((float *) ptrs_flt[i], bytes[i], tst_ptr, tst_bytes);
      }
    }

    //See if we can retrieve names
    vector<string> pulled_names;
    rc = gp2.GetVarNames(&pulled_names);
    switch(version) {
      case 1:
        EXPECT_EQ(0, rc);
        EXPECT_EQ(names.size(), pulled_names.size());
        break;
      case 2:
        EXPECT_EQ(EINVAL, rc);
        EXPECT_EQ(0, pulled_names.size());
        break;
    }
  }

}
TEST_F(LunasaDataObjectPacker, AppendStyle) {

  int rc1,rc2;
  vector<string> names;
  vector<double *> ptrs;
  vector<bool> found1;
  vector<bool> found2;
  size_t max_bytes=1024;



  DataObjectPacker dop1(max_bytes, 0, 1);
  DataObjectPacker dop2(max_bytes, 0, 2);

  size_t bytes_left1=max_bytes;
  size_t bytes_left2=max_bytes;
  for(int i=0; i<20; i++) {

    string name = "thing-"+std::to_string(i);
    double *ptr = new double[8];
    for(int j=0; j<8; j++)
      ptr[j]=double(i*1000+j);
    names.push_back(name);
    ptrs.push_back(ptr);

    rc1=dop1.AppendVariable(name, (double *)ptr,8*sizeof(double), 1);
    rc2=dop2.AppendVariable(name, (double *)ptr,8*sizeof(double), 1);

    //Sizes should be our expected size, plus some rounding
    size_t s1 = dop1.ComputeEntrySize(name, 8*sizeof(double)); EXPECT_EQ(s1, 8*sizeof(double)+name.length()+1+1+2+4); //Should match struct
    size_t s2 = dop2.ComputeEntrySize(name, 8*sizeof(double)); EXPECT_EQ(s2, 8*sizeof(double)+4+4+1+3); //Should match struct

    if(bytes_left1 < s1) {  EXPECT_EQ(-1, rc1);
    } else {                EXPECT_EQ( 0, rc1); bytes_left1-=s1; }
    if(bytes_left2 < s2) {  EXPECT_EQ(-1, rc2);
    } else {                EXPECT_EQ( 0, rc2); bytes_left2-=s2; }

    found1.push_back(rc1==0);
    found2.push_back(rc2==0);
    //cout <<"Inserted ok: "<<found1[i]<<" "<<found2[i]<<endl;
  }

  for(int i=0; i<20; i++) {
    double *ptr;
    size_t bytes;
    uint8_t type;

    //Check1
    rc1 = dop1.GetVarPointer(names[i], reinterpret_cast<void **>(&(ptr)), &bytes, &type);
    if(rc1==0) {
      EXPECT_TRUE(found1[i]);
      EXPECT_EQ(sizeof(double)*8, bytes);
      EXPECT_EQ(1, type);
      if(bytes==sizeof(double)*8) {
        double *src_ptr = ptrs[i];
        bool ok=true;
        for(int j=0; j<8; j++)
          ok &= (src_ptr[j] == ptr[j]);
        EXPECT_TRUE(ok);
      }
    } else {
      EXPECT_FALSE(found1[i]);
    }

    //Check 2
    rc2 = dop2.GetVarPointer(names[i], reinterpret_cast<void **>(&(ptr)), &bytes, &type);
    if(rc2==0) {
      EXPECT_TRUE(found2[i]);
      EXPECT_EQ(sizeof(double)*8, bytes);
      EXPECT_EQ(1, type);
      if(bytes==sizeof(double)*8) {
        double *src_ptr = ptrs[i];
        bool ok=true;
        for(int j=0; j<8; j++)
          ok &= (src_ptr[j] == ptr[j]);
        EXPECT_TRUE(ok);
      }
    } else {
      EXPECT_FALSE(found2[i]);
    }

    delete[] ptrs[i];

  }


}

TEST_F(LunasaDataObjectPacker, RefCounts) {

  int count;

  //Version 1: Make sure 2 ref counts while packer alive
  DataObjectPacker dop1(1024, 0, 1);
  auto ldo1 = dop1.GetDataObject();
  count  = ldo1.internal_use_only.GetRefCount();
  EXPECT_EQ(2, count);
  //cout << "version1 refcount " << ldo1.internal_use_only.GetRefCount() << endl;

  //Version 2: Pull ldo out of packer, deallocate packer, make sure ldo exists
  DataObject ldo2;
  {
    DataObjectPacker dop2(1024,0,1);
    ldo2 = dop2.GetDataObject();
    count = ldo2.internal_use_only.GetRefCount();
    EXPECT_EQ(2, count);
    //cout <<"version2a refcount " << ldo2.internal_use_only.GetRefCount() << endl;
  }
  count = ldo2.internal_use_only.GetRefCount();
  EXPECT_EQ(1, count);
  //cout <<"version2b refcount " << ldo2.internal_use_only.GetRefCount() << endl;

  //Version 3: pass ldo into packer, deallocate packer
  {
    DataObjectPacker dop3(ldo2);
    count = ldo2.internal_use_only.GetRefCount();
    EXPECT_EQ(2, count);
    //cout <<"version3a refcount is "<<count<<endl;
  }
  count = ldo2.internal_use_only.GetRefCount();
  EXPECT_EQ(1, count);
  //cout <<"version3b external refcount is "<<count<<endl;

}