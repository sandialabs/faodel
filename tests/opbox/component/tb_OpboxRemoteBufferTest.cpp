// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config_string = R"EOF(
nnti.transport.name                           mpi

# This test checks an absolute offset, which only works w/ the malloc allocator
lunasa.lazy_memory_manager    malloc
lunasa.eager_memory_manager   malloc

config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";


class OpboxRemoteBufferTest : public testing::Test {
protected:
    Configuration config;
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

TEST_F(OpboxRemoteBufferTest, start1) {
    DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;
    uint32_t offset=0;
    uint32_t length=0;
    ldo.GetHeaderRdmaHandle((void**)&nbl, offset);
    EXPECT_NE(nbl, nullptr);
    nbl->makeRemoteBuffer(offset, length, &nbr);
}

TEST_F(OpboxRemoteBufferTest, start2) {
    DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;
    uint32_t length=0;
    opbox::net::GetRdmaPtr(&ldo, length, &nbl, &nbr);
    EXPECT_NE(nbl, nullptr);
}

TEST_F(OpboxRemoteBufferTest, start3) {
    DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;
    uint32_t offset=ldo.GetHeaderSize();
    uint32_t length=ldo.GetDataSize();
    opbox::net::GetRdmaPtr(&ldo, offset, length, &nbl, &nbr);
    EXPECT_NE(nbl, nullptr);
}

TEST_F(OpboxRemoteBufferTest, start4) {
    DataObject ldo(128, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;

    uint32_t offset=0;
    uint32_t length=ldo.GetHeaderSize()+ldo.GetMetaSize()+ldo.GetDataSize();
    opbox::net::GetRdmaPtr(&ldo, offset, length, &nbl, &nbr);
    EXPECT_NE(nbl, nullptr);
    EXPECT_EQ(nbr.GetLength(), length);

    nbr.IncreaseOffset(ldo.GetHeaderSize());
    EXPECT_EQ(nbr.GetLength(), ldo.GetMetaSize() + ldo.GetDataSize());

    nbr.DecreaseLength(ldo.GetMetaSize());
    EXPECT_EQ(nbr.GetLength(), ldo.GetDataSize());

    nbr.TrimToLength(2560);
    EXPECT_EQ(nbr.GetLength(), 2560);
}

TEST_F(OpboxRemoteBufferTest, start5) {
    DataObject ldo(128, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;

    opbox::net::GetRdmaPtr(&ldo, &nbl, &nbr);
    EXPECT_NE(nbl, nullptr);
    EXPECT_EQ(nbr.GetLength(), ldo.GetHeaderSize()+ldo.GetMetaSize()+ldo.GetDataSize());

    nbr.IncreaseOffset(ldo.GetHeaderSize());
    EXPECT_EQ(nbr.GetLength(), ldo.GetMetaSize() + ldo.GetDataSize());

    nbr.IncreaseOffset(ldo.GetMetaSize());
    EXPECT_EQ(nbr.GetLength(), ldo.GetDataSize());

    nbr.TrimToLength(2560);
    EXPECT_EQ(nbr.GetLength(), 2560);
}

TEST_F(OpboxRemoteBufferTest, start6) {
    DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

    opbox::net::NetBufferLocal  *nbl=nullptr;
    opbox::net::NetBufferRemote  nbr;
    uint32_t offset=ldo.GetHeaderSize();
    uint32_t length=ldo.GetDataSize();
    opbox::net::GetRdmaPtr(&ldo, &nbl, &nbr);
    EXPECT_NE(nbl, nullptr);
    EXPECT_EQ(nbr.GetOffset(), ldo.GetLocalHeaderSize());
    EXPECT_EQ(nbr.GetLength(), ldo.GetHeaderSize()+ldo.GetMetaSize()+ldo.GetDataSize());
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
