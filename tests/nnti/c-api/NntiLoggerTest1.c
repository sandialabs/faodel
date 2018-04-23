// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <assert.h>
#include <stdio.h>

#include <stdlib.h>

#include <sys/stat.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"

#include "test_utils.h"

int success=TRUE;


int
main(int argc, char *argv[])
{
    NNTI_result_t rc;
    NNTI_transport_id_t transport_id = NNTI_DEFAULT_TRANSPORT;
    NNTI_transport_t transport;
    char *req_url = NULL;
    char *my_url = (char *)calloc(NNTI_URL_LEN+1,1);

    struct stat sb;
    char logfile[1024];

    char *log_filename="NntiLoggerTest1.log";

    setenv("NNTI_LOG_FILENAME", log_filename, 1);
    setenv("NNTI_LOG_FILEPER", "1", 1);
    setenv("NNTI_LOG_LEVEL", "DEBUG", 1);

    test_bootstrap();

    rc = NNTI_init(transport_id, req_url, &transport);
    assert(rc==NNTI_OK);
    log_debug("NntiLoggetTest1", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("NntiLoggetTest1", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("NntiLoggetTest1", "my_url=%s", my_url);

    NNTI_fini(transport);

    sprintf(logfile, "%s.%d.log", log_filename, getpid());
    if (stat(logfile, &sb) == -1) {
        perror("stat");
        success=FALSE;
    }

    if (success)
        fprintf(stdout, "\nEnd Result: TEST PASSED\n");
    else
        fprintf(stdout, "\nEnd Result: TEST FAILED\n");

    return (success ? 0 : 1 );
}
