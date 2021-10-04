// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nnti/nnti.h"

#include "test_utils.h"

int success=1;


int
main(int argc, char *argv[])
{
    NNTI_result_t rc;
    NNTI_transport_id_t transport_id = NNTI_DEFAULT_TRANSPORT;
    NNTI_transport_t transport;
    char *req_url = NULL;
    char *my_url = (char *)calloc(NNTI_URL_LEN+1,1);

    NNTI_buffer_t       dst_buf;
    char               *dst_base=NULL;

    test_bootstrap_start();

    rc = NNTI_init(transport_id, req_url, &transport);
    if (rc != NNTI_OK) {
        success = 0;
        goto cleanup;
    }

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    if (rc != NNTI_OK) {
        success = 0;
        goto cleanup;
    }
    if (is_init != 1) {
        success = 0;
        goto cleanup;
    }

    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    if (rc != NNTI_OK) {
        success = 0;
        goto cleanup;
    }

    rc = NNTI_alloc(transport, 3200, NNTI_BF_LOCAL_WRITE, (NNTI_event_queue_t)0, (NNTI_event_callback_t)0, NULL, &dst_base, &dst_buf);
    if (rc != NNTI_OK) {
        success = 0;
        goto cleanup;
    }

    rc = NNTI_free(dst_buf);
    if (rc != NNTI_OK) {
        success = 0;
        goto cleanup;
    }

    NNTI_fini(transport);
    test_bootstrap_finish();

cleanup:
    if (success) {
        fprintf(stdout, "\nEnd Result: TEST PASSED\n");
    } else {
        fprintf(stdout, "\nEnd Result: TEST FAILED\n");
    }

    return (success ? 0 : 1 );
}
