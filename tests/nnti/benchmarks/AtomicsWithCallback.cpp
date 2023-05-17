// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <assert.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_util.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "bench_utils.hpp"

using namespace std;
using namespace faodel;

bool success=true;

class test_context {
public:
    uint64_t atomic_count_;
    uint64_t threshold_;

    nnti::datatype::nnti_event_callback *cb_;
    nnti::transports::transport         *transport_;
    struct buffer_properties             atomic_src_;
    struct buffer_properties             atomic_target_;

public:
    test_context(
        uint64_t                             threshold,
        nnti::datatype::nnti_event_callback *cb,
        nnti::transports::transport         *transport,
        struct buffer_properties             atomic_src,
        struct buffer_properties             atomic_target)
    : atomic_count_(0),
      threshold_(threshold),
      cb_(cb),
      transport_(transport),
      atomic_src_(atomic_src),
      atomic_target_(atomic_target)
    {
        return;
    }
};

class test_callback {
public:
    test_callback()
    {
        return;
    }

    NNTI_result_t operator() (NNTI_event_t *event, void *context) {

        test_context *c = (test_context*)context;

        log_debug("test_callback", "enter");

        switch (event->type) {
            case NNTI_EVENT_ATOMIC:
                log_debug("test_callback", "ATOMIC event (atomic_count_=%lu)", c->atomic_count_);

                if (c->atomic_count_ < c->threshold_) {
                    fadd_async(c->transport_, c->atomic_src_.hdl, c->atomic_target_.hdl, 8, 1, event->peer, *c->cb_, context);
                    c->atomic_count_++;
                } else {
                    // we're done here.  return NNTI_ECANCEL to push an event on the EQ.
                    return NNTI_ECANCELED;
                }
                break;
            default:
                break;
        }

        log_debug("test_callback", "exit");

        return NNTI_OK;
    }
};

string default_config_string = R"EOF(

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";

NNTI_result_t runbench_fadd(bool                         server,
                            test_context                *ppc,
                            nnti::transports::transport *t,
                            NNTI_event_queue_t           test_eq,
                            NNTI_peer_t                  peer_hdl)
{
    NNTI_event_t event;

    if (server) {
        auto start = std::chrono::high_resolution_clock::now();

        MPI_Barrier(MPI_COMM_WORLD);

        auto end = std::chrono::high_resolution_clock::now();

        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->threshold_;

        log_info("AtomicsWithCallback chrono fetch_add", "%6lu    %6luus   %6.3fus",
            ppc->threshold_,
            total_us,
            us_per_xfer);
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        fadd_async(t, ppc->atomic_src_.hdl, ppc->atomic_target_.hdl, 8, 1, peer_hdl, *ppc->cb_, ppc);

        // the state machine will put an event on the eq when the volley is over
        recv_data(t, test_eq, &event);

        MPI_Barrier(MPI_COMM_WORLD);

        auto end = std::chrono::high_resolution_clock::now();

        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->threshold_;

        log_info("AtomicsWithCallback chrono fetch_add", "%6lu    %6luus   %6.3fus",
            ppc->threshold_,
            total_us,
            us_per_xfer);
    }

    return NNTI_OK;
}

NNTI_result_t runbench_cswap(bool                         server,
                             test_context                *ppc,
                             nnti::transports::transport *t,
                             NNTI_event_queue_t           test_eq,
                             NNTI_peer_t                  peer_hdl)
{
    NNTI_event_t event;

    if (server) {
        auto start = std::chrono::high_resolution_clock::now();

        MPI_Barrier(MPI_COMM_WORLD);

        auto end = std::chrono::high_resolution_clock::now();

        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->threshold_;

        log_info("AtomicsWithCallback chrono compare_swap", "%6lu    %6luus   %6.3fus",
            ppc->threshold_,
            total_us,
            us_per_xfer);
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        cswap_async(t, ppc->atomic_src_.hdl, ppc->atomic_target_.hdl, 8, 1, 1, peer_hdl, *ppc->cb_, ppc);

        // the state machine will put an event on the eq when the volley is over
        recv_data(t, test_eq, &event);

        MPI_Barrier(MPI_COMM_WORLD);

        auto end = std::chrono::high_resolution_clock::now();

        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->threshold_;

        log_info("AtomicsWithCallback chrono compare_swap", "%6lu    %6luus   %6.3fus",
            ppc->threshold_,
            total_us,
            us_per_xfer);
    }

    return NNTI_OK;
}

int main(int argc, char *argv[])
{
    Configuration config;

    NNTI_result_t rc;
    nnti::transports::transport *t=nullptr;

    char                   server_url[1][NNTI_URL_LEN];
    const uint32_t         num_servers = 1;
    uint32_t               num_clients;
    bool                   i_am_server = false;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    config = Configuration(default_config_string);
    config.AppendFromReferences();

    test_setup(0,
               NULL,
               config,
               "AtomicsWithCallback",
               server_url,
               mpi_size,
               mpi_rank,
               num_servers,
               num_clients,
               i_am_server,
               t);

    nnti::datatype::nnti_event_callback null_cb(t);
    nnti::datatype::nnti_event_callback obj_cb(t, test_callback());

    uint32_t volley_count=1000;

    NNTI_peer_t         peer_hdl;

    NNTI_event_queue_t  unexpected_eq;
    NNTI_event_queue_t  test_eq;
    NNTI_event_t        event;

    struct buffer_properties src_buf;
    struct buffer_properties my_pingpong_buf;
    struct buffer_properties peer_pingpong_buf;

    rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &unexpected_eq);
    rc = t->eq_create(128, NNTI_EQF_UNSET, &test_eq);

    test_callback *cb = new test_callback();
    nnti::datatype::nnti_event_callback *ppcb = new nnti::datatype::nnti_event_callback(t, *cb);
    test_context                        *ppc  = new test_context(volley_count,
                                                                 ppcb,
                                                                 t,
                                                                 my_pingpong_buf,
                                                                 peer_pingpong_buf);

    src_buf.size=2*1024*1024;
    rc = t->alloc(src_buf.size,
                  (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                  unexpected_eq,
                  null_cb,
                  nullptr,
                  &src_buf.base,
                  &src_buf.hdl);
    my_pingpong_buf.size=2*1024*1024;
    rc = t->alloc(my_pingpong_buf.size,
                  (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                  test_eq,
                  *ppcb,
                  ppc,
                  &my_pingpong_buf.base,
                  &my_pingpong_buf.hdl);

    ppc->atomic_src_ = my_pingpong_buf;

    if (i_am_server) {
        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("AtomicsWithCallback", "recv_target_hdl() failed: %d", rc);
        }

        ppc->atomic_target_ = peer_pingpong_buf;

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("AtomicsWithCallback", "send_target_hdl() failed: %d", rc);
        }

        log_info("AtomicsWithCallback chrono fetch_add", "iters    time      usec/op");

        for (int i=0;i<10;i++) {
            *(uint64_t*)my_pingpong_buf.base = 0;
            MPI_Barrier(MPI_COMM_WORLD);
            runbench_fadd(i_am_server, ppc, t, test_eq, peer_hdl);
            ppc->atomic_count_  = 0;
        }

        MPI_Barrier(MPI_COMM_WORLD);

        log_info("AtomicsWithCallback chrono compare_swap", "iters    time      usec/op");

        for (int i=0;i<10;i++) {
            *(uint64_t*)my_pingpong_buf.base = 0;
            MPI_Barrier(MPI_COMM_WORLD);
            runbench_cswap(i_am_server, ppc, t, test_eq, peer_hdl);
            ppc->atomic_count_  = 0;
        }

        // send the all clear message to the other process
        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("AtomicsWithCallback", "send_target_hdl() failed: %d", rc);
        }

        MPI_Barrier(MPI_COMM_WORLD);

    } else {

        rc = t->connect(server_url[0], 1000, &peer_hdl);

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("AtomicsWithCallback", "send_target_hdl() failed: %d", rc);
        }

        NNTI_peer_t recv_peer;
        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &recv_peer, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("AtomicsWithCallback", "recv_target_hdl() failed: %d", rc);
        }

        ppc->atomic_target_ = peer_pingpong_buf;

        log_info("AtomicsWithCallback chrono fetch_add", "iters    time      usec/op");

        for (int i=0;i<10;i++) {
            *(uint64_t*)my_pingpong_buf.base = 0;
            MPI_Barrier(MPI_COMM_WORLD);
            runbench_fadd(i_am_server, ppc, t, test_eq, peer_hdl);
            ppc->atomic_count_  = 0;
        }

        MPI_Barrier(MPI_COMM_WORLD);

        log_info("AtomicsWithCallback chrono compare_swap", "iters    time      usec/op");

        for (int i=0;i<10;i++) {
            *(uint64_t*)my_pingpong_buf.base = 0;
            MPI_Barrier(MPI_COMM_WORLD);
            runbench_cswap(i_am_server, ppc, t, test_eq, peer_hdl);
            ppc->atomic_count_  = 0;
        }

        // wait for the all clear message from the other process
        rc = recv_data(t, unexpected_eq, &event);

        MPI_Barrier(MPI_COMM_WORLD);

        t->disconnect(peer_hdl);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (t->initialized()) {
        t->stop();
    } else {
        success = false;
    }

    if (success) {
        log_debug_stream("AtomicsWithCallback") << "\nEnd Result: TEST PASSED";
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    } else {
        log_debug_stream("AtomicsWithCallback") << "\nEnd Result: TEST FAILED";
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return (success ? 0 : 1 );
}
