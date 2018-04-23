// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

# default to using mpi, but allow override in config file pointed to by CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

#
security_bucket                       bobbucket

# Tester: Run a dedicated tester that has a resource manager tester named /
tester.rpc_tester_type                single
#tester.net.url.write_to_file          .tester-url
tester.resource_manager.type          tester
tester.resource_manager.path          /bob
tester.resource_manager.write_to_file .tester-url

# Client: Don't use a tester, just send requests
client.rpc_tester_type                 none
client.resource_manager.path           /bob/1
client.resource_manager.read_from_file .tester-url
)EOF";


class OpboxInitTest : public testing::Test {
protected:
    int mpi_rank, mpi_size;
    int root_rank;

    virtual void SetUp () {
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
        root_rank = 0;
    }
    virtual void TearDown () {
    }
};

TEST_F(OpboxInitTest, start1) {
    opbox::net::Attrs attrs;
    opbox::net::GetAttrs(attrs);
    DataObject ldo = opbox::net::NewMessage(attrs.max_eager_size);
//    EXPECT_NE(ldo, nullptr);
    opbox::net::ReleaseMessage(ldo);
}

int main(int argc, char **argv){

    ::testing::InitGoogleTest(&argc, argv);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    int mpi_rank,mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    Configuration conf(default_config_string);
    conf.AppendFromReferences();
    if(argc>1){
        if(string(argv[1])=="-v"){         conf.Append("loglevel all");
        } else if(string(argv[1])=="-V"){  conf.Append("loglevel all\nnssi_rpc.loglevel all");
        }
    }
    conf.Append("node_role", (mpi_rank==0) ? "tester" : "target");
    bootstrap::Start(conf, opbox::bootstrap);

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    MPI_Barrier(MPI_COMM_WORLD);
    bootstrap::Finish();

    MPI_Finalize();
    return rc;
}
