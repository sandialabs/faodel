// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include <zlib.h>

#include <atomic>
#include <future>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

#
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


void calc_crc(char *base, uint32_t length, uint32_t seed) {
    *(uint32_t*)(base+4) = (uint32_t)seed;  // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (Bytef*)(base+4), length-4); // the checksum
    *(uint32_t*)base = 0;
    *(uint32_t*)base = crc;

    fprintf(stderr, "sender: length=%u seed=0x%x  base[0]=0x%08x  crc=0x%08x\n", length, seed, *(uint32_t*)base, crc);
}

void verify_crc(char *base, uint32_t length) {
    uint32_t seed = *(uint32_t*)(base+4); // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (Bytef*)(base+4), length-4); // the checksum

    fprintf(stderr, "receiver: length=%u seed=0x%x  payload[0]=0x%08x  crc=0x%08x\n", length, seed, *(uint32_t*)base, crc);

    if (*(uint32_t*)base != crc) {
        fprintf(stderr, "receiver: crc mismatch (expected=0x%08x  actual=0x%08x)\n", *(uint32_t*)base, (uint32_t)crc);
    }
    EXPECT_EQ(*(uint32_t*)base, crc);
}


class OpboxPutTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;
  int root_rank;

  std::atomic<int> send_count;
  std::atomic<int> recv_count;
  std::atomic<int> put_count;
  std::promise<int> send_promise, recv_promise, put_promise;
  std::future<int>  send_future, recv_future, put_future;

  int put_threshold;
  int send_threshold;
  int recv_threshold;

  DataObject put_obj_initiator;
  DataObject put_subobj_initiator;

  class send_callback
  {
  public:
      OpboxPutTest &parent_;
  public:
      send_callback(OpboxPutTest &p): parent_(p) {}

      WaitingType operator() (OpArgs *args)
      {
          parent_.send_count++;
          if (parent_.send_count == parent_.send_threshold) {
              parent_.send_promise.set_value(1);
          }
          return opbox::WaitingType::done_and_destroy;
      }
  };
  class put_callback
  {
  public:
      OpboxPutTest &parent_;
      DataObject ldo_;
      opbox::net::peer_t  *peer_;
  public:
      put_callback(OpboxPutTest &p, DataObject ldo, opbox::net::peer_t *peer): parent_(p),ldo_(ldo),peer_(peer) {}

      WaitingType operator() (OpArgs *args)
      {
          char *payload = ldo_.GetDataPtr<char *>();
          verify_crc(payload, ldo_.GetDataSize());

          parent_.put_count++;
          if (parent_.put_count == parent_.put_threshold) {
              opbox::net::Attrs attrs;
              opbox::net::GetAttrs(attrs);
              DataObject msg = opbox::net::NewMessage(attrs.max_eager_size);
              char *payload = msg.GetDataPtr<char *>();
              calc_crc(payload, msg.GetDataSize(), 3);
              opbox::net::SendMsg(peer_, msg, send_callback(parent_));

              parent_.put_promise.set_value(1);
          }
          return opbox::WaitingType::done_and_destroy;
      }
  };
  class recv_put_callback
  {
  public:
      OpboxPutTest &parent_;
      int state;
  public:
      recv_put_callback(OpboxPutTest &p): parent_(p) { state = 0; }

      void operator() (opbox::net::peer_ptr_t peer, message_t *message)
      {
          opbox::net::Attrs attrs;
          opbox::net::GetAttrs(attrs);

          char *payload = (char *)message;
          verify_crc(payload, attrs.max_eager_size);

          opbox::net::NetBufferRemote nbr;
          memcpy(&nbr, payload+8, MAX_NET_BUFFER_REMOTE_SIZE);

          switch (state) {
              case 0:
                  parent_.put_obj_initiator = DataObject(0, 5120, DataObject::AllocatorType::eager);
                  payload = parent_.put_obj_initiator.GetDataPtr<char *>();
                  memset(payload+8, 1, 5120-8);
                  calc_crc(payload, parent_.put_obj_initiator.GetDataSize(), 2);
                  opbox::net::Put(
                      peer,
                      parent_.put_obj_initiator,
                      &nbr,
                      put_callback(parent_,parent_.put_obj_initiator,peer));
                  state++;
                  break;
              case 1:
                  parent_.put_subobj_initiator = DataObject(0, 5120, DataObject::AllocatorType::eager);
                  payload = parent_.put_subobj_initiator.GetDataPtr<char *>();
                  memset(payload+8, 1, 5120-8);
                  calc_crc(payload, parent_.put_subobj_initiator.GetDataSize(), 3);
                  /* Number of bytes from the LDO's header to its end. */
                  uint32_t header_total_size = parent_.put_subobj_initiator.GetHeaderSize() +
                                               parent_.put_subobj_initiator.GetMetaSize() +
                                               parent_.put_subobj_initiator.GetDataSize();
                  opbox::net::Put(
                      peer,
                      parent_.put_subobj_initiator,
                      0,
                      &nbr,
                      0,
                      header_total_size,
                      put_callback(parent_,parent_.put_subobj_initiator,peer));
                  state++;
                  break;
          }

          parent_.recv_count++;
          if (parent_.recv_count == parent_.recv_threshold) {
              parent_.recv_promise.set_value(1);
          }
          return;
      }
  };
  class recv_callback
  {
  public:
      OpboxPutTest &parent_;
  public:
      recv_callback(OpboxPutTest &p): parent_(p) {}

      void operator() (opbox::net::peer_ptr_t peer, message_t *message)
      {
          opbox::net::Attrs attrs;
          opbox::net::GetAttrs(attrs);

          char *payload = (char *)message;
          verify_crc(payload, attrs.max_eager_size);

          parent_.recv_count++;
          if (parent_.recv_count == parent_.recv_threshold) {
              parent_.recv_promise.set_value(1);
          }
          return;
      }
  };

  virtual void SetUp () {
      MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
      MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
      root_rank = 0;

      if(mpi_rank == root_rank){
          opbox::net::RegisterRecvCallback(recv_put_callback(*this));
      } else {
          opbox::net::RegisterRecvCallback(recv_callback(*this));
      }
      bootstrap::Start();

      send_count = 0;
      recv_count = 0;
      put_count  = 0;

      put_future  = put_promise.get_future();
      send_future = send_promise.get_future();
      recv_future = recv_promise.get_future();
  }
  virtual void TearDown () {
  }
};

TEST_F(OpboxPutTest, start1) {
    int rc;

    std::cout << "Our MPI rank is " << mpi_rank << std::endl;

    faodel::nodeid_t myid = opbox::GetMyID();
    std::cout << "Our nodeid is " << myid.GetHex() << std::endl;

    opbox::net::Attrs attrs;
    opbox::net::GetAttrs(attrs);

    //faodel::nodeid_t gather_result[mpi_size];
    std::vector< faodel::nodeid_t > gather_result( mpi_size );
    MPI_Allgather(&myid, sizeof(faodel::nodeid_t), MPI_CHAR, gather_result.data(), sizeof(faodel::nodeid_t), MPI_CHAR, MPI_COMM_WORLD);

    if(mpi_rank == root_rank){
        put_threshold  = 2;
        send_threshold = 1;
        recv_threshold = 2;

        put_future.get();  // wait for all gets to complete
        send_future.get(); // wait for all receives to complete
        recv_future.get(); // wait for all receives to complete
    } else {
        sleep(1);
        put_threshold  = 0;
        send_threshold = 2;
        recv_threshold = 1;

        opbox::net::peer_t *peer;
        faodel::nodeid_t root_nodeid = gather_result[root_rank];
        rc = opbox::net::Connect(&peer, root_nodeid);
        EXPECT_EQ(rc, 0);

        DataObject put_target(0, 5120, DataObject::AllocatorType::eager);
        memset(put_target.GetDataPtr(), 1, put_target.GetDataSize());
        calc_crc((char*)put_target.GetDataPtr(), put_target.GetDataSize(), 1);

        opbox::net::NetBufferLocal *nbl = nullptr;
        opbox::net::NetBufferRemote nbr;
        uint32_t header_offset;
        void *header_rdma_handle;
        put_target.GetHeaderRdmaHandle(&header_rdma_handle, header_offset);
        /* Number of bytes from HEADER to the end. */
        uint32_t header_total_size = put_target.GetHeaderSize() + put_target.GetMetaSize() + put_target.GetDataSize();

        opbox::net::GetRdmaPtr(&put_target, header_offset, header_total_size, &nbl, &nbr);

        DataObject ldo;
        char *payload = nullptr;

        ldo = opbox::net::NewMessage(attrs.max_eager_size);
        payload = ldo.GetDataPtr<char *>();
        memcpy(payload+8, (char*)&nbr, MAX_NET_BUFFER_REMOTE_SIZE);
        calc_crc(payload, ldo.GetDataSize(), 2);
        opbox::net::SendMsg(peer, ldo, send_callback(*this));

        ldo = opbox::net::NewMessage(attrs.max_eager_size);
        payload = ldo.GetDataPtr<char *>();
        memcpy(payload+8, (char*)&nbr, MAX_NET_BUFFER_REMOTE_SIZE);
        calc_crc(payload, ldo.GetDataSize(), 2);
        opbox::net::SendMsg(peer, ldo, send_callback(*this));

        send_future.get(); // wait for all sends to complete
        recv_future.get(); // wait for all receives to complete
    }
}

int main(int argc, char **argv){

    ::testing::InitGoogleTest(&argc, argv);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    Configuration conf(default_config_string);
    conf.AppendFromReferences();
    if(argc>1){
        if(string(argv[1])=="-v"){         conf.Append("loglevel all");
        } else if(string(argv[1])=="-V"){  conf.Append("loglevel all\nnssi_rpc.loglevel all");
        }
    }
    conf.Append("node_role", (mpi_rank==0) ? "tester" : "target");
    bootstrap::Init(conf, opbox::bootstrap);

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Barrier(MPI_COMM_WORLD);
    bootstrap::Finish();

    MPI_Finalize();
    return 0;
}
