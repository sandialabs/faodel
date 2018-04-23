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
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class NntiUnexpectedLongSendTest : public testing::Test {
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
                   "UnexpectedLongSendTest",
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

TEST_F(NntiUnexpectedLongSendTest, start1) {
    NNTI_result_t rc;

    NNTI_peer_t peer_hdl;

    uint32_t msg_size = 4096;

    nnti::datatype::nnti_event_callback null_cb(t, (NNTI_event_callback_t)0);

    if (i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       dst_buf;
        char               *dst_base=nullptr;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        t->alloc(msg_size*10, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), (NNTI_event_queue_t)0, null_cb, nullptr, &dst_base, &dst_buf);

        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);

        MPI_Barrier(MPI_COMM_WORLD);

        for (int j=0;j<10;j++) {
            uint32_t msgs_received=0;
            while (true) {
                rc = t->eq_wait(&eq, 1, 100, &which, &event);
                if (rc != NNTI_OK) {
                    log_error("UnexpectedLongSendTest", "eq_wait() failed: %d", rc);
                    continue;
                }
                uint64_t dst_offset = msgs_received * msg_size;
                rc =  t->next_unexpected(dst_buf, dst_offset, &result_event);
                if (rc != NNTI_OK) {
                    log_error("UnexpectedLongSendTest", "next_unexpected() failed: %d", rc);
                }
                if (++msgs_received == 10) {
                    break;
                }
            }
            for (int i=0;i<10;i++) {
                char *payload = dst_base + (i * msg_size);

                uLong crc = crc32(0L, Z_NULL, 0);
                crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4);

                log_debug("UnexpectedLongSendTest", "crc(%d)=%08x", i, crc);

                if (*(uint32_t*)payload != crc) {
                    log_error("UnexpectedLongSendTest", "crc mismatch (expected=%08x  actual=%08x)", *(uint32_t*)payload, (uint32_t)crc);
                    EXPECT_EQ(*(uint32_t*)payload, crc);
                }
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = (NNTI_transport_t)t;
        base_wr.peer          = event.peer;
        base_wr.local_hdl     = dst_buf;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg_size;

        for (int j=0;j<10;j++) {
            for (int i=0;i<10;i++) {
                base_wr.local_offset = i * msg_size;

                nnti::datatype::nnti_work_request wr(t, base_wr);
                NNTI_work_id_t                       wid;

                char *payload = dst_base + (i * msg_size);
                memset(payload, 0x06, msg_size);
                uLong crc = crc32(0L, Z_NULL, 0);
                crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4);

                *(uint32_t*)payload = crc;

                log_debug("UnexpectedLongSendTest", "payload(%d)=%08x  crc(%d)=%08x", i, *(uint32_t*)payload, i, crc);

                rc = t->send(&wr, &wid);
            }
            for (int i=0;i<10;i++) {
                rc = t->eq_wait(&eq, 1, 100, &which, &event);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       reg_buf, unused_buf;
        char               *reg_base=nullptr, *unused_base=nullptr;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        // give the server a chance to startup
        MPI_Barrier(MPI_COMM_WORLD);

        rc = t->connect(server_url[0], 1000, &peer_hdl);
        rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        rc = t->alloc(msg_size*10, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, null_cb, nullptr, &unused_base, &unused_buf);
        rc = t->alloc(msg_size*10, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, null_cb, nullptr, &reg_base, &reg_buf);

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = (NNTI_transport_t)t;
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = reg_buf;
        base_wr.local_offset  = 0;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg_size;

        char *payload = reg_base;
        uint64_t packed_size;
        rc = t->dt_sizeof((void*)reg_buf, &packed_size);
        if (rc != NNTI_OK) {
            log_fatal("UnexpectedLongSendTest", "dt_sizeof() failed: %d", rc);
        }
        rc = t->dt_pack((void*)reg_buf, payload+8, 256);
        if (rc != NNTI_OK) {
            log_fatal("UnexpectedLongSendTest", "dt_pack() failed: %d", rc);
        }

        for (int j=0;j<10;j++) {
            for (int i=0;i<10;i++) {
                base_wr.local_offset = i * msg_size;

                nnti::datatype::nnti_work_request wr(t, base_wr);
                NNTI_work_id_t                       wid;

                char *payload = reg_base + (i * msg_size);
                memset(payload, 0x05, msg_size);
                uLong crc = crc32(0L, Z_NULL, 0);
                crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4);

                *(uint32_t*)payload = crc;

                log_debug("UnexpectedLongSendTest", "payload(%d)=%08x  crc(%d)=%08x", i, *(uint32_t*)payload, i, crc);

                rc = t->send(&wr, &wid);
            }
            for (int i=0;i<10;i++) {
                rc = t->eq_wait(&eq, 1, 100, &which, &event);
                if (rc != NNTI_OK) {
                    log_error("UnexpectedLongSendTest", "eq_wait() failed: %d", rc);
                }
            }
        }

        MPI_Barrier(MPI_COMM_WORLD);

        NNTI_buffer_t  dst_buf;
        char          *dst_base=nullptr;
        t->alloc(msg_size*10, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), (NNTI_event_queue_t)0, null_cb, nullptr, &dst_base, &dst_buf);

        for (int j=0;j<10;j++) {
            uint32_t msgs_received=0;
            while (true) {
                rc = t->eq_wait(&eq, 1, 100, &which, &event);
                if (rc != NNTI_OK) {
                    log_error("UnexpectedLongSendTest", "eq_wait() failed: %d", rc);
                    continue;
                }
                uint64_t dst_offset = msgs_received * msg_size;
                rc =  t->next_unexpected(dst_buf, dst_offset, &result_event);
                if (rc != NNTI_OK) {
                    log_error("UnexpectedLongSendTest", "next_unexpected() failed: %d", rc);
                }
                if (++msgs_received == 10) {
                    break;
                }
            }
            for (int i=0;i<10;i++) {
                char *payload = dst_base + (i * msg_size);

                uLong crc = crc32(0L, Z_NULL, 0);
                crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4);

                log_debug("UnexpectedLongSendTest", "crc(%d)=%08x", i, crc);

                if (*(uint32_t*)payload != crc) {
                    log_error("UnexpectedLongSendTest", "crc mismatch (expected=%08x  actual=%08x)", *(uint32_t*)payload, (uint32_t)crc);
                    EXPECT_EQ(*(uint32_t*)payload, crc);
                }
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

    MPI_Finalize();

    return (rc);
}
