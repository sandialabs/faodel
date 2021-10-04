// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
//#include <mpi.h>
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <sys/time.h>
#include <map>
#include <array>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config = R"EOF(

default.kelpie.core_type nonet

#lkv settings for the server
server.mutex_type   rwlock

lunasa.eager_memory_manager tcmalloc
node_role server
)EOF";

#define BUFFER_LENGTH 32
#define NUM_THREADS 16

typedef struct {
  double elapsed_time;
  long num_operations;
  long total_bytes_used;
} test_results_t;

typedef array<map<string, test_results_t>, NUM_THREADS> alloc_test_results_t;
typedef array<map<string, test_results_t>, NUM_THREADS> preload_test_results_t;

static int ops = 10000;

// !!!! TODO: these are possibly useful elsewhere, probably belongs in separate file
static const char *mem_suffixes[] = {"bytes", "KB", "MB", "GB"};

static void memory_text(unsigned long num_bytes, char *buffer, char length) {
  int i = 0;
  double text_bytes = (double) num_bytes;
  for(i = 0; i<4; i++) {
    if(text_bytes<1024) {
      break;
    } else {
      text_bytes /= 1024;
    }
  }

  snprintf(buffer, length, "%.2f %s", text_bytes, mem_suffixes[i]);
}

static const char *time_suffixes[] = {"s", "ms", "us", "ns"};

static void time_text(double num_seconds, char *buffer, char length) {
  int i = 0;
  for(i = 0; i<4; i++) {
    if(num_seconds>=1.0) {
      break;
    } else {
      num_seconds *= 1000;
    }
  }

  snprintf(buffer, length, "%.2f %s", num_seconds, time_suffixes[i]);
}

int get_thread_id(void) {
  /* The existing methods of thread identification (e.g., pthread_self() and gettid()) don't
     allow for the programmatic identification of a single thread (e.g., if( my_id == 0)).  
     The identifier generated by this function is guaranteed to assign each thread a unique 
     identifier in the range [0, NUM_THREADS]. */
  static pthread_mutex_t id_mutex;
  static int global_id = 0;

  pthread_mutex_init(&id_mutex, NULL);
  pthread_mutex_lock(&id_mutex);
  int id = global_id++;
  pthread_mutex_unlock(&id_mutex);
  return id;
}


class LunasaThreadedAllocTest : public testing::Test {
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

  pthread_t threads[NUM_THREADS];
  alloc_test_results_t alloc_test_results;
  preload_test_results_t preload_test_results;
};

void *fixed_allocation(void *arg, unsigned long allocation_size) {
  int i = 0;

  /* Admittedly, this is a bit of an odd construction.  It's left over from a previous
     version of this function.  Retaining a 0-terminated list of allocation sizes 
     preserves some flexibility if/when we ever need it. [snl] */
  unsigned long num_bytes[] = {allocation_size, 0};
  map<string, test_results_t> *results = (map<string, test_results_t> *) arg;

  DataObject *allocs = new DataObject[ops];
  //char buffer[BUFFER_LENGTH];
  test_results_t r;

  while(num_bytes[i]>0) {

    // ALLOCATION test
    clock_t t0 = clock();
    for(int j = 0; j<ops; j++) {
      allocs[j] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
    }
    clock_t t1 = clock();
    double t = (t1 - t0)/(double) CLOCKS_PER_SEC;

    r.elapsed_time = t;
    r.num_operations = ops;
    r.total_bytes_used = ops*num_bytes[i];
    results->insert(pair<string, test_results_t>(string("allocation test"), r));

    // DE-ALLOCATION test
    t0 = clock();
    for(int j = 0; j<ops; j++) {
      allocs[j] = DataObject();
    }
    t1 = clock();
    t = (t1 - t0)/(double) CLOCKS_PER_SEC;

    r.elapsed_time = t;
    r.num_operations = ops;
    r.total_bytes_used = ops*num_bytes[i];
    results->insert(pair<string, test_results_t>(string("deallocation test"), r));

    // RE-ALLOCATION test
    t0 = clock();
    for(int j = 0; j<ops; j++) {
      allocs[j] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
    }
    t1 = clock();
    t = (t1 - t0)/(double) CLOCKS_PER_SEC;

    r.elapsed_time = t;
    r.num_operations = ops;
    r.total_bytes_used = ops*num_bytes[i];
    results->insert(pair<string, test_results_t>(string("reallocation test"), r));

    // RE-DEALLOCATION test
    t0 = clock();
    for(int j = 0; j<ops; j++) {
      allocs[j] = DataObject();
    }
    t1 = clock();
    t = (t1 - t0)/(double) CLOCKS_PER_SEC;

    r.elapsed_time = t;
    r.num_operations = ops;
    r.total_bytes_used = ops*num_bytes[i];
    results->insert(pair<string, test_results_t>(string("re-deallocation test"), r));

    i++;
  }

  delete[] allocs;
  // !!!! TODO : check pthread documentation to see whether this is a necessary return value
  return nullptr;
}

void *fixed_allocation_small(void *arg) {
  unsigned long size = 2*1024;
  return fixed_allocation(arg, size);
}

void *fixed_allocation_large(void *arg) {
  unsigned long size = 8*1024*1024;
  return fixed_allocation(arg, size);
}

void *random_allocation(void *arg) {
  int i = 0;
  DataObject *allocs = new DataObject[ops];
  char buffer[BUFFER_LENGTH];
  test_results_t r;

  srand((int) clock());

  unsigned long num_bytes[ops];
  long sum = 0;
  for(int i = 0; i<ops; i++) {
    num_bytes[i] = rand()%(2*1024*1024);
    sum += num_bytes[i];
  }

  map<string, test_results_t> *results = (map<string, test_results_t> *) arg;

  // ALLOCATION test
  clock_t t0 = clock();
  for(int j = 0; j<ops; j++) {
    allocs[j] = DataObject(0, num_bytes[j], DataObject::AllocatorType::eager);
  }
  clock_t t1 = clock();
  double t = (t1 - t0)/(double) CLOCKS_PER_SEC;

  r.elapsed_time = t;
  r.num_operations = ops;
  r.total_bytes_used = ops*num_bytes[i];
  results->insert(pair<string, test_results_t>(string("allocation test"), r));

  // DE-ALLOCATION test
  t0 = clock();
  for(int j = 0; j<ops; j++) {
    allocs[j] = DataObject();
  }
  t1 = clock();
  t = (t1 - t0)/(double) CLOCKS_PER_SEC;

  r.elapsed_time = t;
  r.num_operations = ops;
  r.total_bytes_used = ops*num_bytes[i];
  results->insert(pair<string, test_results_t>(string("deallocation test"), r));

  // RE-ALLOCATION test
  t0 = clock();
  for(int j = 0; j<ops; j++) {
    allocs[j] = DataObject(0, num_bytes[j], DataObject::AllocatorType::eager);
  }
  t1 = clock();
  t = (t1 - t0)/(double) CLOCKS_PER_SEC;

  r.elapsed_time = t;
  r.num_operations = ops;
  r.total_bytes_used = ops*num_bytes[i];
  results->insert(pair<string, test_results_t>(string("reallocation test"), r));

  // RE-DEALLOCATION test
  t0 = clock();
  for(int j = 0; j<ops; j++) {
    allocs[j] = DataObject();
  }
  t1 = clock();

  r.elapsed_time = t;
  r.num_operations = ops;
  r.total_bytes_used = ops*num_bytes[i];
  results->insert(pair<string, test_results_t>(string("re-deallocation test"), r));
  i++;

  delete[] allocs;
  // !!!! TODO : check pthread documentation
  return nullptr;
}

void process_alloc_results(alloc_test_results_t &results, string prefix, string key) {
  char buffer[BUFFER_LENGTH];
  double total_time = 0.0;
  long total_operations = 0;
  long total_bytes = 0;
  for(long unsigned int i = 0; i<results.size(); i++) {
    test_results_t r = results[i].at(key);
    total_time += r.elapsed_time;
    total_operations += r.num_operations;
    total_bytes += r.total_bytes_used;
  }

  /* Write the results to stdout */
  time_text(total_time, buffer, BUFFER_LENGTH);
  printf("%-18s total time: %9s", prefix.c_str(), buffer);
  time_text(total_time/total_operations, buffer, BUFFER_LENGTH);
  printf(" / avg time: %9s", buffer);
  memory_text(total_bytes/total_operations, buffer, BUFFER_LENGTH);
  printf(" / avg allocation size: %8s\n", buffer);
}

TEST_F(LunasaThreadedAllocTest, SmallFixedSizeAllocations) {
  cout << "========= FIXED ALLOCATION test (small / " << NUM_THREADS << " threads) ===========" << endl;
  for(int i = 0; i<NUM_THREADS; i++) {
    alloc_test_results[i].clear();
  }

  /* SPAWN threads and RUN the tests */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, fixed_allocation_small, (void **) &alloc_test_results[i]);
  }

  /* WAIT for threads to finish */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // ALLOCATION test
  process_alloc_results(alloc_test_results, string("[allocation]"), string("allocation test"));

  // DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[deallocation]"), string("deallocation test"));

  // REALLOCATION test
  process_alloc_results(alloc_test_results, string("[reallocation]"), string("reallocation test"));

  // RE-DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[re-deallocation]"), string("re-deallocation test"));
}

TEST_F(LunasaThreadedAllocTest, LargeFixedSizeAllocations) {
  cout << "========= FIXED ALLOCATION test (large / " << NUM_THREADS << " threads) ===========" << endl;
  for(int i = 0; i<NUM_THREADS; i++) {
    alloc_test_results[i].clear();
  }

  /* SPAWN threads and RUN the tests */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, fixed_allocation_large, (void **) &alloc_test_results[i]);
  }

  /* WAIT for threads to finish */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // ALLOCATION test
  process_alloc_results(alloc_test_results, string("[allocation]"), string("allocation test"));

  // DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[deallocation]"), string("deallocation test"));

  // REALLOCATION test
  process_alloc_results(alloc_test_results, string("[reallocation]"), string("reallocation test"));

  // RE-DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[re-deallocation]"), string("re-deallocation test"));
}

TEST_F(LunasaThreadedAllocTest, RandomSizeAllocations) {
  cout << "============= RANDOM ALLOCATION test (" << NUM_THREADS << " threads) ===============" << endl;
  for(int i = 0; i<NUM_THREADS; i++) {
    alloc_test_results[i].clear();
  }

  /* SPAWN threads and RUN the tests */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, random_allocation, (void **) &alloc_test_results[i]);
  }

  /* WAIT for threads to finish */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // ALLOCATION test
  process_alloc_results(alloc_test_results, string("[allocation]"), string("allocation test"));

  // DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[deallocation]"), string("deallocation test"));

  // REALLOCATION test
  process_alloc_results(alloc_test_results, string("[reallocation]"), string("reallocation test"));

  // RE-DEALLOCATION test
  process_alloc_results(alloc_test_results, string("[re-deallocation]"), string("re-deallocation test"));
}

void *preloaded_allocation(void *arg) {
  unsigned long num_bytes[] = {2*1024, 0};
  unsigned long num_preloaded_allocs = 1024*1024;
  unsigned long preloaded_alloc_size = 2*1024;
  int i = 0;
  int thread_id = get_thread_id();
  test_results_t r;
  char buffer[BUFFER_LENGTH];

  map<string, test_results_t> *results = (map<string, test_results_t> *) arg;

  DataObject *preloaded_allocs = new DataObject[num_preloaded_allocs];
  DataObject *alloc = new DataObject();

  while(num_bytes[i]>0) {

    long sum = num_bytes[i]*num_preloaded_allocs;
    memory_text(sum, buffer, BUFFER_LENGTH);

    // Preload
    if(thread_id == 0) cout << "preloading..." << flush;
    for(long unsigned int j = 0; j<num_preloaded_allocs; j++) {
      preloaded_allocs[j] = DataObject(0, preloaded_alloc_size, DataObject::AllocatorType::eager);
    }
    if(thread_id == 0) cout << "done" << endl;

    // ALLOCATION test
    clock_t t0 = clock();
    for(int j = 0; j<ops; j++) {
      *alloc = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
      *alloc = DataObject();
    }
    clock_t t1 = clock();
    double t = (t1 - t0)/(double) CLOCKS_PER_SEC;
    time_text(t, buffer, BUFFER_LENGTH);
    r.elapsed_time = t;
    r.num_operations = ops;
    r.total_bytes_used = ops*num_bytes[i];
    results->insert(pair<string, test_results_t>(string("preload test"), r));
    i++;
  }

  if(thread_id == 0) cout << "cleaning up..." << flush;
  delete[] preloaded_allocs;
  delete alloc;
  if(thread_id == 0) cout << "done" << endl;
  // !!!! TODO: check pthread
  return nullptr;
}

void process_preload_results(preload_test_results_t &results, string prefix, string key) {
  char buffer[BUFFER_LENGTH];
  double total_time = 0.0;
  long total_operations = 0;
  long total_bytes = 0;
  for(long unsigned int i = 0; i<results.size(); i++) {
    test_results_t r = results[i].at(key);
    total_time += r.elapsed_time;
    total_operations += r.num_operations;
    total_bytes += r.total_bytes_used;
  }

  /* Write the results to stdout */
  time_text(total_time, buffer, BUFFER_LENGTH);
  printf("%-18s total time: %9s", prefix.c_str(), buffer);
  time_text(total_time/total_operations, buffer, BUFFER_LENGTH);
  printf(" / avg time: %9s", buffer);
  memory_text(total_bytes/total_operations, buffer, BUFFER_LENGTH);
  printf(" / avg allocation size: %8s\n", buffer);
}

TEST_F(LunasaThreadedAllocTest, PreloadedAllocations) {
  cout << "========== PRELOADED ALLOCATION test (" << NUM_THREADS << " threads) ============" << endl;
  for(int i = 0; i<NUM_THREADS; i++) {
    preload_test_results[i].clear();
  }

  /* SPAWN threads and RUN the tests */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, preloaded_allocation, (void **) &preload_test_results[i]);
  }

  /* WAIT for threads to finish */
  for(int i = 0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  process_preload_results(preload_test_results, string("[alloc/dealloc]"), string("preload test"));
}

int main(int argc, char **argv) {
  if(argc != 2) {
    cout << "[usage]: " << argv[0] << " <fixed-small|fixed-large|random|preloaded>" << endl;
    return 0;
  }

  ::testing::InitGoogleTest(&argc, argv);
  if(strcmp(argv[1], "fixed-small") == 0) {
    cout << "fixed-small" << endl;
    ::testing::GTEST_FLAG(filter) = "LunasaThreadedAllocTest.SmallFixedSizeAllocations";
  } else if(strcmp(argv[1], "fixed-large") == 0) {
    cout << "fixed-large" << endl;
    ::testing::GTEST_FLAG(filter) = "LunasaThreadedAllocTest.LargeFixedSizeAllocations";
  } else if(strcmp(argv[1], "random") == 0) {
    cout << "random" << endl;
    ::testing::GTEST_FLAG(filter) = "LunasaThreadedAllocTest.RandomSizeAllocations";
  } else if(strcmp(argv[1], "preloaded") == 0) {
    ::testing::GTEST_FLAG(filter) = "LunasaThreadedAllocTest.PreloadedAllocations";
  } else {
    cout << "ERROR: Unrecognized test (" << argv[1] << ")" << endl;
    return -1;
  }
  int rc = RUN_ALL_TESTS();
  return rc;
}

