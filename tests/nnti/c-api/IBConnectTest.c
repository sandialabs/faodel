// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>

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
    log_debug("IBConnectTest", "Init ran");

    int is_init=-1;
    rc = NNTI_initialized(transport_id, &is_init);
    assert(rc==NNTI_OK);
    assert(is_init==1);

    log_debug("IBConnectTest", "Is initialized");
    rc = NNTI_get_url(transport, my_url, NNTI_URL_LEN);
    //Dies here?
    assert(rc==NNTI_OK);

    log_debug("IBConnectTest", "my_url=%s", my_url);

    gethostname(my_hostname, NNTI_HOSTNAME_LEN);

    sprintf(server_url, "ib://%s:1990/", server_hostname);

    int i_am_server = !strcmp(server_hostname, my_hostname);

    if (i_am_server) {
        // sleep this thread while clients connect/disconnect
        sleep(10);
    } else {
        // give the server a chance to startup
        sleep(2);
        rc = NNTI_connect(transport, server_url, 1000, &peer_hdl);
        log_debug("IBConnectTest", "NNTI_connect() rc=%d", rc);
        if (rc != NNTI_OK) {
            success = 0;
        } else {
            rc = NNTI_disconnect(transport, peer_hdl);
            log_debug("IBConnectTest", "NNTI_disconnect() rc=%d", rc);
            if (rc != NNTI_OK) {
                success = 0;
            }
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
