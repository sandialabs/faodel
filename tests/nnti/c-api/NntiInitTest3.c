// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"

#include "test_utils.h"

int
main(int argc, char *argv[])
{
    NNTI_result_t rc;
    NNTI_transport_id_t transport_id = NNTI_DEFAULT_TRANSPORT;
    NNTI_transport_t transport;

    char my_hostname[NNTI_HOSTNAME_LEN];
    gethostname(my_hostname, NNTI_HOSTNAME_LEN);
    char req_url[NNTI_URL_LEN];
    char *my_url = (char *)calloc(NNTI_URL_LEN+1,1);

    test_bootstrap_start();

    sprintf(req_url, "ib://%s", my_hostname);

    rc = NNTI_init(transport_id, req_url, &transport);
    assert(rc==NNTI_OK);
    log_debug("NntiInitTest3", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("NntiInitTest3", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("NntiInitTest3", "my_url=%s", my_url);

    NNTI_fini(transport);
    test_bootstrap_finish();

    return 0;
}
