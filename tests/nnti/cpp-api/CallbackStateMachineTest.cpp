// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_util.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "test_utils.hpp"

using namespace std;
using namespace faodel;

int msg_size;
int test_iters;

class state_machine_context {
public:
    enum state {
        SENDING,
        GETTING,
        PUTTING,
        DONE
    };
    enum state state_;

    uint64_t send_count_;
    uint64_t get_count_;
    uint64_t put_count_;
    uint64_t send_threshold_;
    uint64_t get_threshold_;
    uint64_t put_threshold_;

    nnti::datatype::nnti_event_callback *cb_;
    nnti::transports::transport         *transport_;
    struct buffer_properties             send_src_;
    struct buffer_properties             send_target_;
    struct buffer_properties             local_rdma_;
    struct buffer_properties             remote_rdma_;

public:
    state_machine_context(
        uint64_t                             send_threshold,
        uint64_t                             get_threshold,
        uint64_t                             put_threshold,
        nnti::datatype::nnti_event_callback *cb,
        nnti::transports::transport         *transport,
        struct buffer_properties             send_src,
        struct buffer_properties             send_target,
        struct buffer_properties             local_rdma,
        struct buffer_properties             remote_rdma)
    : state_(SENDING),
      send_count_(1),
      get_count_(0),
      put_count_(0),
      send_threshold_(send_threshold),
      get_threshold_(get_threshold),
      put_threshold_(put_threshold),
      cb_(cb),
      transport_(transport),
      send_src_(send_src),
      send_target_(send_target),
      local_rdma_(local_rdma),
      remote_rdma_(remote_rdma)
    {
        return;
    }
};

class state_machine_callback {
public:
    state_machine_callback()
    {
        return;
    }

    NNTI_result_t operator() (NNTI_event_t *event, void *context) {

        state_machine_context *c = (state_machine_context*)context;

        log_debug("state_machine_callback", "c->state_=%d, send_count_=%lu, get_count_=%lu, put_count_=%lu, send_threshold_=%lu, get_threshold_=%lu, put_threshold_=%lu",
            c->state_, c->send_count_, c->get_count_, c->put_count_, c->send_threshold_, c->get_threshold_, c->put_threshold_);

        if (c->state_ == state_machine_context::state::DONE) {
            return NNTI_EIO;
        }

        switch (event->type) {
            case NNTI_EVENT_SEND:
                if (c->send_count_ < c->send_threshold_) {
                    // issue another send
                    populate_buffer(c->transport_, c->send_count_, 0, c->send_src_.hdl, c->send_src_.base, c->send_src_.size);
                    send_data_async(c->transport_, 0, c->send_src_.hdl, c->send_target_.hdl, event->peer, *c->cb_, context);

                    c->send_count_++;
                    break;
                } else {
                    c->state_ = state_machine_context::state::GETTING;
                    // issue get
                    get_data_async(c->transport_, c->remote_rdma_.hdl, c->local_rdma_.hdl, event->peer, *c->cb_, context);
                    c->get_count_++;
                }
                break;
            case NNTI_EVENT_GET:
                // verify get
                if (c->get_count_ < c->get_threshold_) {
                    for (int i=0;i<10;i++) {
                        EXPECT_TRUE(verify_buffer((char*)event->start, event->offset+(i*msg_size), msg_size*test_iters));
                    }
                    // issue another get
                    get_data_async(c->transport_, c->remote_rdma_.hdl, c->local_rdma_.hdl, event->peer, *c->cb_, context);

                    c->get_count_++;
                    break;
                } else {
                    for (int i=0;i<10;i++) {
                        EXPECT_TRUE(verify_buffer((char*)event->start, event->offset+(i*msg_size), msg_size*test_iters));
                    }
                    c->state_ = state_machine_context::state::PUTTING;
                    // issue put
                    for (int i=0;i<10;i++) {
                        populate_buffer(c->transport_, 2*i, i, c->local_rdma_.hdl, c->local_rdma_.base, c->local_rdma_.size);
                    }
                    put_data_async(c->transport_, c->local_rdma_.hdl, c->remote_rdma_.hdl, event->peer, *c->cb_, context);
                    c->put_count_++;
                }
                break;
            case NNTI_EVENT_PUT:
                if (c->put_count_ < c->put_threshold_) {
                    // issue another put
                    put_data_async(c->transport_, c->local_rdma_.hdl, c->remote_rdma_.hdl, event->peer, *c->cb_, context);
                    c->put_count_++;
                    break;
                } else {
                    c->state_ = state_machine_context::state::DONE;
                    send_data_async(c->transport_, 0, c->send_src_.hdl, c->send_target_.hdl, event->peer, *c->cb_, context);
                }
                break;
            default:
                break;
        }

        return NNTI_OK;
    }
};

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";


class NntiCallbackStateMachineTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

    int mpi_rank, mpi_size;
    int root_rank;

    char                   server_url[1][NNTI_URL_LEN];
    const uint32_t         num_servers = 1;
    uint32_t               num_clients;
    bool                   i_am_server = false;

  void SetUp () override {
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
        root_rank = 0;
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        MPI_Barrier(MPI_COMM_WORLD);

        msg_size   = 320;
        test_iters = 1000;

        test_setup(0,
                   NULL,
                   config,
                   "CallbackStateMachineTest",
                   server_url,
                   mpi_size,
                   mpi_rank,
                   num_servers,
                   num_clients,
                   i_am_server,
                   t);
    }
    virtual void TearDown () {
        NNTI_result_t nnti_rc = NNTI_OK;
        bool init;

        init = t->initialized();
        EXPECT_TRUE(init);

        if (init) {
            nnti_rc = t->stop();
            EXPECT_EQ(nnti_rc, NNTI_OK);
        }
    }
};

TEST_F(NntiCallbackStateMachineTest, start1) {
    NNTI_result_t rc = NNTI_OK;
    NNTI_peer_t peer_hdl;

    NNTI_event_queue_t  eq;
    NNTI_event_t        event;
    struct buffer_properties src_buf;
    struct buffer_properties rdma_buf;
    struct buffer_properties my_q_buf;

    nnti::datatype::nnti_event_callback null_cb(t);

    rc = t->eq_create(1024, NNTI_EQF_UNEXPECTED, &eq);
    src_buf.size   = msg_size;
    src_buf.offset = 0;
    t->alloc(src_buf.size,
             (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
             eq,
             null_cb,
             nullptr,
             &src_buf.base,
             &src_buf.hdl);
    rdma_buf.size   = msg_size*test_iters;
    rdma_buf.offset = 0;
    t->alloc(rdma_buf.size,
             (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
             eq,
             null_cb,
             nullptr,
             &rdma_buf.base,
             &rdma_buf.hdl);
    my_q_buf.size   = msg_size*test_iters;
    my_q_buf.offset = 0;
    t->alloc(my_q_buf.size,
             (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE|NNTI_BF_QUEUING),
             eq,
             null_cb,
             nullptr,
             &my_q_buf.base,
             &my_q_buf.hdl);

    if (i_am_server) {
        NNTI_buffer_t target_hdl;
        NNTI_peer_t   peer_hdl;

        MPI_Barrier(MPI_COMM_WORLD);

        for (int i=0;i<10;i++) {
            rc = populate_buffer(t, i, i, rdma_buf.hdl, rdma_buf.base, rdma_buf.size);
        }

        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &target_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "recv_target_hdl() failed: %d", rc);
        }

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_q_buf.hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "send_target_hdl() failed: %d", rc);
        }

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, rdma_buf.hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "send_target_hdl() failed: %d", rc);
        }

        for (int i=0;i<test_iters;i++) {
            rc = recv_data(t, eq, &event);
            EXPECT_TRUE(verify_buffer((char*)event.start, event.offset, event.length));
            t->event_complete(&event);
        }

        rc = recv_data(t, eq, &event);

        for (int i=0;i<10;i++) {
            EXPECT_TRUE(verify_buffer(rdma_buf.base, rdma_buf.offset+(i*msg_size), rdma_buf.size));
        }

        // send the all clear message to the other process
        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, rdma_buf.hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "send_target_hdl() failed: %d", rc);
        }

        MPI_Barrier(MPI_COMM_WORLD);

    } else {
        MPI_Barrier(MPI_COMM_WORLD);

        rc = t->connect(server_url[0], 1000, &peer_hdl);

        struct buffer_properties send_target_buf;
        struct buffer_properties rdma_target_buf;
        NNTI_peer_t   recv_peer;

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_q_buf.hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "send_target_hdl() failed: %d", rc);
        }

        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &send_target_buf.hdl, &recv_peer, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "recv_target_hdl() failed: %d", rc);
        }

        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &rdma_target_buf.hdl, &recv_peer, eq);
        if (rc != NNTI_OK) {
            log_error("CallbackStateMachineTest", "recv_target_hdl() failed: %d", rc);
        }

        state_machine_callback *cb = new state_machine_callback();
        nnti::datatype::nnti_event_callback *smcb = new nnti::datatype::nnti_event_callback(t, *cb);
        state_machine_context               *smc  = new state_machine_context(test_iters,
                                                                              test_iters,
                                                                              test_iters,
                                                                              smcb,
                                                                              t,
                                                                              src_buf,
                                                                              send_target_buf,
                                                                              rdma_buf,
                                                                              rdma_target_buf);

        rc = populate_buffer(t, 0, 0, src_buf.hdl, src_buf.base, src_buf.size);
        rc = send_data_async(t, 0, src_buf.hdl, send_target_buf.hdl, peer_hdl, *smcb, smc);

        // the state machine will put an event on the eq when it reaches the DONE state
        rc = wait_data(t, eq);

        // wait for the all clear message from the other process
        rc = recv_data(t, eq, &event);

        MPI_Barrier(MPI_COMM_WORLD);

        t->disconnect(peer_hdl);
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    EXPECT_EQ(2, mpi_size);
    assert(2==mpi_size);

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Barrier(MPI_COMM_WORLD);
    bootstrap::Finish();

    MPI_Finalize();

    return (rc);
}
