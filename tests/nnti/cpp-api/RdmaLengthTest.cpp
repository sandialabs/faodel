// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
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

#include <iostream>
#include <sstream>
#include <thread>

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_util.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "test_env.hpp"
#include "test_utils.hpp"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";

//const uint32_t outer=100;
//const uint32_t inner=1000;
const uint32_t blocksize=8192;


class NntiRdmaLengthTest : public testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(NntiRdmaLengthTest, simple) {
    NNTI_result_t rc;

    NNTI_peer_t peer_hdl;

    nnti::datatype::nnti_event_callback null_cb(FaodelNntiTests::globals->t);
    nnti::datatype::nnti_event_callback func_cb(FaodelNntiTests::globals->t, cb_func);
    nnti::datatype::nnti_event_callback obj_cb(FaodelNntiTests::globals->t, callback());

    if (FaodelNntiTests::globals->i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_buffer_t       buf_hdl;
        char               *buf_base=nullptr;
        uint32_t            buf_size=3200;

        rc = FaodelNntiTests::globals->t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);

        FaodelNntiTests::globals->t->alloc(blocksize, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, func_cb, nullptr, &buf_base, &buf_hdl);

        MPI_Barrier(MPI_COMM_WORLD);

        NNTI_buffer_t target_hdl;
        NNTI_buffer_t ack_hdl;
        NNTI_peer_t   peer_hdl;

        rc = recv_hdl(FaodelNntiTests::globals->t, buf_hdl, buf_base, buf_size, &target_hdl, &peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = recv_hdl(FaodelNntiTests::globals->t, buf_hdl, buf_base, buf_size, &ack_hdl, &peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        // We start off with a test that transfers blocksize bytes, so it should PASS.
        nnti::datatype::nnti_event_callback obj_cb(FaodelNntiTests::globals->t, callback());
        rc = get_data_async(FaodelNntiTests::globals->t, target_hdl, 0, buf_hdl, 0, blocksize, peer_hdl, obj_cb, nullptr);
        EXPECT_EQ(rc, NNTI_OK);
        rc = wait_data(FaodelNntiTests::globals->t, eq);
        EXPECT_EQ(rc, NNTI_OK);
        EXPECT_TRUE(verify_buffer(buf_base, 0, blocksize, blocksize));

        rc = populate_buffer(FaodelNntiTests::globals->t, 0, blocksize, 0, buf_hdl, buf_base, blocksize);
        EXPECT_EQ(rc, NNTI_OK);

        rc = put_data_async(FaodelNntiTests::globals->t, buf_hdl, 0, target_hdl, 0, blocksize, peer_hdl, obj_cb, nullptr);
        EXPECT_EQ(rc, NNTI_OK);
        rc = wait_data(FaodelNntiTests::globals->t, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = send_ack(FaodelNntiTests::globals->t, buf_hdl, ack_hdl, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_buffer_t       buf_hdl;
        NNTI_buffer_t       ack_hdl;
        char               *buf_base=nullptr;
        char               *ack_base=nullptr;
        uint32_t            buf_size=3200;
        uint32_t            ack_size=320;

        // give the server a chance to startup
        MPI_Barrier(MPI_COMM_WORLD);

        rc = FaodelNntiTests::globals->t->connect(FaodelNntiTests::globals->server_url[0], 1000, &peer_hdl);
        rc = FaodelNntiTests::globals->t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        rc = FaodelNntiTests::globals->t->alloc(blocksize, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &buf_base, &buf_hdl);
        rc = FaodelNntiTests::globals->t->alloc(320, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &ack_base, &ack_hdl);

        NNTI_peer_t   recv_peer;

        rc = send_hdl(FaodelNntiTests::globals->t, buf_hdl, buf_base, buf_size, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = populate_buffer(FaodelNntiTests::globals->t, 0, blocksize, 0, buf_hdl, buf_base, blocksize);
        EXPECT_EQ(rc, NNTI_OK);

        rc = send_hdl(FaodelNntiTests::globals->t, ack_hdl, ack_base, ack_size, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = recv_ack(FaodelNntiTests::globals->t, ack_hdl, &recv_peer, eq);
        EXPECT_EQ(rc, NNTI_OK);

        EXPECT_TRUE(verify_buffer(buf_base, 0, blocksize, blocksize));

        FaodelNntiTests::globals->t->disconnect(peer_hdl);
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

TEST_F(NntiRdmaLengthTest, out_of_bounds) {
    NNTI_result_t rc;

    NNTI_peer_t peer_hdl;
    
#ifndef NNTI_ENABLE_ARGS_CHECKING
    if (FaodelNntiTests::globals->t->id() == NNTI_TRANSPORT_UGNI) {
      /*
       * NNTI is not checking args because it is too expensive.
       * If the Faodel UGNI transport was built with one of the 
       * hugepages modules loaded, UGNI will pin the entire hugepage 
       * and let you RDMA to/from any region in that hugepage even 
       * if that is outside the actual region given in the 
       * GNI_RegisterMemory() call.  Therefore, we skip this 
       * test when args checking is disabled and the transport is 
       * UGNI.
       */
      return;
    }
#endif

    nnti::datatype::nnti_event_callback null_cb(FaodelNntiTests::globals->t);
    nnti::datatype::nnti_event_callback func_cb(FaodelNntiTests::globals->t, cb_func);
    nnti::datatype::nnti_event_callback obj_cb(FaodelNntiTests::globals->t, callback());

    if (FaodelNntiTests::globals->i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_buffer_t       buf_hdl;
        char               *buf_base=nullptr;
        uint32_t            buf_size=3200;

        rc = FaodelNntiTests::globals->t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);

        char *heap_buffer = (char*)malloc(8*blocksize);
        FaodelNntiTests::globals->t->register_memory(heap_buffer, 
                                                     blocksize, 
                                                     (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), 
                                                     eq, 
                                                     func_cb, 
                                                     nullptr, 
                                                     &buf_hdl);

        MPI_Barrier(MPI_COMM_WORLD);

        NNTI_buffer_t target_hdl;
        NNTI_buffer_t ack_hdl;
        NNTI_peer_t   peer_hdl;

        rc = recv_hdl(FaodelNntiTests::globals->t, buf_hdl, heap_buffer, buf_size, &target_hdl, &peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = recv_hdl(FaodelNntiTests::globals->t, buf_hdl, heap_buffer, buf_size, &ack_hdl, &peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        // We start off with a test that transfers blocksize bytes, so it should PASS.
        nnti::datatype::nnti_event_callback obj_cb(FaodelNntiTests::globals->t, callback());

        // Next we do a test that transfers blocksize*2 bytes.
        // In a debug build, args checking should FAIL this test with NNTI_EMSGSIZE.
        // In a release build, args checking is disabled so the failure could occur immediately or in an event.
        rc = get_data_async(FaodelNntiTests::globals->t, target_hdl, 0, buf_hdl, 0, blocksize*2, peer_hdl, obj_cb, nullptr);
#ifdef NNTI_ENABLE_ARGS_CHECKING
        // NNTI is doing bounds checking of the RDMA arguments, so expect an immediate failure
        EXPECT_EQ(rc, NNTI_EMSGSIZE);
#else
        // We can't do an EXPECT_*() test here because the failure could happen now or later in an event.
        if (rc == NNTI_OK) {
            NNTI_event_t event;
            // get_data_async() succeeded, so we need to wait for an event.
            rc = wait_data(FaodelNntiTests::globals->t, eq, &event);
            // we expect the the wait() to succeed.
            EXPECT_EQ(rc, NNTI_OK);
            // we expect event.result to be !NNTI_OK.
            EXPECT_NE(event.result, NNTI_OK);
        } else {
            // the get() failed, so we have nothing else to do here.
        }
#endif

        rc = populate_buffer(FaodelNntiTests::globals->t, 0, 2*blocksize, 0, buf_hdl, heap_buffer, blocksize);
        EXPECT_EQ(rc, NNTI_OK);

        rc = put_data_async(FaodelNntiTests::globals->t, buf_hdl, 0, target_hdl, 0, blocksize*2, peer_hdl, obj_cb, nullptr);
#ifdef NNTI_ENABLE_ARGS_CHECKING
        // NNTI is doing bounds checking of the RDMA arguments, so expect an immediate failure
        EXPECT_EQ(rc, NNTI_EMSGSIZE);
#else
        if (rc == NNTI_OK) {
            NNTI_event_t event;
            // put_data_async() succeeded, so we need to wait for an event.
            rc = wait_data(FaodelNntiTests::globals->t, eq, &event);
            // we expect the the wait() to succeed.
            EXPECT_EQ(rc, NNTI_OK);
            // we expect event.result to be !NNTI_OK.
            EXPECT_NE(event.result, NNTI_OK);
        } else {
            // the put() failed, so we have nothing else to do here.
        }
#endif

        rc = send_ack(FaodelNntiTests::globals->t, buf_hdl, ack_hdl, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_buffer_t       buf_hdl;
        NNTI_buffer_t       ack_hdl;
        char               *buf_base=nullptr;
        char               *ack_base=nullptr;
        uint32_t            buf_size=3200;
        uint32_t            ack_size=320;

        // give the server a chance to startup
        MPI_Barrier(MPI_COMM_WORLD);

        rc = FaodelNntiTests::globals->t->connect(FaodelNntiTests::globals->server_url[0], 1000, &peer_hdl);
        rc = FaodelNntiTests::globals->t->eq_create(128, NNTI_EQF_UNEXPECTED, &eq);
        char *heap_buffer = (char*)malloc(8*blocksize);
        FaodelNntiTests::globals->t->register_memory(heap_buffer, 
                           blocksize, 
                          (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), 
                          eq, 
                          obj_cb, 
                          nullptr, 
                          &buf_hdl);
//        rc = FaodelNntiTests::globals->t->alloc(blocksize, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &buf_base, &buf_hdl);
        rc = FaodelNntiTests::globals->t->alloc(320, (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE), eq, obj_cb, nullptr, &ack_base, &ack_hdl);

        NNTI_peer_t   recv_peer;

        rc = send_hdl(FaodelNntiTests::globals->t, buf_hdl, heap_buffer, buf_size, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = populate_buffer(FaodelNntiTests::globals->t, 0, 2*blocksize, 0, buf_hdl, heap_buffer, blocksize);
        EXPECT_EQ(rc, NNTI_OK);

        rc = send_hdl(FaodelNntiTests::globals->t, ack_hdl, ack_base, ack_size, peer_hdl, eq);
        EXPECT_EQ(rc, NNTI_OK);

        rc = recv_ack(FaodelNntiTests::globals->t, ack_hdl, &recv_peer, eq);
        EXPECT_EQ(rc, NNTI_OK);

        FaodelNntiTests::globals->t->disconnect(peer_hdl);
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

    // Create and register an Environment object
    testing::Environment* const faodel_env = 
        testing::AddGlobalTestEnvironment(new FaodelNntiTests::Environment("RdmaLengthTest", default_config_string));

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Finalize();

    return (rc);
}
