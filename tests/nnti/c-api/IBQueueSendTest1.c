// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"

#include "test_utils.h"

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

    setenv("NNTI_LOG_FILENAME", "IBQueueSendTest1.log", 1);
    setenv("NNTI_LOG_FILEPER", "1", 1);
    setenv("NNTI_LOG_LEVEL", "DEBUG", 1);

    rc = NNTI_init(transport_id, req_url, &transport);
    assert(rc==NNTI_OK);
    log_debug("IBQueueSendTest1", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("IBQueueSendTest1", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("IBQueueSendTest1", "my_url=%s", my_url);

    gethostname(my_hostname, NNTI_HOSTNAME_LEN);

    sprintf(server_url, "ib://%s:1990/", server_hostname);

    int i_am_server = !strcmp(server_hostname, my_hostname);

    if (i_am_server) {
        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       src_hdl;
        char               *src_base=NULL;
        uint32_t            src_size=320;
        NNTI_buffer_t       my_q_hdl;
        char               *my_q_base=NULL;
        uint32_t            my_q_size=3200;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;

        rc = NNTI_eq_create(transport, 128, NNTI_EQF_UNEXPECTED, NULL, NULL, &eq);
        rc = NNTI_alloc(transport,
                        src_size,
                        (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                        eq,
                        NULL,
                        NULL,
                        &src_base,
                        &src_hdl);
        rc = NNTI_alloc(transport,
                        my_q_size,
                        (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE|NNTI_BF_QUEUING),
                        eq,
                        cb_func,
                        NULL,
                        &my_q_base,
                        &my_q_hdl);

        NNTI_buffer_t target_hdl;
        NNTI_peer_t   peer_hdl;

        rc = recv_target_hdl(transport, src_hdl, src_base, &target_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("IBQueueSendTest1", "recv_target_hdl() failed: %d", rc);
        }

        rc = send_target_hdl(transport, src_hdl, src_base, src_size, my_q_hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("IBQueueSendTest1", "send_target_hdl() failed: %d", rc);
        }

        int i=0;
        for (i=0;i<10;i++) {
            rc = recv_data(transport, eq, &event);
            if (verify_buffer((char*)event.start, event.offset, event.length) == 0) {
                success = 0;
            }
            rc = NNTI_event_complete(&event);
        }

        for (i=0;i<10;i++) {
            rc = populate_buffer(transport, i, 0, src_hdl, src_base, src_size);
            rc = send_data(transport, 0, src_hdl, target_hdl, peer_hdl, eq);
        }

        // sleep this thread while clients connect/disconnect
        sleep(1);

    } else {

        NNTI_event_queue_t  eq;
        NNTI_event_t        event;
        NNTI_event_t        result_event;
        uint32_t            which;
        NNTI_buffer_t       src_hdl;
        char               *src_base=NULL;
        uint32_t            src_size=320;
        NNTI_buffer_t       my_q_hdl;
        char               *my_q_base=NULL;
        uint32_t            my_q_size=3200;
        NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
        NNTI_work_id_t      wid;

        // give the server a chance to startup
        sleep(1);

        rc = NNTI_connect(transport, server_url, 1000, &peer_hdl);
        log_debug("IBQueueSendTest1", "NNTI_connect() rc=%d", rc);
        if (rc != NNTI_OK) {
            success = 0;
        }
        rc = NNTI_eq_create(transport, 128, NNTI_EQF_UNEXPECTED, NULL, NULL, &eq);
        rc = NNTI_alloc(transport,
                        src_size,
                        (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
                        eq,
                        NULL,
                        NULL,
                        &src_base,
                        &src_hdl);
        rc = NNTI_alloc(transport,
                        my_q_size,
                        (NNTI_buffer_flags_t)(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE|NNTI_BF_QUEUING),
                        eq,
                        cb_func,
                        NULL,
                        &my_q_base,
                        &my_q_hdl);

        NNTI_buffer_t target_hdl;
        NNTI_peer_t   recv_peer;

        rc = send_target_hdl(transport, src_hdl, src_base, src_size, my_q_hdl, peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("IBQueueSendTest1", "send_target_hdl() failed: %d", rc);
        }

        rc = recv_target_hdl(transport, src_hdl, src_base, &target_hdl, &peer_hdl, eq);
        if (rc != NNTI_OK) {
            log_error("IBQueueSendTest1", "recv_target_hdl() failed: %d", rc);
        }

        int i=0;
        for (i=0;i<10;i++) {
            rc = populate_buffer(transport, i, 0, src_hdl, src_base, src_size);
            rc = send_data(transport, 0, src_hdl, target_hdl, peer_hdl, eq);
        }

        for (i=0;i<10;i++) {
            rc = recv_data(transport, eq, &event);
            if (verify_buffer((char*)event.start, event.offset, event.length) == 0) {
                success = 0;
            }
            rc = NNTI_event_complete(&event);
        }

        rc = NNTI_disconnect(transport, peer_hdl);
        log_debug("IBQueueSendTest1", "NNTI_disconnect() rc=%d", rc);
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
