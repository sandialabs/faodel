// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <unistd.h>
#include <glob.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include <iostream>
#include <sstream>
#include <thread>

#include <chrono>

#include "faodel-common/Configuration.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/NodeID.hh"

#include "whookie/Server.hh"

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_util.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "bench_utils.hpp"


NNTI_result_t
cb_func(NNTI_event_t *event, void *context)
{
    std::cout << "This is a callback function.  My parameters are event(" << (void*)event << ") and context(" << (void*)context << ")" << std::endl;
    return NNTI_EIO;
}

int
test_setup(int                           argc,
           char                         *argv[],
           faodel::Configuration       &config,
           const char                   *logfile_basename,
           char                          server_url[][NNTI_URL_LEN],
           const uint32_t                mpi_size,
           const uint32_t                mpi_rank,
           const uint32_t                num_servers,
           uint32_t                     &num_clients,
           bool                         &i_am_server,
           nnti::transports::transport *&t)
{
    for (int i=1;i<argc;i++) {
        std::string s(argv[i]);
        std::replace(s.begin(), s.end(), '=', ' ');
        config.Append(s);
    }

    int num_procs = mpi_size;
    int my_rank   = mpi_rank;

    num_clients = num_procs - num_servers;

    faodel::bootstrap::Start(config, whookie::bootstrap);

    t = nnti::transports::factory::get_instance(config);

    t->start();

    char my_url[NNTI_URL_LEN];
    t->get_url(my_url, NNTI_URL_LEN);

    find_server_urls(num_servers, my_rank, num_procs, my_url, server_url, i_am_server);

    nnti::core::logger::get_instance()->set_channel_severity("BenchPingPong chrono", sbl::severity_level::debug);

    std::stringstream ss;
    config.sstr(ss, 0, 0);
    log_debug_stream("test_setup") << ss.str() << std::endl;

    return 0;
}

int
test_setup(int                           argc,
           char                         *argv[],
           faodel::Configuration       &config,
           const char                   *logfile_basename,
           char                          server_url[][NNTI_URL_LEN],
           const uint32_t                num_servers,
           uint32_t                     &num_clients,
           bool                         &i_am_server,
           nnti::transports::transport *&t)
{
    for (int i=1;i<argc;i++) {
        std::string s(argv[i]);
        std::replace(s.begin(), s.end(), '=', ' ');
        config.Append(s);
    }

    int num_procs=0;
    int my_rank=0;
    get_num_procs(num_procs);
    get_rank(my_rank);

    num_clients = num_procs - num_servers;

    faodel::bootstrap::Start(config, whookie::bootstrap);

    t = nnti::transports::factory::get_instance(config);

    t->start();

    char my_url[NNTI_URL_LEN];
    t->get_url(my_url, NNTI_URL_LEN);

    find_server_urls(num_servers, my_rank, num_procs, my_url, server_url, i_am_server);

    nnti::core::logger::get_instance()->set_channel_severity("BenchPingPong chrono", sbl::severity_level::debug);

    std::stringstream ss;
    config.sstr(ss, 0, 0);
    log_debug_stream("test_setup") << ss.str() << std::endl;

    return 0;
}

int
test_setup(int                           argc,
           char                         *argv[],
           faodel::Configuration       &config,
           const char                   *logfile_basename,
           nnti::transports::transport *&t)
{
    for (int i=1;i<argc;i++) {
        std::string s(argv[i]);
        std::replace(s.begin(), s.end(), '=', ' ');
        config.Append(s);
    }

    faodel::bootstrap::Start(config, whookie::bootstrap);

    t = nnti::transports::factory::get_instance(config);

    t->start();

    nnti::core::logger::get_instance()->set_channel_severity("BenchPingPong chrono", sbl::severity_level::debug);

    std::stringstream ss;
    config.sstr(ss, 0, 0);
    log_debug_stream("test_setup") << ss.str() << std::endl;

    return 0;
}

NNTI_result_t
get_num_procs(int &num_procs)
{
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    log_debug("test_utils", "MPI says job size is %u", num_procs);

    return NNTI_OK;
}

NNTI_result_t
get_rank(int &my_rank)
{
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    log_debug("test_utils", "MPI says my rank is %u", my_rank);

    return NNTI_OK;
}

NNTI_result_t
find_server_urls(uint32_t num_servers,
                 uint32_t my_rank,
                 uint32_t num_procs,
                 char *my_url,
                 char server_url[][NNTI_URL_LEN],
                 bool &i_am_server)
{
    char *all_urls;

    all_urls = (char*)malloc(sizeof(char) * NNTI_URL_LEN * num_procs);

    MPI_Allgather(my_url, NNTI_URL_LEN, MPI_CHAR,
                  all_urls, NNTI_URL_LEN, MPI_CHAR,
                  MPI_COMM_WORLD);
    for (uint32_t i=0;i<num_servers;i++) {
        memcpy(server_url[i], &all_urls[i*NNTI_URL_LEN], NNTI_URL_LEN);
    }

    if (my_rank < num_servers) {
        i_am_server = true;
    } else {
        i_am_server = false;
    }

    return NNTI_OK;
}

NNTI_result_t
send_target_hdl(nnti::transports::transport *t,
                NNTI_buffer_t                send_hdl,
                char                        *send_base,
                uint64_t                     send_size,
                NNTI_buffer_t                target_hdl,
                NNTI_peer_t                  peer_hdl,
                NNTI_event_queue_t           eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    nnti::datatype::nnti_event_callback func_cb(t, cb_func);
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    uint64_t packed_size;
    rc = t->dt_sizeof((void*)target_hdl, &packed_size);
    if (rc != NNTI_OK) {
        log_fatal("test_utils", "dt_sizeof() failed: %d", rc);
    }
    rc = t->dt_pack((void*)target_hdl, send_base, send_size);
    if (rc != NNTI_OK) {
        log_fatal("test_utils", "dt_pack() failed: %d", rc);
    }

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = send_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length        = packed_size;

    nnti::datatype::nnti_work_request wr(t, base_wr, func_cb);
    NNTI_work_id_t                    wid;

    rc = t->send(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = t->eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    return rc;
}

NNTI_result_t
recv_target_hdl(nnti::transports::transport *t,
                NNTI_buffer_t                recv_hdl,
                char                        *recv_base,
                NNTI_buffer_t               *target_hdl,
                NNTI_peer_t                 *peer_hdl,
                NNTI_event_queue_t           eq)
{
    NNTI_result_t rc = NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;

    uint32_t msgs_received=0;
    while (true) {
        rc = t->eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        log_debug_stream("test_utils") << event;
        uint64_t dst_offset = msgs_received * 320;
        rc =  t->next_unexpected(recv_hdl, dst_offset, &result_event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "next_unexpected() failed: %d", rc);
        }
        if (++msgs_received == 1) {
            break;
        }
    }

    // create an nnti_buffer from a packed buffer sent from the client
    t->dt_unpack((void*)target_hdl, recv_base, event.length);

    *peer_hdl   = event.peer;

    return rc;
}

NNTI_result_t
send_hdl(nnti::transports::transport *t,
         NNTI_buffer_t                hdl,
         char                        *hdl_base,
         uint32_t                     hdl_size,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "send_hdl - enter");

    nnti::datatype::nnti_event_callback null_cb(t);
    nnti::datatype::nnti_event_callback func_cb(t, cb_func);
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    uint64_t packed_size;
    rc = t->dt_sizeof((void*)hdl, &packed_size);
    if (rc != NNTI_OK) {
        log_fatal("test_utils", "dt_sizeof() failed: %d", rc);
    }
    rc = t->dt_pack((void*)hdl, hdl_base, hdl_size);
    if (rc != NNTI_OK) {
        log_fatal("test_utils", "dt_pack() failed: %d", rc);
    }

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length        = packed_size;

    nnti::datatype::nnti_work_request wr(t, base_wr, func_cb);
    NNTI_work_id_t                    wid;

    rc = t->send(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = t->eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    log_debug("test_utils", "send_hdl - exit");

    return rc;
}

NNTI_result_t
recv_hdl(nnti::transports::transport *t,
         NNTI_buffer_t                recv_hdl,
         char                        *recv_base,
         uint32_t                     recv_size,
         NNTI_buffer_t               *hdl,
         NNTI_peer_t                 *peer_hdl,
         NNTI_event_queue_t           eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;

    log_debug("test_utils", "recv_hdl - enter");

    uint32_t msgs_received=0;
    while (true) {
        rc = t->eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        log_debug_stream("test_utils") << event;
        uint64_t dst_offset = msgs_received * 320;
        rc =  t->next_unexpected(recv_hdl, dst_offset, &result_event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "next_unexpected() failed: %d", rc);
        }
        if (++msgs_received == 1) {
            break;
        }
    }

    log_debug("test_utils", "handle received");

    // create an nnti_buffer from a packed buffer sent from the client
    t->dt_unpack(hdl, recv_base, event.length);

    *peer_hdl   = event.peer;

    log_debug("test_utils", "recv_hdl - exit");

    return rc;
}

NNTI_result_t
send_ack(nnti::transports::transport *t,
         NNTI_buffer_t                hdl,
         NNTI_buffer_t                ack_hdl,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "send_ack - enter");

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = ack_hdl;
    base_wr.remote_offset = 0;
    base_wr.length        = 64;

    nnti::datatype::nnti_work_request wr(t, base_wr);
    NNTI_work_id_t                    wid;

    rc = t->send(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = t->eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    log_debug("test_utils", "send_ack - exit");

    return rc;
}

NNTI_result_t
recv_ack(nnti::transports::transport *t,
         NNTI_buffer_t                ack_hdl,
         NNTI_peer_t                 *peer_hdl,
         NNTI_event_queue_t           eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    uint32_t            which;

    log_debug("test_utils", "recv_ack - enter");

    uint32_t msgs_received=0;
    while (true) {
        rc = t->eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        log_debug_stream("test_utils") << event;
        if (++msgs_received == 1) {
            break;
        }
    }

    *peer_hdl   = event.peer;

    log_debug("test_utils", "recv_ack - exit");

    return rc;
}

NNTI_result_t
populate_buffer(nnti::transports::transport *t,
                uint32_t                     seed,
                uint64_t                     msg_size,
                uint64_t                     offset_multiplier,
                NNTI_buffer_t                buf_hdl,
                char                        *buf_base,
                uint64_t                     buf_size)
{
    NNTI_result_t rc = NNTI_OK;

    char packed[312];
    int  packed_size = 312;

    rc = t->dt_pack((void*)buf_hdl, packed, 312);
    if (rc != NNTI_OK) {
        log_fatal("test_utils", "dt_pack() failed: %d", rc);
    }

    char *payload = buf_base + msg_size*offset_multiplier;

    log_debug("test_utils", "buf_base=%p buf_size=%lu offset_multiplier=%lu offset=%lu", buf_base, buf_size, offset_multiplier, ((packed_size+4+4)*offset_multiplier));

    memcpy(payload+8, packed, packed_size);  // the data
    *(uint32_t*)(payload+4) = seed;          // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4); // the checksum
    *(uint32_t*)payload = 0;
    *(uint32_t*)payload = crc;

    log_debug("test_utils", "seed=0x%x  payload=%p  payload[0]=%08x  crc=%08x", seed, payload, *(uint32_t*)payload, crc);

    return NNTI_OK;
}

NNTI_result_t
populate_buffer(nnti::transports::transport *t,
                uint32_t                     seed,
                uint64_t                     offset_multiplier,
                NNTI_buffer_t                buf_hdl,
                char                        *buf_base,
                uint64_t                     buf_size)
{
    return populate_buffer(t,
                           seed,
                           320,
                           offset_multiplier,
                           buf_hdl,
                           buf_base,
                           buf_size);
}

bool
verify_buffer(char          *buf_base,
              uint64_t       buf_offset,
              uint64_t       buf_size,
              uint64_t       msg_size)
{
    bool success=true;

    char *payload = buf_base;

    payload = buf_base + buf_offset;
    uint32_t seed = *(uint32_t*)(payload+4); // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, ((Bytef*)payload)+4, msg_size-4); // the checksum

    log_debug("test_utils", "seed=0x%x  payload[0]=0x%08x  crc=0x%08x", seed, *(uint32_t*)payload, crc);

    if (*(uint32_t*)payload != crc) {
        log_error("test_utils", "crc mismatch (expected=0x%08x  actual=0x%08x)", *(uint32_t*)payload, (uint32_t)crc);
        success = false;
    }

    return success;
}

bool
verify_buffer(char          *buf_base,
              uint64_t       buf_offset,
              uint64_t       buf_size)
{
    return verify_buffer(buf_base,
                         buf_offset,
                         buf_size,
                         320);
}

NNTI_result_t
wait_data(nnti::transports::transport *t,
          NNTI_event_queue_t           eq)
{
    NNTI_result_t rc = NNTI_OK;

    NNTI_event_t        event;
    uint32_t            which;

    rc = t->eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
    }

    return rc;
}

NNTI_result_t
send_data_async(nnti::transports::transport         *t,
                uint64_t                             length,
                uint64_t                             offset,
                NNTI_buffer_t                        src_hdl,
                NNTI_buffer_t                        dst_hdl,
                NNTI_peer_t                          peer_hdl,
                nnti::datatype::nnti_event_callback &cb,
                void                                *context)
{
    NNTI_result_t rc = NNTI_OK;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint32_t msg_size = length;

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = offset;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = offset;
    base_wr.length        = msg_size;
    base_wr.cb_context    = context;

    nnti::datatype::nnti_work_request wr(t, base_wr, cb);
    NNTI_work_id_t                    wid;

    rc = t->send(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    return rc;
}

NNTI_result_t
send_data_async(nnti::transports::transport *t,
                uint64_t                     length,
                uint64_t                     offset,
                NNTI_buffer_t                src_hdl,
                NNTI_buffer_t                dst_hdl,
                NNTI_peer_t                  peer_hdl)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return send_data_async(t,
                           length,
                           offset,
                           src_hdl,
                           dst_hdl,
                           peer_hdl,
                           obj_cb,
                           nullptr);
}

NNTI_result_t
send_data(nnti::transports::transport          *t,
          uint64_t                             length,
          uint64_t                             offset,
          NNTI_buffer_t                        src_hdl,
          NNTI_buffer_t                        dst_hdl,
          NNTI_peer_t                          peer_hdl,
          NNTI_event_queue_t                   eq,
          nnti::datatype::nnti_event_callback &cb,
          void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    rc = send_data_async(t,
                         length,
                         offset,
                         src_hdl,
                         dst_hdl,
                         peer_hdl,
                         cb,
                         context);

    rc = wait_data(t, eq);

    return rc;
}

NNTI_result_t
send_data(nnti::transports::transport *t,
          uint64_t                             length,
          uint64_t                             offset,
          NNTI_buffer_t                src_hdl,
          NNTI_buffer_t                dst_hdl,
          NNTI_peer_t                  peer_hdl,
          NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return send_data(t,
                     length,
                     offset,
                     src_hdl,
                     dst_hdl,
                     peer_hdl,
                     eq,
                     obj_cb,
                     nullptr);
}

NNTI_result_t
recv_data(nnti::transports::transport *t,
          NNTI_event_queue_t           eq,
          NNTI_event_t                *event)
{
    NNTI_result_t rc=NNTI_OK;

    uint32_t            which;

    uint32_t msgs_received=0;
    while (true) {
        rc = t->eq_wait(&eq, 1, 1000, &which, event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        log_debug_stream("test_utils") << event;

        if (++msgs_received == 1) {
            break;
        }
    }

    return rc;
}

NNTI_result_t
get_data_async(nnti::transports::transport         *t,
               NNTI_buffer_t                        src_hdl,
               uint64_t                             src_offset,
               NNTI_buffer_t                        dst_hdl,
               uint64_t                             dst_offset,
               uint64_t                             length,
               NNTI_peer_t                          peer_hdl,
               nnti::datatype::nnti_event_callback &cb,
               void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "get_data_async - enter");

    base_wr.op            = NNTI_OP_GET;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = dst_hdl;
    base_wr.local_offset  = dst_offset;
    base_wr.remote_hdl    = src_hdl;
    base_wr.remote_offset = src_offset;
    base_wr.length        = length;
    base_wr.cb_context    = context;

    nnti::datatype::nnti_work_request wr(t, base_wr, cb);
    NNTI_work_id_t                    wid;

    rc = t->get(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
    }

    log_debug("test_utils", "get_data_async - exit");

    return rc;
}

NNTI_result_t
get_data_async(nnti::transports::transport         *t,
               NNTI_buffer_t                        src_hdl,
               NNTI_buffer_t                        dst_hdl,
               NNTI_peer_t                          peer_hdl,
               nnti::datatype::nnti_event_callback &cb,
               void                                *context)
{
    return get_data_async(t,
                          src_hdl,
                          0,
                          dst_hdl,
                          0,
                          3200,
                          peer_hdl,
                          cb,
                          context);
}

NNTI_result_t
get_data_async(nnti::transports::transport *t,
               NNTI_buffer_t                src_hdl,
               NNTI_buffer_t                dst_hdl,
               NNTI_peer_t                  peer_hdl)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return get_data_async(t,
                          src_hdl,
                          0,
                          dst_hdl,
                          0,
                          3200,
                          peer_hdl,
                          obj_cb,
                          nullptr);
}

NNTI_result_t
get_data(nnti::transports::transport         *t,
         NNTI_buffer_t                        src_hdl,
         uint64_t                             src_offset,
         NNTI_buffer_t                        dst_hdl,
         uint64_t                             dst_offset,
         uint64_t                             length,
         NNTI_peer_t                          peer_hdl,
         NNTI_event_queue_t                   eq,
         nnti::datatype::nnti_event_callback &cb,
         void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "get_data - enter");

    rc = get_data_async(t,
                        src_hdl,
                        src_offset,
                        dst_hdl,
                        dst_offset,
                        length,
                        peer_hdl,
                        cb,
                        context);

    rc = wait_data(t, eq);

    log_debug("test_utils", "get_data - exit");

    return rc;
}

NNTI_result_t
get_data(nnti::transports::transport         *t,
         NNTI_buffer_t                        src_hdl,
         NNTI_buffer_t                        dst_hdl,
         NNTI_peer_t                          peer_hdl,
         NNTI_event_queue_t                   eq,
         nnti::datatype::nnti_event_callback &cb,
         void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "get_data - enter");

    rc = get_data(t,
                  src_hdl,
                  0,
                  dst_hdl,
                  0,
                  3200,
                  peer_hdl,
                  eq,
                  cb,
                  context);

    log_debug("test_utils", "get_data - exit");

    return rc;
}

NNTI_result_t
get_data(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         NNTI_buffer_t                dst_hdl,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return get_data(t,
                    src_hdl,
                    0,
                    dst_hdl,
                    0,
                    3200,
                    peer_hdl,
                    eq,
                    obj_cb,
                    nullptr);
}

NNTI_result_t
get_data(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         uint64_t                     src_offset,
         NNTI_buffer_t                dst_hdl,
         uint64_t                     dst_offset,
         uint64_t                     length,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return get_data(t,
                    src_hdl,
                    src_offset,
                    dst_hdl,
                    dst_offset,
                    length,
                    peer_hdl,
                    eq,
                    obj_cb,
                    nullptr);
}

NNTI_result_t
put_data_async(nnti::transports::transport         *t,
               NNTI_buffer_t                        src_hdl,
               uint64_t                             src_offset,
               NNTI_buffer_t                        dst_hdl,
               uint64_t                             dst_offset,
               uint64_t                             length,
               NNTI_peer_t                          peer_hdl,
               nnti::datatype::nnti_event_callback &cb,
               void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "put_data_async - enter");

    base_wr.op            = NNTI_OP_PUT;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = src_offset;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = dst_offset;
    base_wr.length        = length;
    base_wr.cb_context    = context;

    nnti::datatype::nnti_work_request wr(t, base_wr, cb);
    NNTI_work_id_t                    wid;

    rc = t->put(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
    }

    log_debug("test_utils", "put_data_async - exit");

    return rc;
}

NNTI_result_t
put_data_async(nnti::transports::transport         *t,
               NNTI_buffer_t                        src_hdl,
               NNTI_buffer_t                        dst_hdl,
               NNTI_peer_t                          peer_hdl,
               nnti::datatype::nnti_event_callback &cb,
               void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "put_data_async - enter");

    rc = put_data_async(t,
                        src_hdl,
                        0,
                        dst_hdl,
                        0,
                        3200,
                        peer_hdl,
                        cb,
                        context);

    log_debug("test_utils", "put_data_async - exit");

    return rc;
}

NNTI_result_t
put_data_async(nnti::transports::transport *t,
               NNTI_buffer_t                src_hdl,
               NNTI_buffer_t                dst_hdl,
               NNTI_peer_t                  peer_hdl)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return put_data_async(t,
                          src_hdl,
                          0,
                          dst_hdl,
                          0,
                          3200,
                          peer_hdl,
                          obj_cb,
                          nullptr);
}

NNTI_result_t
put_data(nnti::transports::transport         *t,
         NNTI_buffer_t                        src_hdl,
         uint64_t                             src_offset,
         NNTI_buffer_t                        dst_hdl,
         uint64_t                             dst_offset,
         uint64_t                             length,
         NNTI_peer_t                          peer_hdl,
         NNTI_event_queue_t                   eq,
         nnti::datatype::nnti_event_callback &cb,
         void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "put_data - enter");

    rc = put_data_async(t,
                        src_hdl,
                        src_offset,
                        dst_hdl,
                        dst_offset,
                        length,
                        peer_hdl,
                        cb,
                        context);

    rc = wait_data(t, eq);

    log_debug("test_utils", "put_data - exit");

    return rc;
}

NNTI_result_t
put_data(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         uint64_t                     src_offset,
         NNTI_buffer_t                dst_hdl,
         uint64_t                     dst_offset,
         uint64_t                     length,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return put_data(t,
                    src_hdl,
                    src_offset,
                    dst_hdl,
                    dst_offset,
                    length,
                    peer_hdl,
                    eq,
                    obj_cb,
                    nullptr);
}

NNTI_result_t
put_data(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         NNTI_buffer_t                dst_hdl,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return put_data(t,
                    src_hdl,
                    0,
                    dst_hdl,
                    0,
                    3200,
                    peer_hdl,
                    eq,
                    obj_cb,
                    nullptr);
}

NNTI_result_t
fadd_async(nnti::transports::transport         *t,
           NNTI_buffer_t                        src_hdl,
           NNTI_buffer_t                        dst_hdl,
           uint64_t                             length,
           int64_t                              operand,
           NNTI_peer_t                          peer_hdl,
           nnti::datatype::nnti_event_callback &cb,
           void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "fadd_async - enter");

    base_wr.op            = NNTI_OP_ATOMIC_FADD;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = 0;
    base_wr.operand1      = operand;
    base_wr.length        = length;
    base_wr.cb_context    = context;

    nnti::datatype::nnti_work_request wr(t, base_wr, cb);
    NNTI_work_id_t                    wid;

    rc = t->atomic_fop(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
    }

    log_debug("test_utils", "fadd_async - exit");

    return rc;
}

NNTI_result_t
fadd_async(nnti::transports::transport *t,
           NNTI_buffer_t                src_hdl,
           NNTI_buffer_t                dst_hdl,
           int64_t                      operand,
           NNTI_peer_t                  peer_hdl)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return fadd_async(t,
                      src_hdl,
                      dst_hdl,
                      8,
                      operand,
                      peer_hdl,
                      obj_cb,
                      nullptr);
}

NNTI_result_t
fadd(nnti::transports::transport         *t,
         NNTI_buffer_t                        src_hdl,
         NNTI_buffer_t                        dst_hdl,
         uint64_t                             length,
         int64_t                              operand,
         NNTI_peer_t                          peer_hdl,
         NNTI_event_queue_t                   eq,
         nnti::datatype::nnti_event_callback &cb,
         void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "fadd - enter");

    rc = fadd_async(t,
                    src_hdl,
                    dst_hdl,
                    length,
                    operand,
                    peer_hdl,
                    cb,
                    context);

    rc = wait_data(t, eq);

    log_debug("test_utils", "fadd - exit");

    return rc;
}

NNTI_result_t
fadd(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         NNTI_buffer_t                dst_hdl,
         int64_t                      operand,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return fadd(t,
                src_hdl,
                dst_hdl,
                8,
                operand,
                peer_hdl,
                eq,
                obj_cb,
                nullptr);
}

NNTI_result_t
cswap_async(nnti::transports::transport         *t,
           NNTI_buffer_t                        src_hdl,
           NNTI_buffer_t                        dst_hdl,
           uint64_t                             length,
           int64_t                              operand1,
           int64_t                              operand2,
           NNTI_peer_t                          peer_hdl,
           nnti::datatype::nnti_event_callback &cb,
           void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "cswap_async - enter");

    base_wr.op            = NNTI_OP_ATOMIC_CSWAP;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = 0;
    base_wr.operand1      = operand1;
    base_wr.operand2      = operand2;
    base_wr.length        = length;
    base_wr.cb_context    = context;

    nnti::datatype::nnti_work_request wr(t, base_wr, cb);
    NNTI_work_id_t                    wid;

    rc = t->atomic_cswap(&wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
    }

    log_debug("test_utils", "cswap_async - exit");

    return rc;
}

NNTI_result_t
cswap_async(nnti::transports::transport *t,
           NNTI_buffer_t                src_hdl,
           NNTI_buffer_t                dst_hdl,
           int64_t                      operand1,
           int64_t                      operand2,
           NNTI_peer_t                  peer_hdl)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return cswap_async(t,
                      src_hdl,
                      dst_hdl,
                      8,
                      operand1,
                      operand2,
                      peer_hdl,
                      obj_cb,
                      nullptr);
}

NNTI_result_t
cswap(nnti::transports::transport         *t,
         NNTI_buffer_t                        src_hdl,
         NNTI_buffer_t                        dst_hdl,
         int64_t                              operand1,
         int64_t                              operand2,
         NNTI_peer_t                          peer_hdl,
         NNTI_event_queue_t                   eq,
         nnti::datatype::nnti_event_callback &cb,
         void                                *context)
{
    NNTI_result_t rc=NNTI_OK;

    log_debug("test_utils", "cswap - enter");

    rc = cswap_async(t,
                    src_hdl,
                    dst_hdl,
                    8,
                    operand1,
                    operand2,
                    peer_hdl,
                    cb,
                    context);

    rc = wait_data(t, eq);

    log_debug("test_utils", "cswap - exit");

    return rc;
}

NNTI_result_t
cswap(nnti::transports::transport *t,
         NNTI_buffer_t                src_hdl,
         NNTI_buffer_t                dst_hdl,
         int64_t                      operand1,
         int64_t                      operand2,
         NNTI_peer_t                  peer_hdl,
         NNTI_event_queue_t           eq)
{
    nnti::datatype::nnti_event_callback obj_cb(t, callback());

    return cswap(t,
                src_hdl,
                dst_hdl,
                operand1,
                operand2,
                peer_hdl,
                eq,
                obj_cb,
                nullptr);
}
