// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

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

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";


class NntiLongSendTest : public testing::Test {
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

        test_setup(0,
                   NULL,
                   config,
                   "LongSendTest",
                   server_url,
                   mpi_size,
                   mpi_rank,
                   num_servers,
                   num_clients,
                   i_am_server,
                   t);
    }

  void TearDown () override {
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

TEST_F(NntiLongSendTest, start1) {
    NNTI_result_t rc;
    NNTI_peer_t peer_hdl;

    log_debug_stream("LongSendTest") << "i_am_server=" << i_am_server;

    nnti::datatype::nnti_event_callback null_cb(t);
    nnti::datatype::nnti_event_callback func_cb(t, cb_func);
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    if (i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       buf_hdl;
        char               *buf_base=nullptr;
        uint32_t            buf_size=40960;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        t->alloc(buf_size, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, func_cb, nullptr, &buf_base, &buf_hdl);

        MPI_Barrier(MPI_COMM_WORLD);

        NNTI_buffer_t target_hdl;
        NNTI_peer_t   peer_hdl;

        rc = recv_target_hdl(t, buf_hdl, buf_base, &target_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("ShortSendTest", "recv_target_hdl() failed: %d", rc);
        }

        rc = send_target_hdl(t, buf_hdl, buf_base, buf_size, buf_hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("ShortSendTest", "send_target_hdl() failed: %d", rc);
        }

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = recv_data(t, eq, &event);
            }
            for (int i=0;i<10;i++) {
                EXPECT_TRUE(verify_buffer((char*)event.start, i*(buf_size/10), event.length, 4096));
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

        for (int i=0;i<10;i++) {
            rc = populate_buffer(t, i, 4096, i, buf_hdl, buf_base, buf_size);
        }

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = send_data(t, 4096, i, buf_hdl, target_hdl, peer_hdl, eq);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       buf_hdl, unused_hdl;
        char               *buf_base=nullptr, *unused_base=nullptr;
        uint32_t            buf_size=40960;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        // give the server a chance to startup
        MPI_Barrier(MPI_COMM_WORLD);

        rc = t->connect(server_url[0], 1000, &peer_hdl);
        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        rc = t->alloc(buf_size, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &unused_base, &unused_hdl);
        rc = t->alloc(buf_size, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &buf_base, &buf_hdl);

        NNTI_buffer_t target_hdl;
        NNTI_peer_t   recv_peer;

        rc = send_target_hdl(t, buf_hdl, buf_base, buf_size, buf_hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("ShortSendTest", "send_target_hdl() failed: %d", rc);
        }

        rc = recv_target_hdl(t, buf_hdl, buf_base, &target_hdl, &recv_peer, eq);
        if (rc != NNTI_OK) {
            log_error("ShortSendTest", "recv_target_hdl() failed: %d", rc);
        }

        for (int i=0;i<10;i++) {
            rc = populate_buffer(t, i, 4096, i, buf_hdl, buf_base, buf_size);
        }

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = send_data(t, 4096, i, buf_hdl, target_hdl, peer_hdl, eq);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = recv_data(t, eq, &event);
            }
            for (int i=0;i<10;i++) {
                EXPECT_TRUE(verify_buffer((char*)event.start, i*(buf_size/10), event.length, 4096));
            }
        }

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
