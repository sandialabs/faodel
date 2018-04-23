// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include <iostream>
#include <sstream>
#include <thread>

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_util.hpp"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/transport_factory.hpp"

#include "test_utils.hpp"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class NntiAllocFreeTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

    virtual void SetUp () {
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        test_setup(0,
                   NULL,
                   config,
                   "AllocFreeTest",
                   t);
    }
    virtual void TearDown () {
        NNTI_result_t nnti_rc = NNTI_OK;
        bool init;

        init = t->initialized();
        EXPECT_TRUE(init);

        if (init) {
            nnti_rc = t->stop();
            EXPECT_EQ(nnti_rc, NNTI_OK);
        }
    }
};

TEST_F(NntiAllocFreeTest, start1) {
    NNTI_buffer_t  dst_buf;
    char          *dst_base=nullptr;

    nnti::datatype::nnti_event_callback null_cb(t, (NNTI_event_callback_t)0);

    NNTI_result_t rc = t->alloc(3200, NNTI_BF_LOCAL_WRITE, (NNTI_event_queue_t)0, null_cb, nullptr, &dst_base, &dst_buf);
    EXPECT_EQ(rc, NNTI_OK);
    rc = t->free(dst_buf);
    EXPECT_EQ(rc, NNTI_OK);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    EXPECT_EQ(1, mpi_size);
    assert(1==mpi_size);

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return (rc);
}
