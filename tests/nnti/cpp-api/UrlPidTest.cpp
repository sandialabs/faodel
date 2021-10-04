// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"

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
)EOF";


class UrlPidTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

  void SetUp () override {
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        test_setup(0,
                   NULL,
                   config,
                   "UrlPidTest",
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

TEST_F(UrlPidTest, start1) {
    NNTI_result_t rc = NNTI_OK;
    char url1[NNTI_URL_LEN];
    char url2[NNTI_URL_LEN];
    NNTI_process_id_t pid1;
    NNTI_process_id_t pid2;

    rc = t->pid(&pid1);
    EXPECT_EQ(rc, NNTI_OK);

    rc = t->dt_pid_to_url(pid1, url1, NNTI_URL_LEN);
    EXPECT_EQ(rc, NNTI_OK);

    rc = t->dt_url_to_pid(url1, &pid2);
    EXPECT_EQ(rc, NNTI_OK);

    rc = t->dt_pid_to_url(pid2, url2, NNTI_URL_LEN);
    EXPECT_EQ(rc, NNTI_OK);

    EXPECT_EQ(pid1, pid2);
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
    bootstrap::Finish();

    MPI_Finalize();

    return (rc);
}
