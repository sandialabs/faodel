// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;


void calc_crc(const char* prefix, char *base, uint32_t length, uint32_t seed) {
  *(uint32_t *) (base + 4) = (uint32_t) seed;  // the salt

  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef *) (base + 4), length - 4); // the checksum
  *(uint32_t *) base = 0;
  *(uint32_t *) base = crc;

  fprintf(stderr, "%s: length=%u seed=0x%x  base[0]=0x%08x  base[%d]=0x%x  crc=0x%08x\n",
    prefix, length, seed, *(uint32_t*)base, length-4, *(uint32_t*)&base[length-4], (uint32_t)crc);
}

void verify_crc(const char* prefix, char *base, uint32_t length) {
  uint32_t seed = *(uint32_t *) (base + 4); // the salt

  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef *) (base + 4), length - 4); // the checksum

  fprintf(stderr, "%s: length=%u seed=0x%x  payload[0]=0x%08x  payload[%d]=0x%x  crc=0x%08x\n",
    prefix, length, seed, *(uint32_t*)base, length-4, *(uint32_t*)&base[length-4], (uint32_t)crc);

  EXPECT_EQ(*(uint32_t*)base, crc);
  if(*(uint32_t *) base != crc) {
    fprintf(stderr, "%s: crc mismatch (expected=0x%08x  actual=0x%08x)\n",
            prefix, *(uint32_t*)base, (uint32_t)crc);
  }
}


class OpboxGetTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  std::atomic<int> send_count;
  std::atomic<int> recv_count;
  std::atomic<int> get_count;
  std::promise<int> send_promise, recv_promise, get_promise;
  std::future<int> send_future, recv_future, get_future;

  int get_threshold;
  int send_threshold;
  int recv_threshold;

  DataObject get_obj_initiator;
  DataObject get_subobj_initiator;

  class send_callback {
  public:
    OpboxGetTest &parent_;
  public:
    send_callback(OpboxGetTest &p) : parent_(p) {}

    WaitingType operator()(OpArgs *args) {
      parent_.send_count++;
      if(parent_.send_count == parent_.send_threshold) {
        parent_.send_promise.set_value(1);
      }
      return opbox::WaitingType::done_and_destroy;
    }
  };

  class get_callback {
  public:
    OpboxGetTest &parent_;
    DataObject ldo_;
    opbox::net::peer_t *peer_;
  public:
    get_callback(OpboxGetTest &p, DataObject ldo, opbox::net::peer_t *peer) : parent_(p), ldo_(ldo), peer_(peer) {}

    WaitingType operator()(OpArgs *args) {
      char *payload = ldo_.GetDataPtr<char *>();
      verify_crc("get initiator", payload, ldo_.GetDataSize());

      parent_.get_count++;
      if(parent_.get_count == parent_.get_threshold) {
        opbox::net::Attrs attrs;
        opbox::net::GetAttrs(&attrs);
        DataObject msg = opbox::net::NewMessage(attrs.max_eager_size);
        char *payload = msg.GetDataPtr<char *>();
        calc_crc("get initiator", payload, msg.GetDataSize(), 4);
        opbox::net::SendMsg(peer_, msg, send_callback(parent_));

        parent_.get_promise.set_value(1);
      }
      return opbox::WaitingType::done_and_destroy;
    }
  };

  class recv_get_callback {
  public:
    OpboxGetTest &parent_;
    int state;
  public:
    recv_get_callback(OpboxGetTest &p) : parent_(p) { state = 0; }

    void operator()(opbox::net::peer_ptr_t peer, message_t *message) {
      opbox::net::Attrs attrs;
      opbox::net::GetAttrs(&attrs);

      char *payload = (char *) message;
      verify_crc("receiver", payload, attrs.max_eager_size);

      opbox::net::NetBufferRemote nbr;
      memcpy(&nbr, payload + 8, MAX_NET_BUFFER_REMOTE_SIZE);

      switch(state) {
        case 0:
          parent_.get_obj_initiator = DataObject(0, 5120, DataObject::AllocatorType::eager);
          memset(parent_.get_obj_initiator.GetDataPtr(), 6, parent_.get_obj_initiator.GetDataSize());
          opbox::net::Get(
                  peer,
                  &nbr,
                  parent_.get_obj_initiator,
                  get_callback(parent_, parent_.get_obj_initiator, peer));
          state++;
          break;
        case 1:
          parent_.get_subobj_initiator = DataObject(0, 5120, DataObject::AllocatorType::eager);
          memset(parent_.get_subobj_initiator.GetDataPtr(), 8, parent_.get_subobj_initiator.GetDataSize());
          uint32_t length = parent_.get_subobj_initiator.GetHeaderSize() +
                            parent_.get_subobj_initiator.GetMetaSize() +
                            parent_.get_subobj_initiator.GetDataSize();
//                  opbox::net::Get(
//                      peer,
//                      &nbr,
//                      parent_.get_subobj_initiator,
//                      get_callback(parent_,parent_.get_subobj_initiator,peer));
          opbox::net::Get(
                  peer,
                  &nbr,
                  0,
                  parent_.get_subobj_initiator,
                  0,
                  length,
                  get_callback(parent_, parent_.get_subobj_initiator, peer));
          state++;
          break;
      }

      parent_.recv_count++;
      if(parent_.recv_count == parent_.recv_threshold) {
        parent_.recv_promise.set_value(1);
      }
      return;
    }
  };

  class recv_callback {
  public:
    OpboxGetTest &parent_;
  public:
    recv_callback(OpboxGetTest &p) : parent_(p) {}

    void operator()(opbox::net::peer_ptr_t peer, message_t *message) {
      opbox::net::Attrs attrs;
      opbox::net::GetAttrs(&attrs);

      char *payload = (char *) message;
      verify_crc("sender", payload, attrs.max_eager_size);

      parent_.recv_count++;
      if(parent_.recv_count == parent_.recv_threshold) {
        parent_.recv_promise.set_value(1);
      }
      return;
    }
  };

  void SetUp() override {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    root_rank = 0;

    if(mpi_rank == root_rank) {
      opbox::net::RegisterRecvCallback(recv_get_callback(*this));
    } else {
      opbox::net::RegisterRecvCallback(recv_callback(*this));
    }
    bootstrap::Start();

    send_count = 0;
    recv_count = 0;
    get_count = 0;

    get_future = get_promise.get_future();
    send_future = send_promise.get_future();
    recv_future = recv_promise.get_future();
  }

  void TearDown() override {
  }
};

TEST_F(OpboxGetTest, start1) {
  int rc;

  faodel::nodeid_t myid = opbox::GetMyID();
  std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);

  std::vector<faodel::nodeid_t> gather_result(mpi_size);
  //faodel::nodeid_t gather_result[mpi_size];
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result.data(), sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

  if(mpi_rank == root_rank) {
    get_threshold = 2;
    send_threshold = 1;
    recv_threshold = 2;

    get_future.get();  // wait for all gets to complete
    send_future.get(); // wait for all receives to complete
    recv_future.get(); // wait for all receives to complete
  } else {
    sleep(1);
    get_threshold = 0;
    send_threshold = 2;
    recv_threshold = 1;

    opbox::net::peer_t *peer;
    faodel::nodeid_t root_nodeid = gather_result[root_rank];
    rc = opbox::net::Connect(&peer, root_nodeid);
    EXPECT_EQ(rc, 0);

    DataObject get_target(0, 5120, DataObject::AllocatorType::eager);
    memset(get_target.GetDataPtr(), 1, get_target.GetDataSize());
    calc_crc("get target", (char*)get_target.GetDataPtr(), get_target.GetDataSize(), 1);

    opbox::net::NetBufferLocal *nbl = nullptr;
    opbox::net::NetBufferRemote nbr;
    opbox::net::GetRdmaPtr(&get_target, &nbl, &nbr);

    DataObject ldo;
    char *payload = nullptr;

    ldo = opbox::net::NewMessage(attrs.max_eager_size);
    payload = ldo.GetDataPtr<char *>();
    memcpy(payload + 8, (char *) &nbr, MAX_NET_BUFFER_REMOTE_SIZE);
    calc_crc("sender", payload, ldo.GetDataSize(), 2);
    opbox::net::SendMsg(peer, ldo, send_callback(*this));

    ldo = opbox::net::NewMessage(attrs.max_eager_size);
    payload = (char *) ldo.GetDataPtr();
    memcpy(payload + 8, (char *) &nbr, MAX_NET_BUFFER_REMOTE_SIZE);
    calc_crc("sender", payload, ldo.GetDataSize(), 3);
    opbox::net::SendMsg(peer, ldo, send_callback(*this));

    send_future.get(); // wait for all sends to complete
    recv_future.get(); // wait for all receives to complete
  }
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  bootstrap::Init(Configuration(""), opbox::bootstrap);

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
