// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"

#include "nnti/nntiConfig.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_util.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/transport_factory.hpp"

#include "test_utils.hpp"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
)EOF";


class NntiSelfConnectTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

  void SetUp () override {
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        test_setup(0,
                   NULL,
                   config,
                   "SelfConnectTest",
                   t);
    }

  void TearDown () override {
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

TEST_F(NntiSelfConnectTest, ConnectLoop) {
    NNTI_result_t nnti_rc = NNTI_OK;
    NNTI_peer_t peer_hdl;

    char my_url[NNTI_URL_LEN];
    t->get_url(my_url, NNTI_URL_LEN);

    for (int i=0;i<10;i++) {
        nnti_rc = t->connect(&my_url[0], 1000, &peer_hdl);
        EXPECT_EQ(nnti_rc, NNTI_OK);
        nnti_rc = t->disconnect(peer_hdl);
        EXPECT_EQ(nnti_rc, NNTI_OK);
    }
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

    bootstrap::Finish();

    MPI_Finalize();

    return (rc);
}
