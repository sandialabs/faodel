// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"


int success=1;


int main(int argc, char *argv[])
{
    NNTI_result_t       rc;
    NNTI_transport_id_t transport_id = NNTI_TRANSPORT_IBVERBS;
    NNTI_transport_t    transport;
    NNTI_peer_t         peer_hdl;

    int i=0;

    char *req_url=NULL;

    char  my_hostname[NNTI_HOSTNAME_LEN];
    char *my_url = (char *)calloc(NNTI_URL_LEN+1,1);

    char *server_hostname=argv[1];
    char *server_url = (char *)calloc(NNTI_URL_LEN+1,1);

    setenv("NNTI_LOG_FILENAME", "IBConnectTest.log", 1);
    setenv("NNTI_LOG_FILEPER", "1", 1);
    setenv("NNTI_LOG_LEVEL", "DEBUG", 1);

    rc = NNTI_init(transport_id, req_url, &transport);
    assert(rc==NNTI_OK);
    log_debug("IBUnexpectedSendTest", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("IBUnexpectedSendTest", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("IBUnexpectedSendTest", "my_url=%s", my_url);

    gethostname(my_hostname, NNTI_HOSTNAME_LEN);

    sprintf(server_url, "ib://%s:1990/", server_hostname);

    int i_am_server = !strcmp(server_hostname, my_hostname);

    if (i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       dst_buf;
        char               *dst_base=NULL;
        uint32_t            dst_size=3200;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        rc = NNTI_alloc(transport, dst_size, NNTI_BF_LOCAL_WRITE, (NNTI_event_queue_t)0, NULL, NULL, &dst_base, &dst_buf);

        rc = NNTI_eq_create(transport, 128, NNTI_EQF_UNEXPECTED, NULL, NULL, &eq);

//        nnti::util::sleep(1000);

        uint32_t msgs_received=0;
        while (1) {
            rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
            if (rc != NNTI_OK) {
                log_error("IBUnexpectedSendTest", "eq_wait() failed: %d", rc);
                continue;
            }
            uint64_t dst_offset = msgs_received * 320;
            rc =  NNTI_next_unexpected(dst_buf, dst_offset, &result_event);
            if (rc != NNTI_OK) {
                log_error("IBUnexpectedSendTest", "next_unexpected() failed: %d", rc);
            }
            if (++msgs_received == 10) {
                break;
            }
        }
        for (i=0;i<10;i++) {
            char *payload = dst_base + (i * 320);

            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, ((Bytef*)payload)+4, 316);

            log_debug("IBUnexpectedSendTest", "crc(%d)=%08x", i, crc);

            if (*(uint32_t*)payload != crc) {
                log_error("IBUnexpectedSendTest", "crc mismatch (expected=%08x  actual=%08x)", *(uint32_t*)payload, (uint32_t)crc);
                success = 0;
            }
        }

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = transport;
        base_wr.peer          = event.peer;
        base_wr.local_hdl     = dst_buf;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = 320;

        for (i=0;i<10;i++) {
            base_wr.local_offset = i * 320;

            NNTI_work_id_t wid;

            char *payload = dst_base + (i * 320);
            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, ((Bytef*)payload)+4, 316);
            log_debug("IBUnexpectedSendTest", "payload(%d)=%08x  crc(%d)=%08x", i, *(uint32_t*)payload, i, crc);

            rc = NNTI_send(&base_wr, &wid);
        }
        for (i=0;i<10;i++) {
            rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
        }

        // sleep this thread while clients connect/disconnect
        sleep(1);

    } else {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       reg_buf;
        char               *reg_base=NULL;
        uint32_t            reg_size=320;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
        NNTI_work_id_t      wid;

        // give the server a chance to startup
        sleep(1);

        rc = NNTI_connect(transport, server_url, 1000, &peer_hdl);
        log_debug("IBUnexpectedSendTest", "NNTI_connect() rc=%d", rc);
        if (rc != NNTI_OK) {
            success = 0;
        }
        rc = NNTI_eq_create(transport, 128, NNTI_EQF_UNEXPECTED, NULL, NULL, &eq);
        rc = NNTI_alloc(transport, reg_size, NNTI_BF_LOCAL_READ, eq, NULL, NULL, &reg_base, &reg_buf);

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = transport;
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = reg_buf;
        base_wr.local_offset  = 0;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = 320;

        char *payload = reg_base;
        uint64_t packed_size;
        rc = NNTI_dt_sizeof(transport, (void*)reg_buf, &packed_size);
        if (rc != NNTI_OK) {
            log_fatal("IBUnexpectedSendTest", "dt_sizeof() failed: %d", rc);
        }
        rc = NNTI_dt_pack(transport, (void*)reg_buf, payload+8, 256);
        if (rc != NNTI_OK) {
            log_fatal("IBUnexpectedSendTest", "dt_pack() failed: %d", rc);
        }

        for (i=0;i<10;i++) {
            *(uint32_t*)(payload+4) = i;

            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, ((Bytef*)payload)+4, 316);
            *(uint32_t*)payload = 0;
            *(uint32_t*)payload = crc;

            log_debug("IBUnexpectedSendTest", "payload(%d)=%08x  crc(%d)=%08x", i, *(uint32_t*)payload, i, crc);

            rc = NNTI_send(&base_wr, &wid);
        }
        for (i=0;i<10;i++) {
            rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
            if (rc != NNTI_OK) {
                log_error("IBUnexpectedSendTest", "eq_wait() failed: %d", rc);
            }
        }

        NNTI_buffer_t  dst_buf;
        char          *dst_base=NULL;
        uint32_t       dst_size=3200;
        NNTI_alloc(transport, dst_size, NNTI_BF_LOCAL_WRITE, (NNTI_event_queue_t)0, NULL, NULL, &dst_base, &dst_buf);

        uint32_t msgs_received=0;
        while (1) {
            rc = NNTI_eq_wait(&eq, 1, 1000, &which, &event);
            if (rc != NNTI_OK) {
                log_error("IBUnexpectedSendTest", "eq_wait() failed: %d", rc);
                continue;
            }
            uint64_t dst_offset = msgs_received * 320;
            rc =  NNTI_next_unexpected(dst_buf, dst_offset, &result_event);
            if (rc != NNTI_OK) {
                log_error("IBUnexpectedSendTest", "next_unexpected() failed: %d", rc);
            }
            if (++msgs_received == 10) {
                break;
            }
        }
        for (i=0;i<10;i++) {
            char *payload = dst_base + (i * 320);

            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, ((Bytef*)payload)+4, 316);

            log_debug("IBUnexpectedSendTest", "crc(%d)=%08x", i, crc);

            if (*(uint32_t*)payload != crc) {
                log_error("IBUnexpectedSendTest", "crc mismatch (expected=%08x  actual=%08x)", *(uint32_t*)payload, (uint32_t)crc);
                success = 0;
            }
        }

//        nnti::util::sleep(3000);

        rc = NNTI_disconnect(transport, peer_hdl);
        log_debug("IBUnexpectedSendTest", "NNTI_disconnect() rc=%d", rc);
        if (rc != NNTI_OK) {
            success = 0;
        }
    }

    if (is_init) {
        NNTI_fini(transport);
    } else {
        success = 0;
    }

    if (success)
        fprintf(stdout, "\nEnd Result: TEST PASSED\n");
    else
        fprintf(stdout, "\nEnd Result: TEST FAILED\n");

    return (success ? 0 : 1 );
}
