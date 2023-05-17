// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_TFT
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


class MPITFTTest : public testing::Test {
protected:
  void SetUp() override {
    local = kelpie::Connect("local:");
    TFT_full  = kelpie::Connect("ref:/TFT_full");
    TFT_back  = kelpie::Connect("ref:/TFT_back");


     //Connect to each rank individually
    for(int i=0; i<G.mpi_size; i++) {
       auto p = kelpie::Connect("ref:/RFT_full&rank="+std::to_string(i));
       if(!p.Valid()) {
          FAIL();
       }
       individual_rank.push_back(p);
    }

    my_id = net::GetMyID();
  }

  void TearDown() override {}

  kelpie::Pool local;
  kelpie::Pool TFT_full; //A pool with all the ranks in it
  kelpie::Pool TFT_back; //A pool with all the ranks in it except first

   std::vector<kelpie::Pool> individual_rank; //Individual pools, each pointing to a single rank

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


// Sanity check: This test just checks to make sure the TFTs are setup correctly
TEST_F(MPITFTTest, CheckTFTs) {

  auto di_full  = TFT_full.GetDirectoryInfo();
  EXPECT_EQ(G.mpi_size,   di_full.members.size());

  //Create 2*N keys with tags. Check that the TFT properly tells us to pick the node
  for(int i=0; i<2*G.mpi_size; i++) {
     //Make a key for each node
     kelpie::Key key("thing");
     key.SetK1Tag(i);

     faodel::nodeid_t node_id;
     int count = TFT_full.FindTargetNode(key, &node_id);
     int spot = i%G.mpi_size;
     EXPECT_EQ(1, count);
     EXPECT_EQ(di_full.members[spot].node, node_id);
  }

}

//Publish a small set to 4 targets
TEST_F(MPITFTTest, BasicPubRemote) {

  auto ldo = generateLDO(100, 9876); //Generate arbitrary object. Makes 4x100 bytes of user data
  kelpie::object_info_t info;

  kelpie::Key key0("single_for_r0"); key0.SetK1Tag(0);
  kelpie::Key key1("single_for_r1"); key1.SetK1Tag(1);
  kelpie::Key key2("single_for_r2"); key2.SetK1Tag(2);
  kelpie::Key key3("single_for_r3"); key3.SetK1Tag(3);
  vector<Key> keys = {key0, key1, key2, key3};

  //Publish all keys and check info
  for(int i =0; i<4; i++){
     info.Wipe();
     rc = TFT_full.Publish(keys[i], ldo, &info);
     EXPECT_EQ(KELPIE_OK, rc);
     EXPECT_EQ(1,  info.row_num_columns);
     EXPECT_EQ(400, info.row_user_bytes);
     EXPECT_EQ(400, info.col_user_bytes);
     if(i==0) {
        EXPECT_EQ(Availability::InLocalMemory, info.col_availability);
     } else {
        EXPECT_EQ(Availability::InRemoteMemory, info.col_availability);
     }
  }

  //Verify that if we look locally, we only see the first key
  for(int i=0; i<4; i++){
     info.Wipe();
     rc = local.Info(keys[i], &info);
     if(i==0){
        EXPECT_EQ(KELPIE_OK, rc);
     } else {
        EXPECT_EQ(KELPIE_ENOENT, rc);
     }
  }

   //Verify we can see each key through the tft
   for(int i=0; i<4; i++) {
      info.Wipe();
      rc = TFT_full.Info(keys[i], &info);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(400, info.col_user_bytes);
      auto local_or_remote = ((i == 0) ? Availability::InLocalMemory : Availability::InRemoteMemory);
      EXPECT_EQ(local_or_remote, info.col_availability);
   }

   //Verify we can grab each item over the TFT
   for(int i=0; i<4; i++){
      lunasa::DataObject ldo2;
      rc = TFT_full.Need(keys[i], &ldo2);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(0, ldo.DeepCompare(ldo2));
   }

   //Verify we can grab each item over the TFT
   for(int i=0; i<4; i++) {
      info.Wipe();
      int spot = i%G.mpi_size;
      rc = individual_rank[spot].Info(keys[i], &info);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(1, info.row_num_columns);
      if(rc!=KELPIE_OK) continue;


      //cout<<"Fetching "<<spot<<" from "<< individual_rank[spot].str()<<endl;
      lunasa::DataObject ldo2;
      rc = individual_rank[i%G.mpi_size].Need(keys[i], &ldo2);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(0, ldo.DeepCompare(ldo2));
   }

}

//Publish a small set to 4 targets
TEST_F(MPITFTTest, PubAllRemote) {

   auto ldo = generateLDO(100, 9876); //Generate arbitrary object. Makes 4x100 bytes of user data
   kelpie::object_info_t info;

   int pool_size = G.mpi_size - 1;

   vector<pair<Key,int>> keys_ranks;
   for(int i=0; i<100; i++) {
      kelpie::Key key("BackTest");
      key.SetK1Tag(i);
      int rank = (i%pool_size) + 1;  //Rank0 is this rank. Pool is 1-(numnodes-1).
      keys_ranks.push_back( pair<Key,int>(key, rank));
      //cout <<"Key "<<key.str()<<" rank "<<rank<<endl;
   }

   //Publish all keys, verifying return info
   for(int i =0; i<keys_ranks.size(); i++){
      info.Wipe();
      rc = TFT_back.Publish(keys_ranks[i].first, ldo, &info);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(1,  info.row_num_columns);
      EXPECT_EQ(400, info.row_user_bytes);
      EXPECT_EQ(400, info.col_user_bytes);
      EXPECT_EQ(Availability::InRemoteMemory, info.col_availability);
   }

   //See if each object lives in the expected server
   for(auto key_rank : keys_ranks) {
      //Verify not available locally
      info.Wipe();
      rc = local.Info(key_rank.first, &info);
      EXPECT_EQ(KELPIE_ENOENT, rc);
      if(rc!=KELPIE_ENOENT) continue;

      //Pull from tft
      lunasa::DataObject ldo2;
      rc = TFT_back.Need(key_rank.first, &ldo2);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(0, ldo.DeepCompare(ldo2));
      if(rc!=KELPIE_OK) continue;

      //See if the object lives in the expected rank
      info.Wipe();
      rc = individual_rank[key_rank.second].Info(key_rank.first, &info);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(1, info.row_num_columns);
      if(rc!=KELPIE_OK) continue;

      lunasa::DataObject ldo3;
      //cout<<"Fetching "<<key_rank.second<<" from "<< individual_rank[key_rank.second].str()<<endl;
      rc = individual_rank[key_rank.second].Need(key_rank.first, &ldo3);
      EXPECT_EQ(KELPIE_OK, rc);
      EXPECT_EQ(0, ldo.DeepCompare(ldo3));
   }

}



void targetLoop(){
  //G.dump();
}

int main(int argc, char **argv){

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  //Insert any Op registrations here

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config, 4);


  if(G.mpi_rank==0){
    //Register the TFT
    DirectoryInfo di_full("TFT:/TFT_full",    "This TFT includes all the ranks");
    DirectoryInfo di_back("TFT:/TFT_back",    "This TFT includes all ranks except rank 0");
    DirectoryInfo di_rft_full("RFT:/RFT_full", "This is a rank-folding table to access individual nodes");
    
    for(int i=0; i<G.mpi_size; i++){
      di_full.Join(G.nodes[i]);
      di_rft_full.Join(G.nodes[i]);
      if(i>0)  di_back.Join(G.nodes[i]);
    }
    dirman::HostNewDir(di_full);
    dirman::HostNewDir(di_back);
    dirman::HostNewDir(di_rft_full);


    
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
