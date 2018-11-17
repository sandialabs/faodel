// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <functional>

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include "faodel-common/Configuration.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/NodeID.hh"

#include "webhook/Server.hh"

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"

#include "test_utils.h"

std::string default_config_string = R"EOF(
nnti.transport.name                           mpi
)EOF";

extern "C"
void
test_bootstrap_start(void)
{
    faodel::Configuration config;

    config = faodel::Configuration(default_config_string);
    config.AppendFromReferences();

    faodel::bootstrap::Start(config, webhook::bootstrap);

}

extern "C"
void
test_bootstrap_finish(void)
{
    faodel::bootstrap::Finish();

}


extern "C"
NNTI_result_t
cb_func(NNTI_event_t *event, void *context)
{
    log_debug("test_utils", "This is a callback function.  My parameters are event(%p) and context(%p).", (void*)event, (void*)context);
    return NNTI_EIO;
}

extern "C"
NNTI_result_t
send_target_hdl(NNTI_transport_t  transport,
    NNTI_buffer_t                 send_hdl,
    char                         *send_base,
    uint64_t                      send_size,
    NNTI_buffer_t                 target_hdl,
    NNTI_peer_t                   peer_hdl,
    NNTI_event_queue_t            eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint64_t packed_size;
    rc = NNTI_dt_sizeof(transport, (void*)target_hdl, &packed_size);
    if (rc != NNTI_OK) {
        log_error("test_utils", "dt_sizeof() failed: %d", rc);
    }
    rc = NNTI_dt_pack(transport, (void*)target_hdl, send_base, send_size);
    if (rc != NNTI_OK) {
        log_error("test_utils", "dt_pack() failed: %d", rc);
    }

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = send_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length        = packed_size;

    base_wr.callback      = cb_func;

    NNTI_work_id_t wid;

    rc = NNTI_send(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
recv_target_hdl(NNTI_transport_t transport,
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
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint32_t msgs_received=0;
    while (TRUE) {
        rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        uint64_t dst_offset = msgs_received * 320;
        rc =  NNTI_next_unexpected(recv_hdl, dst_offset, &result_event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "next_unexpected() failed: %d", rc);
        }
        if (++msgs_received == 1) {
            break;
        }
    }

    // create an nnti_buffer from a packed buffer sent from the client
    NNTI_dt_unpack(transport, (void*)target_hdl, recv_base, event.length);

    *peer_hdl   = event.peer;

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
send_hdl(NNTI_transport_t    transport,
         NNTI_buffer_t       hdl,
         char               *hdl_base,
         uint64_t            hdl_size,
         NNTI_peer_t         peer_hdl,
         NNTI_event_queue_t  eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint64_t packed_size;
    rc = NNTI_dt_sizeof(transport, (void*)hdl, &packed_size);
    if (rc != NNTI_OK) {
        log_error("test_utils", "dt_sizeof() failed: %d", rc);
    }
    rc = NNTI_dt_pack(transport, (void*)hdl, hdl_base, hdl_size);
    if (rc != NNTI_OK) {
        log_error("test_utils", "dt_pack() failed: %d", rc);
    }

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
    base_wr.remote_offset = 0;
    base_wr.length        = packed_size;

    base_wr.callback      = cb_func;

    NNTI_work_id_t wid;

    rc = NNTI_send(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
recv_hdl(NNTI_transport_t    transport,
         NNTI_buffer_t       recv_hdl,
         char               *recv_base,
         uint32_t            recv_size,
         NNTI_buffer_t      *hdl,
         NNTI_peer_t        *peer_hdl,
         NNTI_event_queue_t  eq)
{
    NNTI_result_t rc = NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint32_t msgs_received=0;
    while (TRUE) {
        rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        //log_debug_stream("test_utils") << event;
        uint64_t dst_offset = msgs_received * 320;
        rc =  NNTI_next_unexpected(recv_hdl, dst_offset, &result_event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "next_unexpected() failed: %d", rc);
        }
        if (++msgs_received == 1) {
            break;
        }
    }

    // create an nnti_buffer from a packed buffer sent from the client
    NNTI_dt_unpack(transport, (void*)hdl, recv_base, event.length);

    *peer_hdl   = event.peer;

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
send_ack(NNTI_transport_t    transport,
         NNTI_buffer_t       hdl,
         NNTI_buffer_t       ack_hdl,
         NNTI_peer_t         peer_hdl,
         NNTI_event_queue_t  eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "send_ack - enter");

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = ack_hdl;
    base_wr.remote_offset = 0;
    base_wr.length        = 64;

    NNTI_work_id_t wid;

    rc = NNTI_send(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    log_debug("test_utils", "send_ack - exit");

    return rc;
}

extern "C"
NNTI_result_t
recv_ack(NNTI_transport_t    transport,
         NNTI_buffer_t       ack_hdl,
         NNTI_peer_t        *peer_hdl,
         NNTI_event_queue_t  eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "recv_ack - enter");

    uint32_t msgs_received=0;
    while (TRUE) {
        rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }
        if (++msgs_received == 1) {
            break;
        }
    }

    *peer_hdl   = event.peer;

cleanup:
    log_debug("test_utils", "recv_ack - exit");

    return rc;
}

extern "C"
NNTI_result_t
populate_buffer(NNTI_transport_t transport,
                uint32_t         seed,
                uint64_t         offset_multiplier,
                NNTI_buffer_t    buf_hdl,
                char            *buf_base,
                uint64_t         buf_size)
{
    NNTI_result_t rc = NNTI_OK;

    char packed[312];
    int  packed_size = 312;

    rc = NNTI_dt_pack(transport, (void*)buf_hdl, packed, 312);
    if (rc != NNTI_OK) {
        log_error("test_utils", "dt_pack() failed: %d", rc);
    }

    char *payload = buf_base + ((packed_size+4+4)*offset_multiplier);

    memcpy(payload+8, packed, packed_size);  // the data
    *(uint32_t*)(payload+4) = seed;          // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, ((Bytef*)payload)+4, 316); // the checksum
    *(uint32_t*)payload = 0;
    *(uint32_t*)payload = crc;

    log_debug("test_utils", "seed=0x%x  payload=%p  payload[0]=%08x  crc=%08x", seed, payload, *(uint32_t*)payload, crc);

    return NNTI_OK;
}

extern "C"
int
verify_buffer(char          *buf_base,
              uint64_t       buf_offset,
              uint64_t       buf_size)
{
    int success=TRUE;

    char     *payload = buf_base;

    payload = buf_base + buf_offset;
    uint32_t seed = *(uint32_t*)(payload+4); // the salt

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, ((Bytef*)payload)+4, 316); // the checksum

    log_debug("test_utils", "seed=0x%x  payload[0]=0x%08x  crc=0x%08x", seed, *(uint32_t*)payload, crc);

    if (*(uint32_t*)payload != crc) {
        log_error("test_utils", "crc mismatch (expected=0x%08x  actual=0x%08x)", *(uint32_t*)payload, (uint32_t)crc);
        success = FALSE;
    }

    return success;
}

extern "C"
NNTI_result_t
send_data(NNTI_transport_t   transport,
          uint64_t           offset_multiplier,
          NNTI_buffer_t      src_hdl,
          NNTI_buffer_t      dst_hdl,
          NNTI_peer_t        peer_hdl,
          NNTI_event_queue_t eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint32_t msg_size = 320;

    base_wr.op            = NNTI_OP_SEND;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = offset_multiplier * msg_size;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = offset_multiplier * msg_size;
    base_wr.length        = msg_size;

    base_wr.callback      = cb_func;

    NNTI_work_id_t wid;

    rc = NNTI_send(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
recv_data(NNTI_transport_t transport,
          NNTI_event_queue_t           eq,
          NNTI_event_t                *event)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    uint32_t msgs_received=0;
    while (TRUE) {
        rc = NNTI_eq_wait(&eq, 1, 1000, &which, event);
        if (rc != NNTI_OK) {
            log_error("test_utils", "eq_wait() failed: %d", rc);
            continue;
        }

        if (++msgs_received == 1) {
            break;
        }
    }

cleanup:
    return rc;
}

extern "C"
NNTI_result_t
get_data(NNTI_transport_t   transport,
         NNTI_buffer_t      src_hdl,
         NNTI_buffer_t      dst_hdl,
         NNTI_peer_t        peer_hdl,
         NNTI_event_queue_t eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "get_data - enter");

    base_wr.op            = NNTI_OP_GET;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = dst_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = src_hdl;
    base_wr.remote_offset = 0;
    base_wr.length        = 3200;

    NNTI_work_id_t wid;

    rc = NNTI_get(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    log_debug("test_utils", "get_data - exit");

    return rc;
}

extern "C"
NNTI_result_t
put_data(NNTI_transport_t   transport,
         NNTI_buffer_t      src_hdl,
         NNTI_buffer_t      dst_hdl,
         NNTI_peer_t        peer_hdl,
         NNTI_event_queue_t eq)
{
    NNTI_result_t rc=NNTI_OK;

    NNTI_event_t        event;
    NNTI_event_t        result_event;
    uint32_t            which;
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

    log_debug("test_utils", "put_data - enter");

    base_wr.op            = NNTI_OP_PUT;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = transport;
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = src_hdl;
    base_wr.local_offset  = 0;
    base_wr.remote_hdl    = dst_hdl;
    base_wr.remote_offset = 0;
    base_wr.length        = 3200;

    NNTI_work_id_t wid;

    rc = NNTI_put(&base_wr, &wid);
    if (rc != NNTI_OK) {
        log_error("test_utils", "send() failed: %d", rc);
        goto cleanup;
    }
    rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
    if (rc != NNTI_OK) {
        log_error("test_utils", "eq_wait() failed: %d", rc);
        goto cleanup;
    }

cleanup:
    log_debug("test_utils", "put_data - exit");

    return rc;
}
