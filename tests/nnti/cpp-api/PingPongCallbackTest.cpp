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

class pingpong_context {
public:
    uint64_t send_count_;
    uint64_t recv_count_;
    uint64_t volley_threshold_;

    nnti::datatype::nnti_event_callback *cb_;
    nnti::transports::transport         *transport_;
    struct buffer_properties             send_src_;
    struct buffer_properties             send_target_;

public:
    pingpong_context(
        uint64_t                             volley_threshold,
        nnti::datatype::nnti_event_callback *cb,
        nnti::transports::transport         *transport,
        struct buffer_properties             send_src,
        struct buffer_properties             send_target)
    : send_count_(0),
      recv_count_(0),
      volley_threshold_(volley_threshold),
      cb_(cb),
      transport_(transport),
      send_src_(send_src),
      send_target_(send_target)
    {
        return;
    }
};

class pingpong_callback {
public:
    pingpong_callback()
    {
        return;
    }

    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        NNTI_result_t rc = NNTI_OK;

        pingpong_context *c = (pingpong_context*)context;

        log_debug("pingpong_callback", "enter");

        switch (event->type) {
            case NNTI_EVENT_SEND:
                log_debug("pingpong_callback", "SEND event (send_count_=%lu)", c->send_count_);

                if (c->send_count_ < c->volley_threshold_) {
                    c->send_count_++;
                } else {
                    // we're done here.  return NNTI_ECANCEL to push an event on the EQ.
                    return NNTI_ECANCELED;
                }
                break;
            case NNTI_EVENT_RECV:
                log_debug("pingpong_callback", "RECV event (recv_count_=%lu)", c->recv_count_);
                if (c->recv_count_ < c->volley_threshold_) {
                    // issue another send
                    EXPECT_TRUE(verify_buffer((char*)event->start, event->offset, event->length));

                    char     *payload = (char*)event->start + event->offset;
                    uint32_t seed     = *(uint32_t*)(payload+4); // the salt
                    seed++;

                    rc = populate_buffer(c->transport_, seed, 0, c->send_src_.hdl, c->send_src_.base, c->send_src_.size);

                    rc = send_data_async(c->transport_, 0, c->send_src_.hdl, c->send_target_.hdl, event->peer, *c->cb_, context);

                    c->recv_count_++;
                } else {
                    // we're done here.  return NNTI_ECANCEL to push an event on the EQ.
                    return NNTI_ECANCELED;
                }
                break;
            default:
                break;
        }

        log_debug("pingpong_callback", "exit");

        return NNTI_OK;
    }
};

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class NntiPingPongTest : public testing::Test {
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
                   "PingPongTest",
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

TEST_F(NntiPingPongTest, start1) {
    NNTI_result_t rc;

    nnti::datatype::nnti_event_callback null_cb(t);
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

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

    pingpong_callback *cb = new pingpong_callback();
    nnti::datatype::nnti_event_callback *ppcb = new nnti::datatype::nnti_event_callback(t, *cb);
    pingpong_context                    *ppc  = new pingpong_context(volley_count,
                                                                     ppcb,
                                                                     t,
                                                                     my_pingpong_buf,
                                                                     peer_pingpong_buf);

    src_buf.size=3200;
    rc = t->alloc(src_buf.size,
                  (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                  unexpected_eq,
                  null_cb,
                  nullptr,
                  &src_buf.base,
                  &src_buf.hdl);
    my_pingpong_buf.size=3200;
    rc = t->alloc(my_pingpong_buf.size,
                  (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                  pingpong_eq,
                  *ppcb,
                  ppc,
                  &my_pingpong_buf.base,
                  &my_pingpong_buf.hdl);

    ppc->send_src_ = my_pingpong_buf;

    if (i_am_server) {
        MPI_Barrier(MPI_COMM_WORLD);

        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("PingPongCallbackTest", "recv_target_hdl() failed: %d", rc);
        }

        ppc->send_target_ = peer_pingpong_buf;

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("PingPongCallbackTest", "send_target_hdl() failed: %d", rc);
        }

        // the state machine will put an event on the eq when the volley is over
        rc = recv_data(t, pingpong_eq, &event);

        // send the all clear message to the other process
        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("PingPongCallbackTest", "send_target_hdl() failed: %d", rc);
        }
    } else {
        // give the server a chance to startup
//        nnti::util::sleep(1000);

        MPI_Barrier(MPI_COMM_WORLD);

        rc = t->connect(server_url[0], 1000, &peer_hdl);

        rc = send_target_hdl(t, src_buf.hdl, src_buf.base, src_buf.size, my_pingpong_buf.hdl, peer_hdl, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("PingPongCallbackTest", "send_target_hdl() failed: %d", rc);
        }

        NNTI_peer_t recv_peer;
        rc = recv_target_hdl(t, src_buf.hdl, src_buf.base, &peer_pingpong_buf.hdl, &recv_peer, unexpected_eq);
        if (rc != NNTI_OK) {
            log_error("PingPongCallbackTest", "recv_target_hdl() failed: %d", rc);
        }

        ppc->send_target_ = peer_pingpong_buf;

        rc = populate_buffer(t, 0, 0, my_pingpong_buf.hdl, my_pingpong_buf.base, my_pingpong_buf.size);

        rc = send_data_async(t, 0, my_pingpong_buf.hdl, peer_pingpong_buf.hdl, peer_hdl, *ppcb, ppc);

        // the state machine will put an event on the eq when the volley is over
        rc = recv_data(t, pingpong_eq, &event);

        // wait for the all clear message from the other process
        rc = recv_data(t, unexpected_eq, &event);

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
