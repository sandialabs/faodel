// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_many_items
//  Purpose: Given a pair of nodes, inject a large number of keys and then list them



#include <mpi.h>
#include <algorithm>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <opbox/common/MessageHelpers.hh>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "faodel-services/MPISyncStart.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/ops/direct/msg_direct.hh"

#include "whookie/Server.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;
using namespace opbox;



//Parameters used in this experiment
// Note: wante more than 10 for num_rows/cols so we can wildcard on 0* and 1*
struct Params {
  int num_rows;
  int num_cols;
  uint32_t ldo_size;
};
Params P = { 16, 32, 1024 };



string default_config_string = R"EOF(


mpisyncstart.debug true

dirman.type centralized
dirman.root_node_mpi 0
dirman.resources_mpi[] dht:/target 1


)EOF";

class MPIManyItems : public testing::Test {
protected:
  void SetUp() override {
    target  = kelpie::Connect("/target");
    my_id = net::GetMyID();
  }

  void TearDown() override {
  }
  kelpie::Pool target;

  faodel::nodeid_t my_id;
  int rc;
};


TEST_F(MPIManyItems, VerifySaneWhookieIP) {
  //If whookie chose the wrong port, it can get a bad ip address

  EXPECT_TRUE( my_id.Valid() );
  EXPECT_TRUE( my_id.ValidIP() );  //Most likely to break if whookie.interfaces is bad
  EXPECT_TRUE( my_id.ValidPort() );
}


TEST_F(MPIManyItems, MessagePackingCheck) {
  //This test is here just to verify that cereal serial packs everything correctly


  const int NUM_ROWS=P.num_rows;
  const int NUM_COLS=P.num_cols;
  vector<kelpie::Key> keys;
  kelpie::ObjectCapacities oc;
  size_t spot=0;
  for(int i=0; i<NUM_ROWS; i++) {
    for(int j=0; j<NUM_COLS; j++) {
      kelpie::Key key(StringZeroPad(i,255),StringZeroPad(j,255));
      oc.Append( key, spot);
      keys.push_back(key);
      spot++;
    }
  }

  //Pretend we're sending a message, just so we can build a reply message
  lunasa::DataObject ldo1, ldo2;
  kelpie::msg_direct_buffer_t::Alloc(
          ldo1,
          1, 100, NODE_LOCALHOST,
          opbox::MAILBOX_UNSPECIFIED, opbox::MAILBOX_UNSPECIFIED,
          bucket_t("bosstone"), kelpie::Key("frank"),
          2, 3,
          nullptr);
  auto *imsg = ldo1.GetDataPtr<kelpie::msg_direct_buffer_t*>();

  //Fake a reply message
  AllocateCerealReplyMessage<kelpie::ObjectCapacities>(ldo2, &imsg->hdr, 0, oc);

  //Pretend like we got the reply
  auto *omsg = ldo2.GetDataPtr<kelpie::msg_direct_buffer_t*>();
  auto found_oc = opbox::UnpackCerealMessage<kelpie::ObjectCapacities>(&omsg->hdr);

  EXPECT_EQ(oc.Size(), found_oc.Size());
  if(oc.Size() == found_oc.Size()) {
    spot=0;
    for(int i=0; i<NUM_ROWS; i++)
      for(int j=0; j<NUM_COLS; j++) {
        EXPECT_EQ(oc.keys.at(spot), found_oc.keys.at(spot));
        EXPECT_EQ(oc.capacities.at(spot), spot);
        spot++;
      }

  }
  EXPECT_EQ(NUM_ROWS*NUM_COLS, spot);



}

TEST_F(MPIManyItems, BlastAndCheck) {

  rc_t rc;

  //Use the same ldo for every object
  lunasa::DataObject ldo(1024);
  auto *x = ldo.GetDataPtr<int *>();
  for(int i=0; i<1024/sizeof(int); i++)
    x[i]=i;

  const int NUM_ROWS=P.num_rows;
  const int NUM_COLS=P.num_cols;

  //Generate all the row/col names (just zero padded numbers)
  vector<string> expected_cols;
  for(int i=0; i<std::max(NUM_ROWS, NUM_COLS); i++) {
    expected_cols.push_back(  StringZeroPad(i,255) );
  }

  for(int i=0; i<NUM_ROWS; i++) {
    kelpie::ResultCollector results(NUM_COLS);
    for(int j = 0; j < NUM_COLS; j++) {
      kelpie::Key key(expected_cols.at(i), expected_cols.at(j));
      target.Publish(key, ldo, results);
    }
    results.Sync();

    //Make a query
    kelpie::ObjectCapacities oc;
    rc = target.List(kelpie::Key(expected_cols.at(i), "*"), &oc);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(NUM_COLS, oc.keys.size());
    cout<<"Finished Row "<<i<<". Expected "<<NUM_COLS<<" items, list found "<<oc.keys.size()<<endl;
    std::sort(oc.keys.begin(), oc.keys.end());
    for(int i=0; i< oc.keys.size(); i++){
      //cout <<oc.keys.at(i).str() <<endl;
      EXPECT_EQ(expected_cols.at(i), oc.keys.at(i).K2());
    }
  }


}




void targetLoop() {
  //G.dump();
}

int main(int argc, char **argv){
  int rc=0;
  int mpi_rank, mpi_size;

  ::testing::InitGoogleTest(&argc, argv);
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  if(mpi_size!=2) {
    if(mpi_rank==0)
      cerr<<"Error: This test expects exactly two ranks"<<endl;
    MPI_Finalize();
    return -1;
  }

  int c;
  while((c=getopt(argc,argv,"r:c:d:")) != -1){
    switch(c){
      case 'r': P.num_rows = atoi(optarg); break;
      case 'c': P.num_cols = atoi(optarg); break;
      case 'd': faodel::StringToUInt32(&P.ldo_size, string(optarg)); break;
      default:
        cout << "Unknown option. Params are:\n"
             << " -r num_rows        : how many rows of data \n"
             << " -c num_cols        : How many columns of data to send before syncing\n"
             << " -d datasize(k,m,g) : Size of each data object\n";
        MPI_Finalize();
        exit(-1);
    }
  }


  //Insert any Op registrations here
  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();



  mpisyncstart::bootstrap();
  bootstrap::Start(config, kelpie::bootstrap);


  if(mpi_rank!=0) {
    targetLoop();
    sleep(1);
  } else {
    rc = RUN_ALL_TESTS();
    sleep(1);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  bootstrap::Finish();

  MPI_Finalize();

  return rc;
}
