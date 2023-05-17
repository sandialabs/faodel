// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

security_bucket                       bobbucket

# Tester: Run a dedicated tester that has a resource manager tester named /
tester.rpc_tester_type                single
#tester.net.url.write_to_file          .tester-url
tester.resource_manager.type          tester
tester.resource_manager.path          /bob
tester.resource_manager.write_to_file .tester-url

# Client: Don't use a tester, just send requests
client.rpc_tester_type                 none
client.resource_manager.path           /bob/1
client.resource_manager.read_from_file .tester-url
)EOF";


class OpboxSelfSendTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  std::atomic<int> send_count;
  std::atomic<int> recv_count;
  std::promise<int> send_promise, recv_promise;
  std::future<int> send_future, recv_future;

  const int threshold = 500;

  class recv_callback {
  public:
    OpboxSelfSendTest &parent;
  public:
    recv_callback(OpboxSelfSendTest &p) : parent(p) {}

    void operator()(opbox::net::peer_ptr_t peer, message_t *message) {
      opbox::net::Attrs attrs;
      opbox::net::GetAttrs(&attrs);

      char *payload = (char *) message;
      uint32_t seed = *(uint32_t *) (payload + 4); // the salt

      uLong crc = crc32(0L, Z_NULL, 0);
      crc = crc32(crc, ((Bytef *) payload) + 4, attrs.max_eager_size - 4); // the checksum

      fprintf(stderr, "receiver: seed=0x%x  payload[0]=0x%08x  crc=0x%08x\n", seed, *(uint32_t *) payload,
              (uint32_t) crc);

      if(*(uint32_t *) payload != crc) {
        fprintf(stderr, "receiver: crc mismatch (expected=0x%08x  actual=0x%08x)\n", *(uint32_t *) payload,
                (uint32_t) crc);
      }
      EXPECT_EQ(*(uint32_t *) payload, crc);

      parent.recv_count++;
      if(parent.recv_count == parent.threshold) {
        parent.recv_promise.set_value(1);
      }
      return;
    }
  };

  class send_callback {
  public:
    OpboxSelfSendTest &parent;
  public:
    send_callback(OpboxSelfSendTest &p) : parent(p) {}

    WaitingType operator()(OpArgs *args) {
      parent.send_count++;
      if(parent.send_count == parent.threshold) {
        parent.send_promise.set_value(1);
      }
      return opbox::WaitingType::done_and_destroy;
    }
  };

  void SetUp() override {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    root_rank = 0;

    opbox::net::RegisterRecvCallback(recv_callback(*this));
    bootstrap::Start();

    send_count = 0;
    recv_count = 0;

    recv_future = recv_promise.get_future();
    send_future = send_promise.get_future();
  }

  virtual void TearDown() {
  }
};

TEST_F(OpboxSelfSendTest, start1) {
  int rc;

  std::cout << "Our MPI rank is " << mpi_rank << std::endl;

  faodel::nodeid_t myid = opbox::GetMyID();
  std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);

  opbox::net::peer_t *peer;
  rc = opbox::net::Connect(&peer, myid);
  EXPECT_EQ(rc, 0);

  for(int i = 0; i<threshold; i++) {
    DataObject ldo = opbox::net::NewMessage(attrs.max_eager_size);

    char *payload = ldo.GetDataPtr<char *>();
    uint32_t seed = i + 1;
    *(uint32_t *) (payload + 4) = (uint32_t) seed;  // the salt
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, ((Bytef *) payload) + 4, ldo.GetDataSize() - 4); // the checksum
    *(uint32_t *) payload = 0;
    *(uint32_t *) payload = crc;

    fprintf(stderr, "sender: seed=0x%x  payload[0]=0x%08x  crc=0x%08x\n", seed, *(uint32_t *) payload, (uint32_t) crc);

    opbox::net::SendMsg(peer, std::move(ldo), send_callback(*this));
  }

  send_future.get(); // wait for all sends to complete
  recv_future.get(); // wait for all receives to complete

  std::cout << "send_count == " << send_count.load() << std::endl;
  std::cout << "recv_count == " << recv_count.load() << std::endl;
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  Configuration conf(default_config_string);
  conf.AppendFromReferences();
  if(argc>1) {
    if(string(argv[1]) == "-v") {
      conf.Append("loglevel all");
    } else if(string(argv[1]) == "-V") {
      conf.Append("loglevel all\nnssi_rpc.loglevel all");
    }
  }
  conf.Append("node_role", (mpi_rank == 0) ? "tester" : "target");
  bootstrap::Init(conf, opbox::bootstrap);

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
