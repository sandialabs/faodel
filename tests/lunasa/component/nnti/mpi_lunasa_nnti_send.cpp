// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"
#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "faodel-common/Configuration.hh"
#include "faodel-common/NodeID.hh"

#include "whookie/Server.hh"

#include "nnti/nnti.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/transport_factory.hpp"

#include <time.h>
#include <future>
#include <set>

#include <assert.h>

using namespace std;
using namespace faodel;
using namespace lunasa;

const int URL_SIZE = 128;

class LunasaSendTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size;

  nnti::transports::transport *transport = nullptr;
  NNTI_event_queue_t unexpected_eq_;
  std::promise<int> send_promise, recv_promise;
  std::future<int> send_future, recv_future;
  faodel::nodeid_t nodeid = faodel::NODE_UNSPECIFIED;

  class send_callback_functor {
  private:
    std::promise<int> &send_promise;
  public:
    send_callback_functor(std::promise<int> &send_promise_) : send_promise(send_promise_) {}

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a SEND callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif
      send_promise.set_value(1);
      return NNTI_OK;
    }
  };

  class unexpected_callback {
  private:
    nnti::transports::transport *transport = nullptr;
    std::promise<int> &recv_promise;

    NNTI_result_t recv_callback_func(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a RECV callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif

      NNTI_attrs_t nnti_attrs;
      transport->attrs(&nnti_attrs);

      DataObject memory(nnti_attrs.mtu - nnti_attrs.max_eager_size,
                        nnti_attrs.max_eager_size,
                        DataObject::AllocatorType::eager);
      NNTI_buffer_t rdma_buffer;
      uint32_t rdma_offset = 0;

      std::queue<DataObject::rdma_segment_desc> rdma_segments;
      memory.GetDataRdmaHandles(rdma_segments);
      assert(rdma_segments.size() == 1);
      DataObject::rdma_segment_desc rdma_segment = rdma_segments.front();
      rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
      rdma_offset = rdma_segment.net_buffer_offset;

      NNTI_event_t e;
      transport->next_unexpected(rdma_buffer, rdma_offset, &e);

      char *msg = ((char *) e.start + e.offset);
      int length = (*msg++ & 0xff) << 8;
      length |= (*msg++ & 0xff);
      assert(length>0);
      int seed = (*msg++ & 0xff) << 8;
      seed |= (*msg++ & 0xff);

      std::srand(seed);
      for(int i = 0; i<length; i++) {
        char expected = (std::rand() & 0xff);
        char received = (*msg++ & 0xff);
        assert(received == expected);
      }

      recv_promise.set_value(1);
      return NNTI_OK;
    }

  public:
    unexpected_callback(nnti::transports::transport *transport_, std::promise<int> &recv_promise_) :
            transport(transport_), recv_promise(recv_promise_) {}

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
      recv_callback_func(event, context);
      return NNTI_OK;
    }
  };

  class register_memory_func {
  private:
    nnti::transports::transport *transport = nullptr;

  public:
    register_memory_func(nnti::transports::transport *transport_) : transport(transport_) {}

    void operator()(void *base_addr, size_t length, void *&pinned) {
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
  };

  class unregister_memory_func {
  private:
    nnti::transports::transport *transport = nullptr;

  public:
    unregister_memory_func(nnti::transports::transport *transport_) : transport(transport_) {}

    void operator()(void *&pinned) {
      NNTI_buffer_t reg_buf = (NNTI_buffer_t) pinned;
      transport->unregister_memory(reg_buf);
      pinned = nullptr;
    }
  };

  virtual void SetUp() {
    send_future = send_promise.get_future();
    recv_future = recv_promise.get_future();

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    assert(2 == mpi_size);

    config = Configuration("");
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();

    assert(whookie::Server::IsRunning() && "Whookie not started before NetNnti started");
    nodeid = whookie::Server::GetNodeID();

    transport = nnti::transports::factory::get_instance(config);
    transport->start();
    nnti::datatype::nnti_event_callback recv_cb(transport, unexpected_callback(transport, recv_promise));
    NNTI_result_t result = transport->eq_create(128, NNTI_EQF_UNEXPECTED, recv_cb, nullptr, &unexpected_eq_);
    assert(result == NNTI_OK);

    lunasa::RegisterPinUnpin(register_memory_func(transport), unregister_memory_func(transport));
  }

  virtual void TearDown() {
    if(transport->initialized()) {
      transport->stop();
    }
    bootstrap::Finish();
  }
};

TEST_F(LunasaSendTest, basic) {
  int rc;

  // Currently, only works for two MPI ranks
  int partner_rank = mpi_rank ^0x1;
  faodel::nodeid_t partner_nodeid;

  MPI_Request request;
  rc = MPI_Isend((void *) &nodeid, sizeof(faodel::nodeid_t), MPI_CHAR, partner_rank, 0, MPI_COMM_WORLD, &request);
  assert(rc == MPI_SUCCESS);

  MPI_Status status;
  rc = MPI_Recv((void *) &partner_nodeid, sizeof(faodel::nodeid_t), MPI_CHAR, partner_rank, 0, MPI_COMM_WORLD, &status);
  assert(rc == MPI_SUCCESS);


  if(mpi_rank<partner_rank) {
    std::stringstream url;
    std::string server_protocol("http"); // protocol is obsolete, but still required for now. Must be http now?
    url << server_protocol << "://" << partner_nodeid.GetIP() << ":" << partner_nodeid.GetPort() << "/";

    NNTI_peer_t p;
    transport->connect(url.str().c_str(), 1000, &p);
    int payload_length = 8;

    NNTI_attrs_t nnti_attrs;
    transport->attrs(&nnti_attrs);
    DataObject memory(nnti_attrs.mtu - nnti_attrs.max_eager_size,
                      payload_length + 4,
                      DataObject::AllocatorType::eager);

    char *payload = (char *) memory.GetDataPtr();
    *payload++ = (payload_length >> 8) & 0xff;
    *payload++ = payload_length & 0xff;

    unsigned int seed = std::time(0) & 0xffff;
    *payload++ = (seed >> 8) & 0xff;
    *payload++ = seed & 0xff;

    std::srand(seed);
    for(int i = 0; i<payload_length; i++) {
      char p = std::rand() & 0xff;
      *payload++ = p;
    }

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t wid;

    NNTI_buffer_t rdma_buffer;
    uint32_t rdma_offset = 0;
    std::queue<DataObject::rdma_segment_desc> rdma_segments;
    memory.GetMetaRdmaHandles(rdma_segments);
    assert(rdma_segments.size() == 1);

    DataObject::rdma_segment_desc rdma_segment = rdma_segments.front();
    rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
    rdma_offset = rdma_segment.net_buffer_offset;

    base_wr.op = NNTI_OP_SEND;
    base_wr.flags = (NNTI_op_flags_t) (NNTI_OF_LOCAL_EVENT | NNTI_OF_ZERO_COPY);
    base_wr.trans_hdl = nnti::transports::transport::to_hdl(transport);
    base_wr.peer = p;
    base_wr.local_hdl = rdma_buffer;
    base_wr.local_offset = rdma_offset;
    base_wr.remote_hdl = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length = rdma_segment.size;
    nnti::datatype::nnti_event_callback send_callback(transport, send_callback_functor(send_promise));
    nnti::datatype::nnti_work_request wr(transport, base_wr, send_callback);

    transport->send(&wr, &wid);
    send_future.get();
  } else {
    recv_future.get();
  }

  rc = MPI_Wait(&request, &status);
  assert(rc == MPI_SUCCESS);
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int rc = RUN_ALL_TESTS();
  //cout <<"Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
  return rc;
}

