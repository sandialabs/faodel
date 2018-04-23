// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <assert.h>
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
  /* Negative time doesn't make sense here. */
  assert(num_seconds >= 0);
  if( num_seconds > 0 ) {
    for( i = 0; i < 4; i++ ) {
      if( num_seconds >= 1.0 ) {
        break;
      } else {
        num_seconds *= 1000;
      }
    }
  }
  
  snprintf(buffer, length, "%.2f %s", num_seconds, time_suffixes[i]);
}

class LunasaAllocTest : public testing::Test {
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

static int ops = 10000;
#define BUFFER_LENGTH 32 

TEST_F(LunasaAllocTest, FixedSizedAllocations) {
  srand ((int) clock ());

  unsigned long num_bytes[] = {2*1024, 8*1024*1024, 0};
  int i = 0;

  DataObject *allocs = new DataObject[ops];
  char buffer[BUFFER_LENGTH];
    
  while( num_bytes[i] > 0 ) {

    long sum = num_bytes[i] * ops;
    memory_text(num_bytes[i], buffer, BUFFER_LENGTH);
    cerr << "====== FIXED allocation: " << buffer << " =====" << endl;

    memory_text(sum, buffer, BUFFER_LENGTH);
    cerr << "total allocated: " << buffer << " / allocations: " << ops;
    memory_text(sum/ops, buffer, BUFFER_LENGTH);
    cerr << " allocations / avg allocation size: " << buffer << endl;
  
    // ALLOCATION test
    clock_t t0 = clock ();
    for (int j = 0; j < ops; j ++) {
      allocs[j] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
    }
    clock_t t1 = clock ();
    double t = (t1 - t0) / (double)CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    cerr << "[allocation] total time: " << buffer;
    time_text(t/ops, buffer, BUFFER_LENGTH);
    cerr << " / avg time: " << buffer << endl;

    // DE-ALLOCATION test
    t0 = clock ();
    for (int j = 0; j < ops; j ++) {
      allocs[j] = DataObject ();
    }
    t1 = clock ();
    t = (t1 - t0) / (double)CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    cerr << "[deallocation] total time: " << buffer;
    time_text(t/ops, buffer, BUFFER_LENGTH);
    cerr << " / avg time: " << buffer << endl;

    // RE-ALLOCATION test
    t0 = clock ();
    for (int j = 0; j < ops; j ++) {
      allocs[j] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
    }
    t1 = clock ();
    t = (t1 - t0) / (double)CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    cerr << "[reallocation] total time: " << buffer;
    time_text(t/ops, buffer, BUFFER_LENGTH);
    cerr << " / avg time: " << buffer << endl;

    // RE-DEALLOCATION test
    t0 = clock ();
    for (int j = 0; j < ops; j ++) {
      allocs[j] = DataObject ();
    }
    t1 = clock ();
    t = (t1 - t0) / (double)CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    cerr << "[re-deallocation] total time: " << buffer;
    time_text(t/ops, buffer, BUFFER_LENGTH);
    cerr << " / avg time: " << buffer << endl;

    cerr << endl;
    i++;
  }

  delete[] allocs;
}

/* TODO: lots of overlap with preceding test... */
TEST_F(LunasaAllocTest, RandomSizedAllocations) {
  srand ((int) clock ());

  int i;
  DataObject *allocs = new DataObject[ops];
  size_t *num_bytes = new size_t[ops];

  long sum = 0;
  for (int i = 0; i < ops; i ++)
  {
    num_bytes[i] = rand()%1048576 * sizeof (int);
    sum += num_bytes[i];
  }

  cerr << "Allocating " << sum << " bytes in " << ops << " parts avg " << sum/ops << endl;

  char buffer[BUFFER_LENGTH];
    
  cerr << "====== RANDOM allocations =====" << endl;

  memory_text(sum, buffer, BUFFER_LENGTH);
  cerr << "total allocated: " << buffer << " / allocations: " << ops;
  memory_text(sum/ops, buffer, BUFFER_LENGTH);
  cerr << " allocations / avg allocation size: " << buffer << endl;

  // ALLOCATION test
  clock_t t0 = clock ();
  for (i = 0; i < ops; i ++) {
    allocs[i] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
  }

  clock_t t1 = clock ();
  double t = (t1 - t0) / (double)CLOCKS_PER_SEC;
  time_text(t, buffer, BUFFER_LENGTH);
  cerr << "[allocation] total time: " << buffer;
  time_text(t/ops, buffer, BUFFER_LENGTH);
  cerr << " / avg time: " << buffer << endl;

  // DE-ALLOCATION test
  t0 = clock ();
  for (i = 0; i < ops; i ++) {
    allocs[i] = DataObject ();
  }
  t1 = clock ();
  t = (t1 - t0) / (double)CLOCKS_PER_SEC;
  time_text(t, buffer, BUFFER_LENGTH);
  cerr << "[deallocation] total time: " << buffer;
  time_text(t/ops, buffer, BUFFER_LENGTH);
  cerr << " / avg time: " << buffer << endl;

  // RE-ALLOCATION test
  t0 = clock ();
  for (i = 0; i < ops; i ++) {
    allocs[i] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
  }
  t1 = clock ();
  t = (t1 - t0) / (double)CLOCKS_PER_SEC;
  time_text(t, buffer, BUFFER_LENGTH);
  cerr << "[reallocation] total time: " << buffer;
  time_text(t/ops, buffer, BUFFER_LENGTH);
  cerr << " / avg time: " << buffer << endl;

  // RE-DEALLOCATION test
  t0 = clock ();
  for (i = 0; i < ops; i ++) {
    allocs[i] = DataObject ();
  }
  t1 = clock ();
  t = (t1 - t0) / (double)CLOCKS_PER_SEC;
  time_text(t, buffer, BUFFER_LENGTH);
  cerr << "[re-deallocation] total time: " << buffer;
  time_text(t/ops, buffer, BUFFER_LENGTH);
  cerr << " / avg time: " << buffer << endl;

  delete[] allocs;
  delete[] num_bytes;
}

TEST_F(LunasaAllocTest, PreloadedAllocations) {
  srand ((int) clock ());

  unsigned long num_bytes[] = {2*1024, 0};
  unsigned long num_preloaded_allocs = 8*1024*1024;
  unsigned long preloaded_alloc_size = 2*1024;
  int i = 0;

  DataObject *preloaded_allocs = new DataObject[num_preloaded_allocs];
  DataObject *alloc = new DataObject();

  char buffer[BUFFER_LENGTH];
    
  while( num_bytes[i] > 0 ) {

    cerr << "====== PRELOADED allocations =====" << endl;

    long sum = num_bytes[i] * num_preloaded_allocs;
    memory_text(sum, buffer, BUFFER_LENGTH);
    cerr << "total allocated: " << buffer << " / allocations: " << num_preloaded_allocs << endl;
  
    // Preload
    cout << "preloading..." << flush;
    for (int j = 0; j < num_preloaded_allocs; j ++) {
      preloaded_allocs[j] = DataObject(0, preloaded_alloc_size, DataObject::AllocatorType::eager);
    }
    cout << "done" << endl;

    // ALLOCATION test
    clock_t t0 = clock ();
    for (int j = 0; j < ops; j ++) {
      *alloc = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
      *alloc = DataObject ();
    }
    clock_t t1 = clock ();
    double t = (t1 - t0) / (double)CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    cerr << "[allocation/deallocation] total time: " << buffer << " / ops = " << ops;
    time_text(t/ops, buffer, BUFFER_LENGTH);
    cerr << " / avg time: " << buffer << endl;
    i++;
  }

  cout << "cleaning up..." << flush;
  delete[] preloaded_allocs;
  delete alloc;
  cout << "done" << endl;
}


int main(int argc, char **argv)
{
  if( argc != 2 ) {
    cout << "[usage]: " << argv[0] << " <fixed|random|preloaded>" << endl;
    return 0;
  }

  ::testing::InitGoogleTest(&argc, argv);
  if( strcmp(argv[1], "fixed") == 0 ) {
    ::testing::GTEST_FLAG(filter) = "LunasaAllocTest.FixedSizedAllocations";
  } else if( strcmp(argv[1], "random") == 0 ) {
    ::testing::GTEST_FLAG(filter) = "LunasaAllocTest.RandomSizedAllocations";
  } else if( strcmp(argv[1], "preloaded") == 0 ) {
    ::testing::GTEST_FLAG(filter) = "LunasaAllocTest.PreloadedAllocations";
  } else {
    cout << "ERROR: Unrecognized test (" << argv[1] << ")" << endl;
  }
  int rc = RUN_ALL_TESTS();
}

