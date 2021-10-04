// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include "faodel-common/Common.hh"

#include "nnti/nntiConfig.h"

#include <string.h>
#include <sys/mman.h>

#include <iostream>

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/transport_factory.hpp"

#include "bench_utils.hpp"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";


int main(int argc, char *argv[])
{
    NNTI_result_t rc;
    nnti::transports::transport *t=nullptr;

    Configuration config;
    config = Configuration(default_config_string);
    config.AppendFromReferences();

    test_setup(0,
               NULL,
               config,
               "MemoryRegistrations",
               t);

    nnti::datatype::nnti_event_callback null_cb(t, (NNTI_event_callback_t)0);

    char url[128];
    t->get_url(&url[0], 128);
    std::cout << url << std::endl;

    const int allocation_count=128;

    NNTI_buffer_t  buf[allocation_count];
    char          *base[allocation_count];

    for (int i=0;i<allocation_count;i++) {
        base[i] = (char*)malloc(8192);

        auto start = std::chrono::high_resolution_clock::now();
        memset(base[i], 0, 8192);
        auto end = std::chrono::high_resolution_clock::now();
        cout << "memset 0 time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << endl;

        start = std::chrono::high_resolution_clock::now();
        mlock(base[i], 8192);
        end = std::chrono::high_resolution_clock::now();
        cout << "mlock time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << endl;

        start = std::chrono::high_resolution_clock::now();
        rc = t->register_memory(
            base[i],
            8192,
            NNTI_BF_LOCAL_WRITE,
            (NNTI_event_queue_t)0,
            null_cb,
            nullptr,
            &buf[i]);
        end = std::chrono::high_resolution_clock::now();
        cout << "register time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << endl;
    }
    for (int i=0;i<allocation_count;i++) {
        auto start = std::chrono::high_resolution_clock::now();
        rc = t->unregister_memory(buf[i]);
        auto end = std::chrono::high_resolution_clock::now();
        cout << "unregister time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us" << endl;

        free(base[i]);
    }

    rc = t->stop();

    return (rc);
}
