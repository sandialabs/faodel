// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// Test:    mpi_opbox_net_init
// Purpose: Basic checks on opbox net (mtus are ok, nbr will serialize)
// Note:    needs mpi for node id to be initialized

#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-common/SerializationHelpersBoost.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

#include "support/default_config_string.hh"


class OpboxInitTest : public testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

//Just make sure we can start/stop ok. If not, init problems
TEST_F(OpboxInitTest, StartPlain) {
  sleep(1);
}

TEST_F(OpboxInitTest, Constants) {
  //Make sure numbers are within reason
  EXPECT_GT(MAX_NET_BUFFER_REMOTE_SIZE, 0);
  EXPECT_LT(MAX_NET_BUFFER_REMOTE_SIZE, 100);

  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);
  EXPECT_GT(attrs.max_eager_size, 512UL); //Verify net can get message-sized data
  EXPECT_GT(attrs.mtu, 512UL); //Verify net has usable packet sizes
}


TEST_F(OpboxInitTest, NewMessage) {
  opbox::net::Attrs attrs;
  opbox::net::GetAttrs(&attrs);
  DataObject ldo = opbox::net::NewMessage(attrs.max_eager_size);

  //The data portion of the message should be the size we asked for
  EXPECT_EQ(attrs.max_eager_size, ldo.GetDataSize());
//    EXPECT_NE(ldo, nullptr);
  opbox::net::ReleaseMessage(ldo);
}



// Make sure net buffer remote structures can be serialized correctly
TEST_F(OpboxInitTest, NBRSimpleSerialize) {

  //NetBufferRemote:
  opbox::net::NetBufferRemote nbr1;

  for(int i = 0; i<MAX_NET_BUFFER_REMOTE_SIZE; i++)
    nbr1.data[i] = i;

  string s = faodel::BoostPack(nbr1);
  //cout <<"NetBufferRemote size: "<<s.size()<<endl;

  opbox::net::NetBufferRemote nbr2 = faodel::BoostUnpack<opbox::net::NetBufferRemote>(s);

  for(uint8_t i = 0; i<MAX_NET_BUFFER_REMOTE_SIZE; i++) {
    EXPECT_EQ(i, nbr2.data[i]);
  }

}


int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  Configuration config(multitest_config_string);
  config.AppendFromReferences();
  if(argc>1) {
    if(string(argv[1]) == "-v") {
      config.Append("loglevel all");
    } else if(string(argv[1]) == "-V") {
      config.Append("loglevel all\nnssi_rpc.loglevel all");
    }
  }
  bootstrap::Start(config, opbox::bootstrap);


  int rc = RUN_ALL_TESTS();
  cout << "Tester completed all tests.\n";

  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();
  return rc;
}
