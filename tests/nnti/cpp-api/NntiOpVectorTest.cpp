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

#include <iostream>
#include <thread>
#include <vector>

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "test_utils.hpp"

using namespace std;
using namespace faodel;

string default_config_string = R"EOF(
# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class test_op
: public nnti::core::nnti_op {
private:
    test_op(void);

public:
    test_op(
        nnti::transports::transport  *transport,
        nnti::datatype::nnti_work_id *wid)
    : nnti_op(
        wid)
    {
        return;
    }
};


class NntiOpVectorTest : public testing::Test {
protected:
    Configuration config;

    nnti::transports::transport *t=nullptr;

    virtual void SetUp () {
        config = Configuration(default_config_string);
        config.AppendFromReferences();

        test_setup(0,
                   NULL,
                   config,
                   "OpVectorTest",
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

const int num_op=1024;

TEST_F(NntiOpVectorTest, start1) {
    nnti::datatype::nnti_work_id wid(t);
    std::vector<test_op*>        mirror;

    nnti::core::nnti_op_vector op_vector(1024);

    mirror.reserve(1024);

    for (auto i = 0; i < num_op; ++i) {
        test_op *op = new test_op(t, &wid);
        op_vector.add(op);
        mirror[i] = op;

        if (i % 10 == 0) {
            uint32_t victim_index = i/2;
            nnti::core::nnti_op *victim_op = op_vector.remove(victim_index);
            EXPECT_EQ(victim_op, mirror[victim_index]);

            op = new test_op(t, &wid);
            op_vector.add(op);
            mirror[victim_index] = op;
            EXPECT_EQ(op_vector.at(victim_index), mirror[victim_index]);

            delete victim_op;
        }
    }
    for (auto i = 0; i < mirror.size(); ++i) {
        EXPECT_EQ(op_vector.at(i), mirror[i]);
        op_vector.remove(i);
        delete mirror[i];
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

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return (rc);
}
