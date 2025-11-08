// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <assert.h>
#include <stdio.h>

#include <stdlib.h>

#include <sys/stat.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"

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

    struct stat sb;

    char *log_filename="NntiLoggerTest2.log";

    setenv("NNTI_LOG_FILENAME", log_filename, 1);
    setenv("NNTI_LOG_FILEPER", "0", 1);
    setenv("NNTI_LOG_LEVEL", "DEBUG", 1);

    test_bootstrap_start();

    rc = NNTI_init(transport_id, req_url, &transport);
    assert(rc==NNTI_OK);
    log_debug("NntiLoggetTest2", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("NntiLoggetTest2", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("NntiLoggetTest2", "my_url=%s", my_url);

    NNTI_fini(transport);
    test_bootstrap_finish();

    if (stat(log_filename, &sb) == -1) {
        perror("stat");
        success=0;
    }

    if (success)
        fprintf(stdout, "\nEnd Result: TEST PASSED\n");
    else
        fprintf(stdout, "\nEnd Result: TEST FAILED\n");

    return (success ? 0 : 1 );
}
