// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
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
    uint64_t send_count_;
    uint64_t recv_count_;
    uint64_t volley_threshold_;

    uint64_t length_;

    nnti::datatype::nnti_event_callback *cb_;
    nnti::transports::transport         *transport_;
    struct buffer_properties             send_src_;
    struct buffer_properties             send_target_;

public:
    test_context(
        uint64_t                             volley_threshold,
        uint64_t                             length,
        nnti::datatype::nnti_event_callback *cb,
        nnti::transports::transport         *transport,
        struct buffer_properties             send_src,
        struct buffer_properties             send_target)
    : send_count_(0),
      recv_count_(0),
      volley_threshold_(volley_threshold),
      length_(length),
      cb_(cb),
      transport_(transport),
      send_src_(send_src),
      send_target_(send_target)
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
            case NNTI_EVENT_SEND:
                log_debug("test_callback", "SEND event (send_count_=%lu)", c->send_count_);

                if (c->send_count_ < c->volley_threshold_) {
                    c->send_count_++;
                } else {
                    // we're done here.  return NNTI_ECANCEL to push an event on the EQ.
                    return NNTI_ECANCELED;
                }
                break;
            case NNTI_EVENT_RECV:
                log_debug("test_callback", "RECV event (recv_count_=%lu)", c->recv_count_);

                if (c->recv_count_ < c->volley_threshold_) {
                    // issue another send
                    send_data_async(c->transport_, c->length_, 0, c->send_src_.hdl, c->send_target_.hdl, event->peer, *c->cb_, context);
                    c->recv_count_++;
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

NNTI_result_t runbench(bool                         server,
                       test_context                *ppc,
                       nnti::transports::transport *t,
                       NNTI_event_queue_t           pingpong_eq,
                       NNTI_peer_t                  peer_hdl)
{
    NNTI_event_t event;

    if (server) {
        auto start = std::chrono::high_resolution_clock::now();

        // the state machine will put an event on the eq when the volley is over
        recv_data(t, pingpong_eq, &event);

        auto end = std::chrono::high_resolution_clock::now();

        // total_megabytes is the ROUNDTRIP total so multiply by 2
        float total_megabytes = (ppc->length_ * ppc->volley_threshold_ * 2.0) / (1024.0 * 1024.0);
        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->volley_threshold_/2.0;
        float total_sec       = (float)total_us/1000000.0;
        float MB_per_sec      = total_megabytes / total_sec;

        log_info("SendWithCallback chrono", "%6lu        %6lu    %6luus   %6.3fus   %6.3f",
            ppc->length_, ppc->volley_threshold_,
            total_us,
            us_per_xfer,
            MB_per_sec);
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        send_data_async(t, ppc->length_, 0, ppc->send_src_.hdl, ppc->send_target_.hdl, peer_hdl, *ppc->cb_, ppc);

        // the state machine will put an event on the eq when the volley is over
        recv_data(t, pingpong_eq, &event);

        auto end = std::chrono::high_resolution_clock::now();

        // total_megabytes is the ROUNDTRIP total so multiply by 2
        float total_megabytes = (ppc->length_ * ppc->volley_threshold_ * 2.0) / (1024.0 * 1024.0);
        uint64_t total_us     = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        float us_per_xfer     = (float)total_us/(float)ppc->volley_threshold_/2.0;
        float total_sec       = (float)total_us/1000000.0;
        float MB_per_sec      = total_megabytes / total_sec;

        log_info("SendWithCallback chrono", "%6lu        %6lu    %6luus   %6.3fus   %6.3f",
            ppc->length_, ppc->volley_threshold_,
            total_us,
            us_per_xfer,
            MB_per_sec);
    }

    return NNTI_OK;
}

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";


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
               "SendWithCallback",
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
    NNTI_event_queue_t  pingpong_eq;
    NNTI_event_t        event;

    struct buffer_properties src_buf;
    struct buffer_properties my_pingpong_buf;
    struct buffer_properties peer_pingpong_buf;

    rc = t->eq_create(128, NNTI_EQF_UNEXPECTED, &unexpected_eq);
    rc = t->eq_create(128, NNTI_EQF_UNSET, &pingpong_eq);

    test_callback *cb = new test_callback();
    nnti::datatype::nnti_event_callback *ppcb = new nnti::datatype::nnti_event_callback(t, *cb);
    test_context                        *ppc  = new test_context(volley_count,
                                                                     8,
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
                  pingpong_eq,
                  *ppcb,
                  ppc,
                  &my_pingpong_buf.base,
                  &my_pingpong_buf.hdl);

    ppc->send_src_ = my_pingpong_buf;

    if (i_am_server) {
        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("SendWithCallback", "recv_target_hdl() failed: %d", rc);
        }

        ppc->send_target_ = peer_pingpong_buf;

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("SendWithCallback", "send_target_hdl() failed: %d", rc);
        }

        log_info("SendWithCallback chrono", "bytes/xfer      iters    time      usec/xfer      Mbytes/sec");

        while (ppc->length_ <= 2*1024*1024) {
            MPI_Barrier(MPI_COMM_WORLD);
            runbench(i_am_server, ppc, t, pingpong_eq, peer_hdl);
            ppc->send_count_  = 0;
            ppc->recv_count_  = 0;
            ppc->length_     *= 2;
        }

        // send the all clear message to the other process
        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("SendWithCallback", "send_target_hdl() failed: %d", rc);
        }

        MPI_Barrier(MPI_COMM_WORLD);

    } else {

        rc = t->connect(server_url[0], 1000, &peer_hdl);

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("SendWithCallback", "send_target_hdl() failed: %d", rc);
        }

        NNTI_peer_t recv_peer;
        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &recv_peer, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("SendWithCallback", "recv_target_hdl() failed: %d", rc);
        }

        ppc->send_target_ = peer_pingpong_buf;

        log_info("SendWithCallback chrono", "bytes/xfer      iters    time      usec/xfer      Mbytes/sec");

        while (ppc->length_ <= 2*1024*1024) {
            MPI_Barrier(MPI_COMM_WORLD);
            runbench(i_am_server, ppc, t, pingpong_eq, peer_hdl);
            ppc->send_count_  = 0;
            ppc->recv_count_  = 0;
            ppc->length_     *= 2;
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
        log_debug_stream("SendWithCallback") << "\nEnd Result: TEST PASSED";
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    } else {
        log_debug_stream("SendWithCallback") << "\nEnd Result: TEST FAILED";
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return (success ? 0 : 1 );
}
