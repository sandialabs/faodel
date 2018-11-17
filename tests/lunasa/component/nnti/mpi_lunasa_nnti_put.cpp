// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
#include <mpi.h>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "faodel-common/Configuration.hh"
#include "faodel-common/NodeID.hh"

#include "nnti/nnti.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/nntiConfig.h"
#include "nnti/transport_factory.hpp"
#include "webhook/Server.hh"

#include <time.h>
#include <future>
#include <set>

#include <assert.h>

using namespace std;
using namespace faodel;
using namespace lunasa;


const int URL_SIZE = 128;
char *debug_ptr = nullptr;

#if NNTI_BUILD_MPI
#define MAX_NET_BUFFER_REMOTE_SIZE 68 /* 4 + 4 + 60 */
#elif NNTI_BUILD_UGNI
#define MAX_NET_BUFFER_REMOTE_SIZE 48 /* 4 + 4 + 40 */
#elif NNTI_BUILD_IBVERBS
#define MAX_NET_BUFFER_REMOTE_SIZE 36 /* 4 + 4 + 28 */
#endif

// OVERVIEW
// This test is intended to verify that we can transmit LDOs using RDMA put operations.
// The ROOT process is the source of the put; the LEAF process is the destination of 
// the put (NOTE: in retrospect, calling them DESTINATION and SOURCE prob'ly would have made 
// more sense ;0)).  The principal mechanism that drives this test is a sequence of 
// NNTI callback functions.  The basic sequence of events is:
// 
// * [LEAF] : zero-copy SEND to ROOT advertising the HANDLE and OFFSET of the target memory
// * [ROOT] : receives message from LEAF, populates buffer using random seed and PRNG, PUTs 
//            the header of the source LDO 
// * [ROOT] : when PUT completes, the ROOT PUTs the remaining segment
// * [ROOT] : when all of the PUTs complete, the ROOT uses a zero-copy SEND to notify LEAF
//            of the random seed and that the PUT operations are complete.
// * [LEAF] : receives message from ROOT and validates the contents of the RDMA Put target

// --- DEFINE a message type for setting up the put ---
// We want a structure that is always MAX_NET_BUFFER_REMOTE_SIZE bytes long
typedef struct {
  uint32_t length;    // number of payload bytes
  uint32_t offset;    // offset in remote buffer
} start_put_message_header_t;

typedef struct {
  start_put_message_header_t header;
  char body[MAX_NET_BUFFER_REMOTE_SIZE - sizeof(start_put_message_header_t)];
} start_put_message_t;

static_assert(sizeof(start_put_message_t) == MAX_NET_BUFFER_REMOTE_SIZE, "start_put_message_t is not the correct size");

// --- DEFINE a message type for communicating that put is complete ---
typedef struct {
  uint32_t length;    // number of payload bytes in the put
  uint32_t seed;      // seed used to generate payload of the put
} completed_put_message_header_t;

typedef struct {
  completed_put_message_header_t header;
  char body[MAX_NET_BUFFER_REMOTE_SIZE - sizeof(completed_put_message_header_t)];
} completed_put_message_t;

static_assert(sizeof(completed_put_message_t) == MAX_NET_BUFFER_REMOTE_SIZE,
              "completed_put_message_t is not the correct size");

class put_status {
public:
  put_status(uint32_t remote_offset_,
             std::queue<DataObject::rdma_segment_desc> &rdma_segments_,
             NNTI_buffer_t remote_hdl_,
             uint32_t total_length_,
             uint32_t seed_,
             DataObject *ldo_) :
          remote_offset(remote_offset_),
          rdma_segments(rdma_segments_),
          remote_hdl(remote_hdl_),
          total_length(total_length_),
          seed(seed_),
          ldo(ldo_) {}

  uint32_t remote_offset;
  std::queue<DataObject::rdma_segment_desc> rdma_segments;
  NNTI_buffer_t remote_hdl;
  uint32_t total_length;
  uint32_t seed;
  DataObject *ldo;
};

void cleanup(void *addr) {
  free(addr);
}

class LunasaPutUserTest : public testing::Test {
protected:
  Configuration config;
  int mpi_rank, mpi_size, partner_rank;

  nnti::transports::transport *transport = nullptr;
  NNTI_event_queue_t unexpected_eq_;
  std::promise<int> send_promise, root_recv_promise, put_promise;
  std::future<int> send_future, root_recv_future, put_future;
  std::promise<std::pair<uint32_t, uint32_t> > leaf_recv_promise;
  std::future<std::pair<uint32_t, uint32_t> > leaf_recv_future;
  faodel::nodeid_t nodeid = faodel::NODE_UNSPECIFIED;

  class send_callback_functor_root {
  private:
    std::promise<int> &send_promise;
    DataObject *ldo;
  public:
    send_callback_functor_root(std::promise<int> &send_promise_, DataObject *ldo_) :
            send_promise(send_promise_),
            ldo(ldo_) {}

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a [root] SEND callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif
      send_promise.set_value(1);
      delete ldo;
      return NNTI_EIO;
    }
  };

  class send_callback_functor_leaf {
  private:
    std::promise<int> &send_promise;
  public:
    send_callback_functor_leaf(std::promise<int> &send_promise_) :
            send_promise(send_promise_) {}

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a [leaf] SEND callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif
      send_promise.set_value(1);
      return NNTI_EIO;
    }
  };

  class put_callback_functor {
  private:
    std::promise<int> &put_promise;
    std::promise<int> &send_promise;
    nnti::transports::transport *transport = nullptr;
    put_status *status;
  public:
    put_callback_functor(std::promise<int> &put_promise_,
                         std::promise<int> &send_promise_,
                         nnti::transports::transport *transport_,
                         put_status *status_) :
            put_promise(put_promise_),
            send_promise(send_promise_),
            transport(transport_),
            status(status_) {
    }

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a PUT callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif
      NNTI_buffer_t rdma_buffer;
      uint32_t rdma_offset = 0;

      if(status->rdma_segments.empty()) {
        NNTI_attrs_t nnti_attrs;
        transport->attrs(&nnti_attrs);
        DataObject *memory = new DataObject(nnti_attrs.mtu - nnti_attrs.max_eager_size,
                                            sizeof(completed_put_message_t),
                                            DataObject::AllocatorType::eager);
        completed_put_message_t *m = (completed_put_message_t *) memory->GetDataPtr();
        m->header.length = status->total_length;
        m->header.seed = status->seed;

        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
        NNTI_work_id_t wid;

        std::queue<DataObject::rdma_segment_desc> rdma_segments;

        memory->GetMetaRdmaHandles(rdma_segments);
        DataObject::rdma_segment_desc rdma_segment = rdma_segments.front();
        rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
        rdma_offset = rdma_segment.net_buffer_offset;

        base_wr.op = NNTI_OP_SEND;
        base_wr.flags = (NNTI_op_flags_t) (NNTI_OF_LOCAL_EVENT | NNTI_OF_ZERO_COPY);
        base_wr.trans_hdl = nnti::transports::transport::to_hdl(transport);
        base_wr.peer = event->peer;
        base_wr.local_hdl = rdma_buffer;
        base_wr.local_offset = rdma_offset;
        base_wr.remote_hdl = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length = rdma_segment.size;

        std::function<NNTI_result_t(NNTI_event_t *, void *)> cb =
                send_callback_functor_root(send_promise, memory);
        nnti::datatype::nnti_event_callback send_callback(transport, cb);
        nnti::datatype::nnti_work_request wr(transport, base_wr, send_callback);

        transport->send(&wr, &wid);
        delete status->ldo;
        delete status;
        put_promise.set_value(1);
      } else {
        /* we currently assume that there's only one user data segment. */
        assert(status->rdma_segments.size() == 1);
        DataObject::rdma_segment_desc rdma_segment = status->rdma_segments.front();
        rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
        rdma_offset = rdma_segment.net_buffer_offset;
        status->rdma_segments.pop();

        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
        NNTI_work_id_t wid;

        base_wr.op = NNTI_OP_PUT;
        base_wr.flags = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl = nnti::transports::transport::to_hdl(transport);
        base_wr.peer = event->peer;
        base_wr.local_hdl = rdma_buffer;
        base_wr.local_offset = rdma_offset;
        base_wr.remote_hdl = status->remote_hdl;
        base_wr.remote_offset = status->remote_offset;
        base_wr.length = rdma_segment.size;

        status->total_length += base_wr.length;
        status->remote_offset += base_wr.length;

        nnti::datatype::nnti_event_callback put_callback(transport,
                                                         put_callback_functor(put_promise, send_promise, transport,
                                                                              status));

        nnti::datatype::nnti_work_request wr(transport, base_wr, put_callback);
        transport->put(&wr, &wid);
      }
      return NNTI_EIO;
    }
  };

  class unexpected_callback_root {
  private:
    nnti::transports::transport *transport = nullptr;
    std::promise<int> &recv_promise;
    std::promise<int> &put_promise;
    std::promise<int> &send_promise;

    NNTI_result_t recv_callback_func(NNTI_event_t *event, void *context) {
#ifdef DEBUG
      std::cout << "This is a RECV callback function.  My parameters are event(" << (void*)event
                << ") and context(" << (void*)context << ")" << std::endl;
#endif

      /* The RECEIVED message contains the REMOTE HANDLE for the RDMA buffer */
      NNTI_attrs_t nnti_attrs;
      transport->attrs(&nnti_attrs);

      DataObject memory(nnti_attrs.mtu - nnti_attrs.max_eager_size,
                        nnti_attrs.max_eager_size,
                        DataObject::AllocatorType::eager);
      NNTI_buffer_t rdma_buffer;
      uint32_t rdma_offset = 0;
      std::queue<DataObject::rdma_segment_desc> rx_rdma_segments;
      memory.GetHeaderRdmaHandles(rx_rdma_segments);
      assert(rx_rdma_segments.size() == 1);

      DataObject::rdma_segment_desc &rx_rdma_segment = rx_rdma_segments.front();
      rdma_buffer = (NNTI_buffer_t) rx_rdma_segment.net_buffer_handle;
      rdma_offset = rx_rdma_segment.net_buffer_offset;

      NNTI_event_t e;
      transport->next_unexpected((NNTI_buffer_t) rx_rdma_segment.net_buffer_handle,
                                 rx_rdma_segment.net_buffer_offset, &e);
      start_put_message_t *m = (start_put_message_t *) memory.GetDataPtr();

      NNTI_buffer_t remote_hdl;
      transport->dt_unpack((void *) &remote_hdl, &m->body[0], m->header.length);

      int payload_length = 128;
      unsigned int seed = std::time(0) & 0xffff;
      char *payload = (char *) malloc(payload_length);
      DataObject *put_source = new DataObject(payload, 0, payload_length, cleanup);
      std::srand(seed);
      for(int i = 0; i<payload_length; i++) {
        payload[i] = (char) (std::rand() & 0xff);
      }

      /* PUT the FIRST segment */
      std::queue<DataObject::rdma_segment_desc> rdma_segments;
      put_source->GetHeaderRdmaHandles(rdma_segments);

      /* this is a user LDO, it's divided into two segments. */
      assert(rdma_segments.size() == 2);
      DataObject::rdma_segment_desc &rdma_segment = rdma_segments.front();
      rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
      rdma_offset = rdma_segment.net_buffer_offset;
      rdma_segments.pop();

      NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
      NNTI_work_id_t wid;

      base_wr.op = NNTI_OP_PUT;
      base_wr.flags = NNTI_OF_LOCAL_EVENT;
      base_wr.trans_hdl = nnti::transports::transport::to_hdl(transport);
      base_wr.peer = e.peer;
      base_wr.local_hdl = rdma_buffer;
      base_wr.local_offset = rdma_offset;
      base_wr.local_hdl = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
      base_wr.local_offset = rdma_segment.net_buffer_offset;
      base_wr.remote_hdl = remote_hdl;
      base_wr.remote_offset = m->header.offset;
      base_wr.length = rdma_segment.size;

      put_status *status = new put_status(base_wr.remote_offset + base_wr.length, rdma_segments, remote_hdl,
                                          0, seed, put_source);

      std::function<NNTI_result_t(NNTI_event_t *, void *)> cb =
              put_callback_functor(put_promise, send_promise, transport, status);
      nnti::datatype::nnti_event_callback put_callback(transport, cb);

      nnti::datatype::nnti_work_request wr(transport, base_wr, put_callback);
      transport->put(&wr, &wid);

      recv_promise.set_value(1);
      return NNTI_EIO;
    }

  public:
    unexpected_callback_root(nnti::transports::transport *transport_,
                             std::promise<int> &recv_promise_,
                             std::promise<int> &send_promise_,
                             std::promise<int> &put_promise_) :
            transport(transport_),
            recv_promise(recv_promise_),
            send_promise(send_promise_),
            put_promise(put_promise_) {}

    NNTI_result_t operator()(NNTI_event_t *event, void *context) {
      recv_callback_func(event, context);
      return NNTI_OK;
    }
  };

  class unexpected_callback_leaf {
  private:
    nnti::transports::transport *transport = nullptr;
    std::promise<std::pair<uint32_t, uint32_t> > &recv_promise;

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
      transport->next_unexpected((NNTI_buffer_t) rdma_segment.net_buffer_handle,
                                 rdma_segment.net_buffer_offset, &e);

      completed_put_message_t *m = (completed_put_message_t *) ((char *) e.start + e.offset);
      int length = m->header.length;
      int seed = m->header.seed;

      recv_promise.set_value(std::pair<uint32_t, uint32_t>(length, seed));
      return NNTI_EIO;
    }

  public:
    unexpected_callback_leaf(nnti::transports::transport *transport_,
                             std::promise<std::pair<uint32_t, uint32_t> > &recv_promise_) :
            transport(transport_),
            recv_promise(recv_promise_) {}

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
              static_cast<NNTI_buffer_flags_t>(NNTI_BF_LOCAL_READ | NNTI_BF_LOCAL_WRITE |
                                               NNTI_BF_REMOTE_READ | NNTI_BF_REMOTE_WRITE),
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
    put_future = put_promise.get_future();

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    assert(2 == mpi_size);

    // Currently, only works for two MPI ranks
    partner_rank = mpi_rank ^ 0x1;

    config = Configuration("");
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();

    assert(webhook::Server::IsRunning() && "Webhook not started before NetNnti started");
    nodeid = webhook::Server::GetNodeID();

    transport = nnti::transports::factory::get_instance(config);
    transport->start();
    std::function<NNTI_result_t(NNTI_event_t *, void *)> cb;

    if(mpi_rank<partner_rank) {
      /* ==== ROOT process ==== */
      root_recv_future = root_recv_promise.get_future();
      cb = unexpected_callback_root(transport, root_recv_promise, send_promise, put_promise);
    } else {
      /* ==== LEAF process ==== */
      leaf_recv_future = leaf_recv_promise.get_future();
      cb = unexpected_callback_leaf(transport, leaf_recv_promise);
    }
    nnti::datatype::nnti_event_callback recv_cb(transport, cb);

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

public:
  faodel::nodeid_t GetNodeID() {
    char url[URL_SIZE];
    transport->get_url(url, URL_SIZE);
    return faodel::nodeid_t(url);
  }
};

TEST_F(LunasaPutUserTest, basic) {
  int rc;

  int partner_id;
  faodel::nodeid_t partner_nodeid;

  MPI_Request request;
  rc = MPI_Isend((void *) &nodeid, sizeof(faodel::nodeid_t), MPI_CHAR, partner_rank, 0, MPI_COMM_WORLD, &request);
  assert(rc == MPI_SUCCESS);

  MPI_Status status;
  rc = MPI_Recv((void *) &partner_nodeid, sizeof(faodel::nodeid_t), MPI_CHAR, partner_rank, 0, MPI_COMM_WORLD, &status);
  assert(rc == MPI_SUCCESS);

  rc = MPI_Wait(&request, &status);
  assert(rc == MPI_SUCCESS);

  if(mpi_rank<partner_rank) {
    /* ==== ROOT process : the source of the RDMA put ==== */
    root_recv_future.get();
    put_future.get();
    send_future.get();
  } else {
    /* ==== LEAF process : the destination of the RDMA put ==== */
    std::stringstream url;
    std::string server_protocol("http"); // protocol is obsolete, but still required for now. Previously null
    url << server_protocol << "://" << partner_nodeid.GetIP() << ":" << partner_nodeid.GetPort() << "/";

    NNTI_peer_t p;
    transport->connect(url.str().c_str(), 1000, &p);

    int payload_length = 8;
    NNTI_attrs_t nnti_attrs;
    transport->attrs(&nnti_attrs);

    /* Get buffer to send RDMA information */
    DataObject put_target(0, 128, DataObject::AllocatorType::eager);
    char *payload = (char *) put_target.GetDataPtr();
    memset(payload, 0xFF, 128);

    NNTI_buffer_t rdma_put_buffer;
    uint32_t rdma_put_offset = 0;
    std::queue<DataObject::rdma_segment_desc> rdma_put_segments;
    put_target.GetHeaderRdmaHandles(rdma_put_segments);
    assert(rdma_put_segments.size() == 1);

    DataObject::rdma_segment_desc rdma_put_segment = rdma_put_segments.front();
    rdma_put_buffer = (NNTI_buffer_t) rdma_put_segment.net_buffer_handle;
    rdma_put_offset = rdma_put_segment.net_buffer_offset;

    DataObject memory(nnti_attrs.mtu - nnti_attrs.max_eager_size, 128, DataObject::AllocatorType::eager);
    NNTI_buffer_t rdma_buffer;
    uint32_t rdma_offset = 0;
    std::queue<DataObject::rdma_segment_desc> rdma_segments;

    memory.GetHeaderRdmaHandles(rdma_segments);
    assert(rdma_segments.size() == 1);

    DataObject::rdma_segment_desc rdma_segment = rdma_segments.front();
    rdma_buffer = (NNTI_buffer_t) rdma_segment.net_buffer_handle;
    rdma_offset = rdma_segment.net_buffer_offset;

    char data[MAX_NET_BUFFER_REMOTE_SIZE];
    memset((void *) data, 0xFF, MAX_NET_BUFFER_REMOTE_SIZE);

    start_put_message_t *m = (start_put_message_t *) memory.GetDataPtr();
    // !!!! TODO: Leaf process should advertise size of remote LDO
    m->header.offset = rdma_put_offset;
    m->header.length = sizeof(start_put_message_t) - offsetof(start_put_message_t, body);

    transport->dt_pack((void *) rdma_put_buffer, m->body, MAX_NET_BUFFER_REMOTE_SIZE - 8);

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t wid;

    base_wr.op = NNTI_OP_SEND;
    base_wr.flags = (NNTI_op_flags_t) (NNTI_OF_LOCAL_EVENT | NNTI_OF_ZERO_COPY);
    base_wr.trans_hdl = nnti::transports::transport::to_hdl(transport);
    base_wr.peer = p;
    base_wr.local_hdl = rdma_buffer;
    base_wr.local_offset = rdma_offset;
    base_wr.remote_hdl = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length = memory.GetMetaSize() + memory.GetDataSize();

    nnti::datatype::nnti_event_callback send_callback(transport, send_callback_functor_leaf(send_promise));
    nnti::datatype::nnti_work_request wr(transport, base_wr, send_callback);

    transport->send(&wr, &wid);

    send_future.get();
    std::pair<uint32_t, uint32_t> put_details = leaf_recv_future.get();

    uint32_t length = put_details.first;
    assert(length == put_target.GetDataSize());
    uint32_t seed = put_details.second;
    char *msg = (char *) put_target.GetDataPtr();
    std::srand(seed);
    for(int i = 0; i<put_target.GetDataSize(); i++) {
      char expected = (std::rand() & 0xff);
      char received = (*msg++ & 0xff);
      assert(received == expected);
    }
  }
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
  return rc;
}

