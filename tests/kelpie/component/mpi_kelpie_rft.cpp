// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_rft
//  Purpose: This is set of simple tests to see if we can setup/use a rank-folding table



#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "whookie/Server.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;

//Parameters used in this experiment
struct Params {
  int num_rows;
  int num_cols;
  int ldo_size;
};
Params P = { 2, 10, 20*1024 };



string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server
# default to using mpi, but allow override in config file pointed to by FAODEL_CONFIG

dirman.root_role rooter
dirman.type centralized

target.dirman.host_root



# MPI tests will need to have a standard networking base
#kelpie.type standard

#bootstrap.debug true
#whookie.debug true
#opbox.debug true
#dirman.debug true
#kelpie.debug true

)EOF";


class MPIRFTTest : public testing::Test {
protected:
  void SetUp() override {
    local = kelpie::Connect("local:");
    rft_full  = kelpie::Connect("ref:/rft_full");
    rft_full0 = kelpie::Connect("ref:/rft_full&rank=0"); //Us
    rft_full1 = kelpie::Connect("ref:/rft_full&rank=1"); //Someone else
    rft_back  = kelpie::Connect("ref:/rft_back");
    rft_back0 = kelpie::Connect("ref:/rft_back&rank=0"); //For us
    rft_back1 = kelpie::Connect("ref:/rft_back&rank=1"); //Someone else
    
    my_id = net::GetMyID();
  }

  void TearDown() override {}
  kelpie::Pool rft_full;
  kelpie::Pool rft_full0;
  kelpie::Pool rft_full1;
  kelpie::Pool rft_back;
  kelpie::Pool rft_back0;
  kelpie::Pool rft_back1;
  kelpie::Pool local;

  faodel::nodeid_t my_id;
  int rc;
};

lunasa::DataObject generateLDO(int num_words, uint32_t start_val){
  lunasa::DataObject ldo(0, num_words*sizeof(int), lunasa::DataObject::AllocatorType::eager);
  int *x = static_cast<int *>(ldo.GetDataPtr());
  for(int i=0; i <num_words; i++)
    x[i]=start_val+i;
  return ldo;
}


// Sanity check: This test just checks to make sure the rfts are setup correctly
TEST_F(MPIRFTTest, CheckRFTs) {

  auto di_full  = rft_full.GetDirectoryInfo();
  auto di_full0 = rft_full0.GetDirectoryInfo();
  auto di_full1 = rft_full1.GetDirectoryInfo();

  auto di_back  = rft_back.GetDirectoryInfo();
  auto di_back0 = rft_back0.GetDirectoryInfo();
  auto di_back1 = rft_back1.GetDirectoryInfo();

  //Full size
  EXPECT_EQ(G.mpi_size,   di_full.members.size());
  EXPECT_EQ(G.mpi_size,   di_full0.members.size());
  EXPECT_EQ(G.mpi_size,   di_full1.members.size());


  //Back size: everyone but us
  int num_back = G.mpi_size-1; //not us
  EXPECT_EQ(num_back, di_back.members.size());
  EXPECT_EQ(num_back, di_back0.members.size());
  EXPECT_EQ(num_back, di_back1.members.size());



  kelpie::Key key("foo"); 
  faodel::nodeid_t node_id;
  int count;

  count = rft_full.FindTargetNode(key, &node_id);  EXPECT_EQ(1, count); EXPECT_EQ(my_id, node_id);
  count = rft_full0.FindTargetNode(key, &node_id); EXPECT_EQ(1, count); EXPECT_EQ(my_id, node_id);
  count = rft_full1.FindTargetNode(key, &node_id); EXPECT_EQ(1, count); EXPECT_EQ(di_full.members[1].node, node_id);

  count = rft_back.FindTargetNode(key, &node_id);  EXPECT_EQ(1, count); EXPECT_EQ(di_back.members[0].node, node_id);
  count = rft_back0.FindTargetNode(key, &node_id); EXPECT_EQ(1, count); EXPECT_EQ(di_back.members[0].node, node_id);
  count = rft_back1.FindTargetNode(key, &node_id); EXPECT_EQ(1, count); EXPECT_EQ(di_back.members[1].node, node_id);

}

TEST_F(MPIRFTTest, BasicPubLocal) {

  lunasa::DataObject ldo1(64); //dummy
  
  kelpie::object_info_t info;

  kelpie::Key key1("single_for_full_r0");
  kelpie::Key key2("single_for_full_r0", "part2");

  //Publish to full, which sould land here. Result should show in local memory
  rc = rft_full.Publish(key1, ldo1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(1,  info.row_num_columns);
  EXPECT_EQ(64, info.row_user_bytes);
  EXPECT_EQ(64, info.col_user_bytes);
  EXPECT_EQ(Availability::InLocalMemory, info.col_availability);

  rc = rft_full.Publish(key2, ldo1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(2,   info.row_num_columns);
  EXPECT_EQ(128, info.row_user_bytes);
  EXPECT_EQ(64,  info.col_user_bytes);
  EXPECT_EQ(Availability::InLocalMemory, info.col_availability);


  //See if local
  info.col_user_bytes=-1;
  rc = local.Info(key1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(64,  info.col_user_bytes);
  EXPECT_EQ(Availability::InLocalMemory, info.col_availability);

  info.col_user_bytes=-1;
  rc = local.Info(key2, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(64,  info.col_user_bytes);
  EXPECT_EQ(Availability::InLocalMemory, info.col_availability);


}

TEST_F(MPIRFTTest, BasicPubRemote) {

  lunasa::DataObject ldo1(64); //dummy
  int *x = ldo1.GetDataPtr<int *>();
  for(int i=0; i<64/sizeof(int); i++)
    x[i]=i;

  kelpie::object_info_t info;

  kelpie::Key key1("single_for_back_r0");
  kelpie::Key key2("single_for_back_r0", "part2");

  rc = rft_back.Publish(key1, ldo1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(1,  info.row_num_columns);
  EXPECT_EQ(64, info.row_user_bytes);
  EXPECT_EQ(64, info.col_user_bytes);
  EXPECT_EQ(Availability::InRemoteMemory, info.col_availability);

  rc = rft_back.Publish(key2, ldo1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(2,   info.row_num_columns);
  EXPECT_EQ(128, info.row_user_bytes);
  EXPECT_EQ(64,  info.col_user_bytes);
  EXPECT_EQ(Availability::InRemoteMemory, info.col_availability);


  //Verify these are NOT local
  rc = local.Info(key1, &info);  EXPECT_EQ(KELPIE_ENOENT, rc);
  rc = local.Info(key2, &info);  EXPECT_EQ(KELPIE_ENOENT, rc);


  //See if its on rank0
  rc = rft_back0.Info(key1, &info);
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(64,  info.col_user_bytes);
  EXPECT_EQ(Availability::InRemoteMemory, info.col_availability);

  //See if its on rank1
  rc = rft_back1.Info(key1, &info);
  EXPECT_EQ(KELPIE_ENOENT, rc);

  lunasa::DataObject ldo2;
  rc = rft_back0.Need(key1, &ldo2);
  EXPECT_EQ(rc, KELPIE_OK);
  EXPECT_EQ(0, ldo1.DeepCompare(ldo2));

}

//
// TODO: Add tests for actual folding
// TODO: Add tests for other DHT ops


void targetLoop(){
  //G.dump();
}

int main(int argc, char **argv){

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  //Insert any Op registrations here

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config, 3);


  if(G.mpi_rank==0){
    //Register the rft
    DirectoryInfo di_full("rft:/rft_full",    "This RFT includes all the ranks");
    DirectoryInfo di_back("rft:/rft_back",    "This RFT includes all ranks except rank 0");
    
    for(int i=0; i<G.mpi_size; i++){
      di_full.Join(G.nodes[i]);
      if(i>0)  di_back.Join(G.nodes[i]);
    }
    dirman::HostNewDir(di_full);
    dirman::HostNewDir(di_back);


    
    faodel::nodeid_t n2 = net::GetMyID();

    rc = RUN_ALL_TESTS();
    sleep(1);
  } else {
    targetLoop();
    sleep(1);
  }
  G.StopAll();

  return rc;
}
