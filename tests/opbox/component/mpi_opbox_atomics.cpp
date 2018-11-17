// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


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


void calc_crc(char *base, uint32_t length, uint32_t seed) {
  *(uint32_t *) (base + 4) = (uint32_t) seed;  // the salt

  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef *) (base + 4), length - 4); // the checksum
  *(uint32_t *) base = 0;
  *(uint32_t *) base = crc;

  fprintf(stderr, "sender:   length=%u seed=0x%x  base[0]=0x%08x  crc=0x%08x\n", length, seed, *(uint32_t *) base,
          (uint32_t) crc);
}

void verify_crc(char *base, uint32_t length) {
  uint32_t seed = *(uint32_t *) (base + 4); // the salt

  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (Bytef *) (base + 4), length - 4); // the checksum

  fprintf(stderr, "receiver: length=%u seed=0x%x  payload[0]=0x%08x  crc=0x%08x\n", length, seed, *(uint32_t *) base,
          (uint32_t) crc);

  if(*(uint32_t *) base != crc) {
    fprintf(stderr, "receiver: crc mismatch (expected=0x%08x  actual=0x%08x)\n", *(uint32_t *) base, (uint32_t) crc);
  }
  EXPECT_EQ(*(uint32_t *) base, crc);
}


class OpboxAtomicsTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  std::atomic<int> send_count;
  std::atomic<int> recv_count;
  std::atomic<int> atomics_count;
  std::promise<int> send_promise, recv_promise, atomics_promise;
  std::future<int> send_future, recv_future, atomics_future;

  int send_threshold;
  int recv_threshold;
  int atomics_threshold;

  DataObject atomics_ldo;

  class send_callback {
  public:
    OpboxAtomicsTest &parent_;
  public:
    send_callback(OpboxAtomicsTest &p) : parent_(p) {}

    WaitingType operator()(OpArgs *args) {
      parent_.send_count++;
      if(parent_.send_count == parent_.send_threshold) {
        parent_.send_promise.set_value(1);
      }
      return opbox::WaitingType::done_and_destroy;
    }
  };

  class atomics_callback {
  public:
    OpboxAtomicsTest &parent_;
    DataObject ldo_;
    opbox::net::peer_t *peer_;
  public:
    atomics_callback(OpboxAtomicsTest &p, DataObject ldo, opbox::net::peer_t *peer) : parent_(p), ldo_(ldo),
                                                                                      peer_(peer) {}

    WaitingType operator()(OpArgs *args) {
      int64_t *atomic_val = (int64_t*)parent_.atomics_ldo.GetDataPtr();
      fprintf(stdout, "atomics_callback() - %lx\n", *atomic_val);

      parent_.atomics_count++;
      if(parent_.atomics_count == parent_.atomics_threshold) {
        opbox::net::Attrs attrs;
        opbox::net::GetAttrs(&attrs);
        DataObject msg = opbox::net::NewMessage(attrs.max_eager_size);
        char *payload = (char *) msg.GetDataPtr();
        calc_crc(payload, msg.GetDataSize(), 40);
        opbox::net::SendMsg(peer_, msg, send_callback(parent_));

        parent_.atomics_promise.set_value(1);
      }
      return opbox::WaitingType::done_and_destroy;
    }
  };

  class recv_atomics_callback {
  public:
    OpboxAtomicsTest &parent_;
    int state;
  public:
    recv_atomics_callback(OpboxAtomicsTest &p) : parent_(p) { state = 0; }

    void FetchAdd(opbox::net::peer_ptr_t peer, opbox::net::NetBufferRemote *nbr, int64_t operand) {
      int64_t *atomic_val = (int64_t*)parent_.atomics_ldo.GetDataPtr();
      *atomic_val = 0;

      opbox::net::Atomic(
              peer,
              opbox::net::AtomicOp::FetchAdd,
              parent_.atomics_ldo,
              0,
              nbr,
              0,
              sizeof(int64_t),
              operand,
              atomics_callback(parent_, parent_.atomics_ldo, peer));
    }

    void
    CompareSwap(opbox::net::peer_ptr_t peer, opbox::net::NetBufferRemote *nbr, int64_t operand1, int64_t operand2) {
      int64_t *atomic_val = (int64_t*)parent_.atomics_ldo.GetDataPtr();
      *atomic_val = 0;

      opbox::net::Atomic(
              peer,
              opbox::net::AtomicOp::CompareSwap,
              parent_.atomics_ldo,
              0,
              nbr,
              0,
              sizeof(int64_t),
              operand1,
              operand2,
              atomics_callback(parent_, parent_.atomics_ldo, peer));
    }

    void operator()(opbox::net::peer_ptr_t peer, message_t *message) {
      opbox::net::Attrs attrs;
      opbox::net::GetAttrs(&attrs);

      char *payload = (char *) message;
      verify_crc(payload, attrs.max_eager_size);

      opbox::net::NetBufferRemote nbr;
      memcpy(&nbr, payload + 8, MAX_NET_BUFFER_REMOTE_SIZE);

      switch(state) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
          FetchAdd(
                  peer,
                  &nbr,
                  1);
          state++;
          break;
        case 5:
        case 6:
          FetchAdd(
                  peer,
                  &nbr,
                  -1);
          state++;
          break;
        case 7:
        case 8:
          FetchAdd(
                  peer,
                  &nbr,
                  1);
          state++;
          break;
        case 9:
        case 10:
          CompareSwap(
                  peer,
                  &nbr,
                  5,
                  10);
          state++;
          break;
        case 11:
          CompareSwap(
                  peer,
                  &nbr,
                  10,
                  15);
          state++;
          break;
        case 12:
          CompareSwap(
                  peer,
                  &nbr,
                  15,
                  20);
          state++;

          parent_.recv_promise.set_value(1);
          break;
      }

      parent_.recv_count++;
      return;
    }
  };

  class recv_callback {
  public:
    OpboxAtomicsTest &parent_;
  public:
    recv_callback(OpboxAtomicsTest &p) : parent_(p) {}

    void operator()(opbox::net::peer_ptr_t peer, message_t *message) {
      opbox::net::Attrs attrs;
      opbox::net::GetAttrs(&attrs);

      char *payload = (char *) message;
      verify_crc(payload, attrs.max_eager_size);

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
      opbox::net::RegisterRecvCallback(recv_atomics_callback(*this));
    } else {
      opbox::net::RegisterRecvCallback(recv_callback(*this));
    }
    bootstrap::Start();

    send_count = 0;
    recv_count = 0;
    atomics_count = 0;

    send_future = send_promise.get_future();
    recv_future = recv_promise.get_future();
    atomics_future = atomics_promise.get_future();

    atomics_ldo = DataObject(0, 5120, DataObject::AllocatorType::eager);
    int64_t *atomic_val = (int64_t*)atomics_ldo.GetDataPtr();
    *atomic_val = 0;
    memset((char*)atomic_val, 1, 5120);
  }

  void TearDown() override {
  }
};

TEST_F(OpboxAtomicsTest, start1) {

  int rc;

  faodel::nodeid_t myid = opbox::GetMyID();
  //std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);

  //    faodel::nodeid_t gather_result[mpi_size];
  std::vector<faodel::nodeid_t> gather_result(mpi_size);
  MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result.data(), sizeof(faodel::nodeid_t), MPI_CHAR,
                MPI_COMM_WORLD);

  if(mpi_rank == root_rank) {
    atomics_threshold = 13;
    send_threshold = 1;
    recv_threshold = 13;

    atomics_future.get();  // wait for all gets to complete
    send_future.get();     // wait for all receives to complete
    recv_future.get();     // wait for all atomics to complete
  } else {
    sleep(1);
    atomics_threshold = 0;
    send_threshold = 13;
    recv_threshold = 1;

    opbox::net::peer_t *peer = nullptr;
    faodel::nodeid_t root_nodeid = gather_result[root_rank];
    rc = opbox::net::Connect(&peer, root_nodeid);
    EXPECT_EQ(rc, 0);

    DataObject atomics_target(0, 5120, DataObject::AllocatorType::eager);
    memset(atomics_target.GetDataPtr(), 0, atomics_target.GetDataSize());
    calc_crc((char *) atomics_target.GetDataPtr(), atomics_target.GetDataSize(), 1);

    int64_t *atomic_val = (int64_t *) atomics_target.GetDataPtr();
    *atomic_val = 0;

    fprintf(stdout, "atomic_val=%p\n", atomic_val);
    int64_t *atomic_rewind=atomic_val-32;
    for (int iii=0;iii<128;iii++) {
      fprintf(stdout, "%lx\n", atomic_rewind[iii]);
    }

    opbox::net::NetBufferLocal *nbl = nullptr;
    opbox::net::NetBufferRemote nbr;
    uint32_t data_offset = atomics_target.GetLocalHeaderSize() + atomics_target.GetHeaderSize();
    opbox::net::GetRdmaPtr(&atomics_target, data_offset, atomics_target.GetDataSize(), &nbl, &nbr);

    DataObject ldo;
    char *payload = nullptr;

    for(int i = 0; i<13; i++) {
      ldo = opbox::net::NewMessage(attrs.max_eager_size);
      payload = (char *) ldo.GetDataPtr();
      memcpy(payload + 8, (char *) &nbr, MAX_NET_BUFFER_REMOTE_SIZE);
      calc_crc(payload, ldo.GetDataSize(), 10 + i);
      opbox::net::SendMsg(peer, ldo, send_callback(*this));
    }

    send_future.get(); // wait for all sends to complete
    recv_future.get(); // wait for all receives to complete

    EXPECT_EQ(*atomic_val, 20);
    if (*atomic_val != 20) {
      abort();
    }
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
