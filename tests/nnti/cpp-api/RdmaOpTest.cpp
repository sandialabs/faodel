// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "nnti/nntiConfig.h"

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
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class NntiRdmaOpTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

    int mpi_rank, mpi_size;
    int root_rank;

    char                   server_url[1][NNTI_URL_LEN];
    const uint32_t         num_servers = 1;
    uint32_t               num_clients;
    bool                   i_am_server = false;

    virtual void SetUp () {
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
        root_rank = 0;
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        system("rm -f rank*_url");
        MPI_Barrier(MPI_COMM_WORLD);

        test_setup(0,
                   NULL,
                   config,
                   "RdmaOpTest",
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

TEST_F(NntiRdmaOpTest, start1) {
    NNTI_result_t rc;

    NNTI_peer_t peer_hdl;

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
        uint32_t            buf_size=3200;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        t->alloc(3200, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, func_cb, nullptr, &buf_base, &buf_hdl);

        MPI_Barrier(MPI_COMM_WORLD);

        NNTI_buffer_t target_hdl;
        NNTI_buffer_t ack_hdl;
        NNTI_peer_t   peer_hdl;

        rc = recv_hdl(t, buf_hdl, buf_base, buf_size, &target_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("RdmaOpTest", "recv_target_hdl() failed: %d", rc);
        }

        rc = recv_hdl(t, buf_hdl, buf_base, buf_size, &ack_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("RdmaOpTest", "recv_target_hdl() failed: %d", rc);
        }

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = get_data(t, target_hdl, i*320, buf_hdl, i*320, 320, peer_hdl, eq);
            }
            for (int i=0;i<10;i++) {
                EXPECT_TRUE(verify_buffer(buf_base, i*320, buf_size, 320));
            }
        }

        for (int i=0;i<10;i++) {
            rc = populate_buffer(t, i, i, buf_hdl, buf_base, buf_size);
        }

        for (int j=0;j<100;j++) {
            for (int i=0;i<10;i++) {
                rc = put_data(t, buf_hdl, i*320, target_hdl, i*320, 320, peer_hdl, eq);
            }
        }

        rc = send_ack(t, buf_hdl, ack_hdl, peer_hdl, eq);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       buf_hdl;
        NNTI_buffer_t       ack_hdl;
        char               *buf_base=nullptr;
        char               *ack_base=nullptr;
        uint32_t            buf_size=3200;
        uint32_t            ack_size=320;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        // give the server a chance to startup
        MPI_Barrier(MPI_COMM_WORLD);

        rc = t->connect(server_url[0], 1000, &peer_hdl);
        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        rc = t->alloc(3200, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &buf_base, &buf_hdl);
        rc = t->alloc(320, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &ack_base, &ack_hdl);

        NNTI_buffer_t target_hdl;
        NNTI_peer_t   recv_peer;

        rc = send_hdl(t, buf_hdl, buf_base, buf_size, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("RdmaOpTest", "send_target_hdl() failed: %d", rc);
        }

        for (int i=0;i<10;i++) {
            rc = populate_buffer(t, i, i, buf_hdl, buf_base, buf_size);
        }

        rc = send_hdl(t, ack_hdl, ack_base, ack_size, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("RdmaOpTest", "send_target_hdl() failed: %d", rc);
        }

        rc = recv_ack(t, ack_hdl, &recv_peer, eq);
        if (rc != NNTI_OK) {
            log_error("RdmaOpTest", "recv_target_hdl() failed: %d", rc);
        }

        for (int i=0;i<10;i++) {
            EXPECT_TRUE(verify_buffer(buf_base, i*320, buf_size, 320));
        }

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

    MPI_Finalize();

    return (rc);
}
