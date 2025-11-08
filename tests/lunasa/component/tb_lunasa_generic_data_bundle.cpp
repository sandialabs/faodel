// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
//#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "lunasa/common/GenericRandomDataBundle.hh"
#include "lunasa/common/GenericSequentialDataBundle.hh"


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

class LunasaGenericRandomDataBundle : public testing::Test {
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
};


struct MyEvents {
  int start_time;
  int end_time;
  int max_val;
  int min_val;
};

typedef GenericRandomDataBundle<char>     bundle_char_t;
typedef GenericRandomDataBundle<int>      bundle_int_t; //Have to provide something for meta
typedef GenericRandomDataBundle<uint64_t> bundle_uint64_t;
typedef GenericRandomDataBundle<MyEvents> bundle_myevents_t;

typedef GenericSequentialBundle<MyEvents> seq_bundle_myevents_t;

//Make sure allocations wind up being right
TEST_F(LunasaGenericRandomDataBundle, StructSizeSanityCheck) {

  //These fixed sizes are kludgy and may break on different compilers. The
  //issue is that meta must be < 64KB (thus max size is 65535). 65535 doesn't
  //pack very well, so we ditch the whole last 64b word. Thus, the max is
  //65528.
  EXPECT_EQ(65528, sizeof(bundle_char_t));
  EXPECT_EQ(65528, sizeof(bundle_int_t));
  EXPECT_EQ(65528, sizeof(bundle_uint64_t));
  EXPECT_EQ(65528, sizeof(bundle_myevents_t));


  DataObject ldo_char(sizeof(bundle_char_t),         1024, DataObject::AllocatorType::eager);
  DataObject ldo_int(sizeof(bundle_int_t),           1024, DataObject::AllocatorType::eager);
  DataObject ldo_uint64(sizeof(bundle_uint64_t),     1024, DataObject::AllocatorType::eager);
  DataObject ldo_myevents(sizeof(bundle_myevents_t), 1024, DataObject::AllocatorType::eager);

  auto *char_ptr = ldo_char.GetMetaPtr<bundle_char_t *>();             char_ptr->Init();
  auto *int_ptr = ldo_int.GetMetaPtr<bundle_int_t *>();                int_ptr->Init();
  auto *uint64_ptr = ldo_uint64.GetMetaPtr<bundle_uint64_t *>();       uint64_ptr->Init();
  auto *myevents_ptr = ldo_myevents.GetMetaPtr<bundle_myevents_t *>(); myevents_ptr->Init();

  int EXP_SPACE = (65536  //2^16 is our max size, but we use the value 0, so 65535 is largest
                   - 8    //throw away last word because 65535 causes problems
                   - 8    //Length+pads eats a word
                );
  EXPECT_EQ(EXP_SPACE-2,  2*char_ptr->GetMaxItems());       //Char eats one, and we need one for pad
  EXPECT_EQ(EXP_SPACE-4,  2*int_ptr->GetMaxItems());        //Int eats four but we don't need padding
  EXPECT_EQ(EXP_SPACE-8,  2*uint64_ptr->GetMaxItems());     //64b eats eight, but no alignment
  EXPECT_EQ(EXP_SPACE-16, 2*myevents_ptr->GetMaxItems());   //Struct eats 16 bytes, no alignment


  //Get the data section of each ldo so we can peek at the same spot in different ways
  auto *char_data     = ldo_char.GetDataPtr<char *>();
  auto *int_data      = ldo_int.GetDataPtr<char *>();
  auto *uint64_data   = ldo_uint64.GetDataPtr<char *>();
  auto *myevents_data = ldo_myevents.GetDataPtr<char *>();

  //Write a val into the packed_data, and verify it shows up in data section. Just verifying two are aligned right
  char_ptr->packed_data[0]=0x72;     EXPECT_EQ(0x72, char_data[0]);
  int_ptr->packed_data[0]=0x73;      EXPECT_EQ(0x73, int_data[0]);
  uint64_ptr->packed_data[0]=0x74;   EXPECT_EQ(0x74, uint64_data[0]);
  myevents_ptr->packed_data[0]=0x75; EXPECT_EQ(0x75, myevents_data[0]);

}


TEST_F(LunasaGenericRandomDataBundle, PackBinData) {

  int NUM_INSERTS=100;

  DataObject ldo_myevents(sizeof(bundle_myevents_t), 2*1024*1024, DataObject::AllocatorType::eager);

  auto *myevents = ldo_myevents.GetMetaPtr<bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);
  EXPECT_EQ(0, counters.current_byte_offset);
  EXPECT_EQ(0, counters.current_id);
  EXPECT_EQ(2*1024*1024, counters.max_payload_bytes);

  int expected_payload=0;
  for(int i=0; i<NUM_INSERTS; i++){
    expected_payload+=i;
    if(i==0) {
      ok = myevents->AppendBack(counters, nullptr, 0);  //Null data
      EXPECT_TRUE(ok);
    } else {
      unsigned char *x = new unsigned char[i];
      for(uint16_t j=0; j<i; j++) {
        x[j] = j & 0x0FF;
      }
      ok = myevents->AppendBack(counters, x, i);
      delete[] x;
      EXPECT_TRUE(ok);
    }
  }
  EXPECT_EQ(NUM_INSERTS, counters.current_id);
  EXPECT_EQ(expected_payload, counters.current_byte_offset);

  bundle_offsets_t counters2(&ldo_myevents);
  for(int i=0; i<NUM_INSERTS; i++){
    uint16_t len=1000;
    unsigned char *y;
    ok = myevents->GetNext(counters2, &y, &len);
    EXPECT_TRUE(ok);

    if(ok){
      if(i==0) {
        EXPECT_EQ(0, len);
        EXPECT_EQ(nullptr, y);
      } else {
        bool ok=true;
        for(int j=0; (j<i)&&(ok); j++) {
          ok = (y[j] == j);
        }
        EXPECT_TRUE(ok);
      }
    }

  }
}
TEST_F(LunasaGenericRandomDataBundle, PayloadCapacityCheck) {

  const int LINE_SIZE=100;
  const int NUM_INSERTS=10;

  DataObject ldo_myevents(sizeof(bundle_myevents_t), LINE_SIZE*NUM_INSERTS, DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  unsigned char x[LINE_SIZE];
  for(unsigned int j=0; j<LINE_SIZE; j++) x[j] = j;

  //Fill exactly to payload capacity
  for(int i=0; i<NUM_INSERTS; i++){
    ok = myevents->AppendBack(counters, x, LINE_SIZE);
    EXPECT_TRUE(ok);
  }

  //Should be full. See if we can append
  ok = myevents->AppendBack(counters, x, 1);
  EXPECT_FALSE(ok);
}

TEST_F(LunasaGenericRandomDataBundle, HeaderCapacityCheck) {

  DataObject ldo_myevents(sizeof(bundle_myevents_t), 1024*1024, DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  int max_items = myevents->GetMaxItems();

  unsigned char x=100;
  for(int i=0; i< max_items; i++){
    ok = myevents->AppendBack(counters, &x, 1);
    EXPECT_TRUE(ok);
  }

  ok = myevents->AppendBack(counters, &x, 1);
  EXPECT_FALSE(ok);

}

TEST_F(LunasaGenericRandomDataBundle, Strings) {

  DataObject ldo_myevents(sizeof(bundle_myevents_t), 1024*1024, DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  string strings[] = { "This is a test that stores strings", "donut time", "this is sophisticated text", ""};

  int in_count=0;
  for(int i=0; i<10; i++) {
    for(int j=0; strings[j]!=""; j++) {
      ok = myevents->AppendBack(counters, strings[j] + to_string(i));
      EXPECT_TRUE(ok);
      in_count++;
    }
  }

  int out_count=0;
  bundle_offsets_t counters2(&ldo_myevents);
  for(int i=0; i<10; i++) {
    for(int j=0; strings[j]!=""; j++) {
      string s;
      ok = myevents->GetNext(counters2, &s);
      EXPECT_TRUE(ok);
      EXPECT_EQ(strings[j]+to_string(i), s);
      out_count++;
    }
  }
  EXPECT_EQ(in_count, out_count);

  string s2;
  ok = myevents->GetNext(counters2, &s2);
  EXPECT_FALSE(ok);
}



class LunasaGenericSequentialDataBundle : public testing::Test {
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
};

TEST_F(LunasaGenericSequentialDataBundle, PackBinData) {

  int NUM_INSERTS=100;

  DataObject ldo_myevents(sizeof(seq_bundle_myevents_t), 2*1024*1024, DataObject::AllocatorType::eager);

  auto *myevents = ldo_myevents.GetMetaPtr<seq_bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);
  EXPECT_EQ(0, counters.current_byte_offset);
  EXPECT_EQ(0, counters.current_id);
  EXPECT_EQ(2*1024*1024, counters.max_payload_bytes);

  int expected_payload=0;
  for(int i=0; i<NUM_INSERTS; i++){
    expected_payload+= i + sizeof(uint32_t); //Data plus length
    if(i==0) {
      ok = myevents->AppendBack(counters, nullptr, 0);  //Null data
      EXPECT_TRUE(ok);
    } else {
      unsigned char *x = new unsigned char[i];
      for(uint16_t j=0; j<i; j++) {
        x[j] = j & 0x0FF;
      }
      ok = myevents->AppendBack(counters, x, i);
      delete[] x;
      EXPECT_TRUE(ok);
    }
  }
  EXPECT_EQ(NUM_INSERTS, counters.current_id);
  EXPECT_EQ(expected_payload, counters.current_byte_offset);

  bundle_offsets_t counters2(&ldo_myevents);
  for(int i=0; i<NUM_INSERTS; i++){
    uint32_t len=1000;
    unsigned char *y;
    ok = myevents->GetNext(counters2, &y, &len);
    EXPECT_TRUE(ok);

    if(ok){
      if(i==0) {
        EXPECT_EQ(0, len);
        EXPECT_EQ(nullptr, y);
      } else {
        bool ok=true;
        for(int j=0; (j<i)&&(ok); j++) {
          ok = (y[j] == j);
        }
        EXPECT_TRUE(ok);
      }
    }

  }
}

TEST_F(LunasaGenericSequentialDataBundle, PayloadCapacityCheck) {

  const int LINE_SIZE=100;
  const int NUM_INSERTS=10;

  DataObject ldo_myevents(sizeof(seq_bundle_myevents_t),
                          (sizeof(uint32_t)+LINE_SIZE)*NUM_INSERTS,
                          DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<seq_bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  unsigned char x[LINE_SIZE];
  for(unsigned int j=0; j<LINE_SIZE; j++) x[j] = j;

  //Fill exactly to payload capacity
  for(int i=0; i<NUM_INSERTS; i++){
    ok = myevents->AppendBack(counters, x, LINE_SIZE);
    EXPECT_TRUE(ok);
  }

  //Should be full. See if we can append
  ok = myevents->AppendBack(counters, x, 1);
  EXPECT_FALSE(ok);
}

TEST_F(LunasaGenericSequentialDataBundle, HeaderCapacityCheck) {

  //GSB doesn't have the item limit that GDB does. Make sure we can stuff it w/ more than 64K entries
  const int MAX_ITEMS=(65*1024); //Something bigger than meta

  DataObject ldo_myevents(sizeof(seq_bundle_myevents_t),
                          (MAX_ITEMS*sizeof(uint32_t)),
                          DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<seq_bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  unsigned char x=100;
  for(int i=0; i< MAX_ITEMS; i++){
    ok = myevents->AppendBack(counters, &x, 0);
    EXPECT_TRUE(ok);
  }

  ok = myevents->AppendBack(counters, &x, 1);
  EXPECT_FALSE(ok);

}
TEST_F(LunasaGenericSequentialDataBundle, Strings) {

  DataObject ldo_myevents(sizeof(seq_bundle_myevents_t), 1024*1024, DataObject::AllocatorType::eager);
  auto *myevents = ldo_myevents.GetMetaPtr<seq_bundle_myevents_t *>();
  myevents->Init();

  bundle_offsets_t counters(&ldo_myevents);

  string strings[] = { "This is a test that stores strings", "donut time", "this is sophisticated text", ""};

  int in_count=0;
  for(int i=0; i<10; i++) {
    for(int j=0; strings[j]!=""; j++) {
      ok = myevents->AppendBack(counters, strings[j] + to_string(i));
      EXPECT_TRUE(ok);
      in_count++;
    }
  }

  int out_count=0;
  bundle_offsets_t counters2(&ldo_myevents);
  for(int i=0; i<10; i++) {
    for(int j=0; strings[j]!=""; j++) {
      string s;
      ok = myevents->GetNext(counters2, &s);
      EXPECT_TRUE(ok);
      EXPECT_EQ(strings[j]+to_string(i), s);
      out_count++;
    }
  }
  EXPECT_EQ(in_count, out_count);

  string s2;
  ok = myevents->GetNext(counters2, &s2);
  EXPECT_FALSE(ok);
}
