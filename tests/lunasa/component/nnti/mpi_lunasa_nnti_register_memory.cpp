// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <chrono>
#include <ratio>

#include "gtest/gtest.h"
#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "faodel-common/Configuration.hh"

#include "nnti/nnti.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/transport_factory.hpp"
#include "webhook/Server.hh"


using namespace std;
using namespace faodel;
using namespace lunasa;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config = R"EOF(
server.mutex_type rwlock
lunasa.eager_memory_manager tcmalloc
node_role server
)EOF";

nnti::transports::transport *transport = nullptr;
Configuration config;

void RegisterMemory(void *base_addr, size_t length, void *&pinned) {
  NNTI_buffer_t reg_buf;
  nnti::datatype::nnti_event_callback null_cb(transport, (NNTI_event_callback_t) 0);

  transport->register_memory(
          (char *) base_addr,
          length,
          NNTI_BF_LOCAL_WRITE,
          (NNTI_event_queue_t) 0,
          null_cb,
          nullptr,
          &reg_buf);
  pinned = (void *) reg_buf;
}

void UnregisterMemory(void *&pinned) {
  NNTI_buffer_t reg_buf = (NNTI_buffer_t) pinned;
  transport->unregister_memory(reg_buf);
  pinned = nullptr;
}

class LunasaRegistrationTest : public testing::Test {
protected:

  virtual void SetUp() {}

  virtual void TearDown() {}
};


#define NUMBER 10000
#define SIZE 8192

#define NANOSECOND(tv) ( (unsigned long)(tv.tv_sec*1000*1000*1000 + tv.tv_nsec) )

using hi_res_clock = std::chrono::high_resolution_clock;
hi_res_clock::time_point start, finish;
std::chrono::duration<double, std::nano> elapsed;


TEST_F(LunasaRegistrationTest, FixedAllocationRaw) {

  void *memory[NUMBER];
  for(int i = 0; i<NUMBER; i++) {
    memory[i] = malloc(SIZE);
  }

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  void *pinned[NUMBER];

  start = hi_res_clock::now();

  for(int i = 0; i<NUMBER; i++) {
    RegisterMemory(memory[i], SIZE, pinned[i]);
  }

  finish = hi_res_clock::now();
  elapsed = finish - start;

  printf("REGISTRATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    UnregisterMemory(pinned[i]);
  }
  finish = hi_res_clock::now();

  elapsed = (finish - start)/NUMBER;
  printf("deREGISTRATION time = %.2f us\n", elapsed.count()/1.0e3);

  for(int i = 0; i<NUMBER; i++) {
    free(memory[i]);
  }
}

TEST_F(LunasaRegistrationTest, FixedAllocationLunasa) {

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  DataObject *memory = new DataObject[NUMBER];

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    memory[i] = DataObject(0, SIZE, DataObject::AllocatorType::eager);
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("Lunasa ALLOCATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    memory[i] = DataObject();
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("Lunasa de-ALLOCATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  delete[] memory;
}

TEST_F(LunasaRegistrationTest, RepeatedAllocationRaw) {
  void *memory = malloc(SIZE);

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  void *pinned;

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    RegisterMemory(memory, SIZE, pinned);
    UnregisterMemory(pinned);
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("COMBINED time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  free(memory);
}

TEST_F(LunasaRegistrationTest, RepeatedAllocationLunasa) {

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  DataObject *memory = new DataObject();

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    *memory = DataObject(0, SIZE, DataObject::AllocatorType::eager);
    *memory = DataObject();
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("Lunasa COMBINED time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  delete memory;
}

// Want the same pseudo-random sequence for the both RANDOM allocation tests [sll]
int seed = clock();


TEST_F(LunasaRegistrationTest, RandomAllocationRaw) {
  int i;
  void *memory[NUMBER];
  size_t *num_bytes = new size_t[NUMBER];

  srand(seed);

  for(int i = 0; i<NUMBER; i++) {
    num_bytes[i] = rand()%1048576*sizeof(int);
    memory[i] = malloc(num_bytes[i]);
  }

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  void *pinned[NUMBER];
  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    RegisterMemory(memory[i], num_bytes[i], pinned[i]);
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("REGISTRATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    UnregisterMemory(pinned[i]);
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("deREGISTRATION time = %.2f us\n", elapsed.count()/1.0e3);

  for(int i = 0; i<NUMBER; i++) {
    free(memory[i]);
  }

  delete[] num_bytes;
}


TEST_F(LunasaRegistrationTest, RandomAllocationLunasa) {
  int i;
  DataObject *memory = new DataObject[NUMBER];
  size_t *num_bytes = new size_t[NUMBER];

  srand(seed);

  for(int i = 0; i<NUMBER; i++) {
    num_bytes[i] = rand()%1048576*sizeof(int);
  }

  char url[128];
  transport->get_url(&url[0], 128);
  cout << "Transport URL : " << url << endl;

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    memory[i] = DataObject(0, num_bytes[i], DataObject::AllocatorType::eager);
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("Lunasa ALLOCATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  start = hi_res_clock::now();
  for(int i = 0; i<NUMBER; i++) {
    memory[i] = DataObject();
  }
  finish = hi_res_clock::now();
  elapsed = finish - start;
  printf("Lunasa de-ALLOCATION time = %.2f us\n", elapsed.count()/1.0e3/NUMBER);

  delete[] memory;
  delete[] num_bytes;
}


int main(int argc, char *argv[]) {
  /* NOTE: it is important that these tests not all be run in a single execution of
           the test if tcmalloc is used to manage memory.  Within the constraints of
           Lunasa (i.e., managing a subset of heap memory), tcmalloc has no way of
           releasing (unregistering) registered memory.  As a result, running
           two Lunasa tests within a single test process will simply reuse memory
           that was registered in the previous test. */
  if(argc != 2) {
    cout << "[usage]: " << argv[0] << " <fixed|repeated|random>" << endl;
    return 0;
  }

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  config = Configuration(default_config);
  config.AppendFromReferences();

  bootstrap::Init(config, lunasa::bootstrap);
  bootstrap::Start();

  lunasa::RegisterPinUnpin(RegisterMemory, UnregisterMemory);

  assert(webhook::Server::IsRunning() && "Webhook not started before NetNnti started");
  faodel::nodeid_t nodeid = webhook::Server::GetNodeID();

  transport = nnti::transports::factory::get_instance(config);
  transport->start();

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  EXPECT_EQ(1, mpi_size);
  assert(1 == mpi_size);

  if(strcmp(argv[1], "fixed") == 0) {
    ::testing::GTEST_FLAG(filter) = "LunasaRegistrationTest.FixedAllocation*";
  } else if(strcmp(argv[1], "repeated") == 0) {
    ::testing::GTEST_FLAG(filter) = "LunasaRegistrationTest.RepeatedAllocation*";
  } else if(strcmp(argv[1], "random") == 0) {
    ::testing::GTEST_FLAG(filter) = "LunasaRegistrationTest.RandomAllocation*";
  } else {
    cout << "ERROR: Unrecognized test (" << argv[1] << ")" << endl;
    return 0;
  }

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  //nnti::core::logger::fini();

  MPI_Barrier(MPI_COMM_WORLD);

  if(transport->initialized()) {
    transport->stop();
  }
  bootstrap::Finish();

  MPI_Finalize();
  return (rc);
}
