// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

//Special config: This test has to use the malloc allocator to get the
//                offsets right in the tests.
string remote_buffer_config_string = R"EOF(
# This test checks an absolute offset, which only works w/ the malloc allocator
lunasa.lazy_memory_manager    malloc
lunasa.eager_memory_manager   malloc

)EOF";


class OpboxRemoteBufferTest : public testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(OpboxRemoteBufferTest, start1) {
  DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;
  uint32_t offset = 0;
  uint32_t length = 0;
  ldo.GetHeaderRdmaHandle((void **) &nbl, offset);
  EXPECT_NE(nbl, nullptr);
  nbl->makeRemoteBuffer(offset, length, &nbr);
}

TEST_F(OpboxRemoteBufferTest, start2) {
  DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;
  uint32_t length = 0;
  opbox::net::GetRdmaPtr(&ldo, length, &nbl, &nbr);
  EXPECT_NE(nbl, nullptr);
}

TEST_F(OpboxRemoteBufferTest, start3) {
  DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;
  uint32_t offset = ldo.GetHeaderSize();
  uint32_t length = ldo.GetDataSize();
  opbox::net::GetRdmaPtr(&ldo, offset, length, &nbl, &nbr);
  EXPECT_NE(nbl, nullptr);
}

TEST_F(OpboxRemoteBufferTest, start4) {
  DataObject ldo(128, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;

  uint32_t offset = 0;
  uint32_t length = ldo.GetHeaderSize() + ldo.GetMetaSize() + ldo.GetDataSize();
  opbox::net::GetRdmaPtr(&ldo, offset, length, &nbl, &nbr);
  EXPECT_NE(nbl, nullptr);
  EXPECT_EQ(nbr.GetLength(), length);

  nbr.IncreaseOffset(ldo.GetHeaderSize());
  EXPECT_EQ(nbr.GetLength(), ldo.GetMetaSize() + ldo.GetDataSize());

  nbr.DecreaseLength(ldo.GetMetaSize());
  EXPECT_EQ(nbr.GetLength(), ldo.GetDataSize());

  nbr.TrimToLength(2560);
  EXPECT_EQ(nbr.GetLength(), 2560UL);
}

TEST_F(OpboxRemoteBufferTest, start5) {
  DataObject ldo(128, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;

  opbox::net::GetRdmaPtr(&ldo, &nbl, &nbr);
  EXPECT_NE(nbl, nullptr);
  EXPECT_EQ(nbr.GetLength(), ldo.GetHeaderSize() + ldo.GetMetaSize() + ldo.GetDataSize() + ldo.GetPaddingSize());

  nbr.IncreaseOffset(ldo.GetHeaderSize());
  EXPECT_EQ(nbr.GetLength(), ldo.GetMetaSize() + ldo.GetDataSize() + ldo.GetPaddingSize());

  nbr.IncreaseOffset(ldo.GetMetaSize());
  EXPECT_EQ(nbr.GetLength(), ldo.GetDataSize() + ldo.GetPaddingSize());

  nbr.TrimToLength(2560);
  EXPECT_EQ(nbr.GetLength(), 2560UL);
}

TEST_F(OpboxRemoteBufferTest, start6) {
  DataObject ldo(0, 5120, DataObject::AllocatorType::eager);

  opbox::net::NetBufferLocal *nbl = nullptr;
  opbox::net::NetBufferRemote nbr;
  opbox::net::GetRdmaPtr(&ldo, &nbl, &nbr);
  EXPECT_NE(nbl, nullptr);
  EXPECT_EQ(nbr.GetOffset(), ldo.GetLocalHeaderSize());
  EXPECT_EQ(nbr.GetLength(), ldo.GetHeaderSize() + ldo.GetMetaSize() + ldo.GetDataSize() + ldo.GetPaddingSize());
}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  //Note: This test uses a special config that sets the mem allocators to malloc!
  bootstrap::Start(Configuration(remote_buffer_config_string), opbox::bootstrap);

  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
