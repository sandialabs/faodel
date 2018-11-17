// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
//#include <mpi.h>
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <sys/time.h>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

lunasa.eager_memory_manager tcmalloc
node_role server
)EOF";

static const char *mem_suffixes[] = {"bytes", "KB", "MB", "GB"};
static void memory_text(unsigned long num_bytes, char *buffer, char length)
{
  int i = 0;
  double text_bytes = (double)num_bytes;
  for( i = 0; i < 4; i++ ) {
    if( text_bytes < 1024 ) {
      break;
    } else {
      text_bytes /= 1024;
    }
  }

  snprintf(buffer, length, "%.2f %s", text_bytes, mem_suffixes[i]);
}
  
static const char *time_suffixes[] = {"s", "ms", "us", "ns"};
static void time_text(double num_seconds, char *buffer, char length)
{
  int i = 0;
  for( i = 0; i < 4; i++ ) {
    if( num_seconds >= 1.0 ) {
      break;
    } else {
      num_seconds *= 1000;
    }
  }
  
  snprintf(buffer, length, "%.2f %s", num_seconds, time_suffixes[i]);
}

class LunasaStatisticsTest : public testing::Test {
protected:
  virtual void SetUp(){
    Configuration config(default_config);
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();
  }
  virtual void TearDown(){
    bootstrap::Finish();
  }

  uint64_t offset;
};

int ops = 10;
#define BUFFER_LENGTH 32 

TEST_F(LunasaStatisticsTest, FixedSizeAllocations) {
  srand ((int) clock ());

  unsigned long num_bytes[] = {2*1024, 8*1024*1024, 0};
  int i = 0;

  DataObject *allocs = new DataObject[ops];
  char buffer[BUFFER_LENGTH];
    
  EXPECT_EQ(0, Lunasa::TotalAllocated());
  while( num_bytes[i] > 0 ) {
    long sum = 0;
    // ALLOCATE
    for (int j = 0; j < ops; j ++) {
      allocs[j] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
      sum += allocs[j].GetTotalSize();
      EXPECT_EQ(sum, Lunasa::TotalAllocated());
    }

    // DE-ALLOCATE
    for (int j = 0; j < ops; j ++) {
      sum -= allocs[j].GetTotalSize();
      allocs[j] = DataObject ();
      EXPECT_EQ(sum, Lunasa::TotalAllocated());
    }
    EXPECT_EQ(0, sum);

    i++;
  }

  delete[] allocs;
}

TEST_F(LunasaStatisticsTest, RandomSizeAllocations) {
  srand ((int) clock ());

  unsigned long num_bytes[] = {2*1024, 8*1024*1024, 0};
  int i = 0;

  DataObject *allocs = new DataObject[ops];
  char buffer[BUFFER_LENGTH];
    
  for (int i = 0; i < ops; i ++)
  {
    num_bytes[i] = rand()%1048576 * sizeof (int);
  }

  EXPECT_EQ(0, Lunasa::TotalAllocated());
  long sum = 0;
  // ALLOCATE
  for (int i = 0; i < ops; i ++) {
    allocs[i] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
    sum += allocs[i].GetTotalSize();
    EXPECT_EQ(sum, Lunasa::TotalAllocated());
  }

  // DE-ALLOCATE
  for (int i = 0; i < ops; i ++) {
    sum -= allocs[i].GetTotalSize();
    allocs[i] = DataObject ();
    EXPECT_EQ(sum, Lunasa::TotalAllocated());
  }
  EXPECT_EQ(0, sum);

  delete[] allocs;
}


int main(int argc, char **argv)
{
  if( argc != 2 ) {
    cout << "[usage]: " << argv[0] << " <fixed|random>" << endl;
    return 0;
  }

  ::testing::InitGoogleTest(&argc, argv);
  if( strcmp(argv[1], "fixed") == 0 ) {
    ::testing::GTEST_FLAG(filter) = "LunasaStatisticsTest.FixedSizeAllocations";
  } else if( strcmp(argv[1], "random") == 0 ) {
    ::testing::GTEST_FLAG(filter) = "LunasaStatisticsTest.RandomSizeAllocations";
  } else {
    cout << "ERROR: Unrecognized test (" << argv[1] << ")" << endl;
  }
  int rc = RUN_ALL_TESTS();
}

