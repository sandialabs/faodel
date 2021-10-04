// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

//
//  Test: mpi_kelpie_dht
//  Purpose: This is set of simple tests to see if we can setup/use a dht



#include <mpi.h>
#include <algorithm>
#include <random>

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
using namespace opbox;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;

//Parameters used in this experiment
// Note: wante more than 10 for num_rows/cols so we can wildcard on 0* and 1*
struct Params {
  int num_rows;
  int num_cols;
  int ldo_size;
};
Params P = { 16, 16, 20*1024 };



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

void fn_dumpDummy(const lunasa::DataObject &ldo, faodel::ReplyStream &rs) {

}

void fn_dumpMyFloat(const lunasa::DataObject &ldo, faodel::ReplyStream &rs) {

  auto *x = ldo.GetDataPtr<int *>();
  int len = ldo.GetDataSize() / sizeof(int);

  rs.mkSection("Test Data Dump");

  rs.tableBegin("Stats", 2);
  rs.tableTop({"Parameter", "Value"});
  rs.tableRow({"Number Bytes", to_string(ldo.GetDataSize())});
  rs.tableRow({"Number of Ints:", to_string(len)});
  rs.tableEnd();

  rs.tableBegin("Data Values", 2);
  rs.tableTop({"ID", "val[ID]", "val[ID+1]", "val[ID+2]", "val[ID+3]"});
  for(int i = 0; i < len; i += 4) {
    vector<string> line;
    line.push_back(to_string(i));
    for(int j = 0; j < 4; j++)
      line.push_back((i + j < len) ? to_string(x[i + j]) : "");
    rs.tableRow(line);
  }
  rs.tableEnd();

}

class MPIDHTTest : public testing::Test {
protected:
  void SetUp() override {
    dht_full  = kelpie::Connect("dht:/dht_full");
    dht_front = kelpie::Connect("dht:/dht_front_half");
    dht_back  = kelpie::Connect("dht:/dht_back_half");
    dht_single_self  = kelpie::Connect("dht:/dht_single_self");
    dht_single_other = kelpie::Connect("dht:/dht_single_other");
    my_id = net::GetMyID();
    lunasa::RegisterDataObjectType(faodel::const_hash16("TestData"),"TestData", fn_dumpMyFloat);
    lunasa::RegisterDataObjectType(faodel::const_hash16("Dummy"),   "Dummy",    fn_dumpDummy);

  }

  void TearDown() override {
    lunasa::DeregisterDataObjectType(faodel::const_hash16("TestData"));
    lunasa::DeregisterDataObjectType(faodel::const_hash16("Dummy"));
  }
  kelpie::Pool dht_full;
  kelpie::Pool dht_front;
  kelpie::Pool dht_back;
  kelpie::Pool dht_single_self;
  kelpie::Pool dht_single_other;
  faodel::nodeid_t my_id;
  int rc;
};


lunasa::DataObject generateLDO(int num_words, uint32_t start_val){
  lunasa::DataObject ldo(0, num_words*sizeof(int), lunasa::DataObject::AllocatorType::eager,
                         faodel::const_hash16("TestData"));

  int *x = static_cast<int *>(ldo.GetDataPtr());
  for(int i=0; i <num_words; i++)
    x[i]=start_val+i;
  return ldo;
}

vector<pair<kelpie::Key,lunasa::DataObject>>  generateKVs(std::string prefix) {

  static int ldos_generated=0;
  vector<pair<kelpie::Key,lunasa::DataObject>> items;
  for(int i=0; i<P.num_rows; i++) {
    for(int j=0; j<P.num_cols; j++) {

      //Make sure row/cols have some leading zeros so we can wildcard them
      stringstream ss_row, ss_col;
      ss_row<<"row_"<<prefix<<"_"<<std::setw(2) << std::setfill('0') << i;
      ss_col<<"col_"<<prefix<<"_"<<std::setw(2) << std::setfill('0') << j;

      kelpie::Key key(ss_row.str(), ss_col.str());
      auto ldo = generateLDO( P.ldo_size/sizeof(int), (ldos_generated<<16));
      items.push_back( {key,ldo} ); 
      ldos_generated++;
    }
  }
  return items;
}

// Most tests generate and publish objects
vector<pair<kelpie::Key, lunasa::DataObject>> generateAndPublish(kelpie::Pool dht, std::string key_prefix) {

  kelpie::rc_t rc;
  
  //Generate a set of objects 
  auto kvs = generateKVs(key_prefix);

  //Launch publishes, but block until they complete
  atomic<int> num_left(kvs.size());
  for(auto &kv : kvs) {
    rc = dht.Publish(kv.first, kv.second,
                     [&num_left] (kelpie::rc_t result, kelpie::object_info_t &ci) {
                       EXPECT_EQ(0, result);
                       num_left--;
                     } );
    EXPECT_EQ(0, rc);
  }
  while(num_left) { sched_yield(); } //TODO: add a timeout

  return kvs;
}
void checkInfo(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs,
               bool check_availability, kelpie::Availability expected_availability) {

  kelpie::rc_t rc;
  kelpie::object_info_t info;
  for(auto &kv : kvs) {
    rc = dht.Info(kv.first, &info);
    EXPECT_EQ(0, rc);
    //cout <<"Col info "<<col_info.str()<<endl;
    if(check_availability) {
      EXPECT_EQ(expected_availability, info.col_availability);
    }
    EXPECT_EQ(kv.second.GetMetaSize()+kv.second.GetDataSize(), info.col_user_bytes);
  }
}
void checkNeed(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {
  kelpie::rc_t rc;
  
  for(auto &kv : kvs) {
    lunasa::DataObject ldo;
    uint64_t expected_size= kv.second.GetUserSize();
    rc = dht.Need(kv.first, expected_size, &ldo); EXPECT_EQ(0, rc);
    EXPECT_EQ(kv.second.GetDataSize(), ldo.GetDataSize());
    EXPECT_EQ(0, kv.second.DeepCompare(ldo));
  }
}
void checkWantBounded(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {

  kelpie::rc_t rc;
  lunasa::DataObject ldos[kvs.size()];
  
  atomic<int> num_left(kvs.size());
  int spot=0;
  for(auto &kv : kvs) {
    uint64_t expected_size= kv.second.GetMetaSize() + kv.second.GetDataSize();
    rc = dht.Want(kv.first, expected_size,
                  [&num_left, &ldos, spot] (bool success, kelpie::Key key, lunasa::DataObject user_ldo,
                                            const kelpie::object_info_t &info) {
                                   EXPECT_TRUE(success);
                                   EXPECT_EQ(kelpie::Availability::InLocalMemory, info.col_availability);
                                   ldos[spot]=user_ldo;
                                   num_left--;
                               } );
    EXPECT_EQ(0, rc);
    spot++;
  }
  while(num_left!=0) { sched_yield(); } //TODO: add a timeout
  spot=0;
  for(auto &kv : kvs) {
    EXPECT_EQ(0, kv.second.DeepCompare(ldos[spot]));
    spot++;
  }

}
void checkWantUnbounded(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {

  kelpie::rc_t rc;
  lunasa::DataObject ldos[kvs.size()];
  
  atomic<int> num_left(kvs.size());
  int spot=0;
  for(auto &kv : kvs) {
    rc = dht.Want(kv.first, 
                  [&num_left, &ldos, spot] (bool success, kelpie::Key key, lunasa::DataObject user_ldo,
                                            const kelpie::object_info_t &ci) {
                                   EXPECT_TRUE(success);
                                   EXPECT_EQ(kelpie::Availability::InLocalMemory, ci.col_availability);
                                   ldos[spot]=user_ldo;
                                   num_left--;
                                   //cout<<"Unbound "<<key.str()<<" writing to spot "<<spot<<endl;
                               } );
    EXPECT_EQ(0, rc);
    spot++;
  }
  while(num_left!=0) { sched_yield(); } //TODO: add a timeout
  spot=0;
  for(auto &kv : kvs) {
    EXPECT_EQ(0, kv.second.DeepCompare((ldos[spot])));
    spot++;
  }

}

TEST_F(MPIDHTTest, VerifySaneWhookieIP) {
  //If whookie chose the wrong port, it can get a bad ip address

  EXPECT_TRUE( my_id.Valid() );
  EXPECT_TRUE( my_id.ValidIP() );  //Most likely to break if whookie.interfaces is bad
  EXPECT_TRUE( my_id.ValidPort() );
}


// Sanity check: This test just checks to make sure the dhts are setup correctly. If these
// are note right, then you'll get a lot of errors about whether things are local or remote.
// The most common problem is that whookie grabbed the wrong IP card and it has a bad
// IP address (see sanity check above). To fix, set "whookie.interfaces" in FAODEL_CONFIG.
TEST_F(MPIDHTTest, CheckDHTs) {

  auto di  = dht_full.GetDirectoryInfo();
  auto di_front = dht_front.GetDirectoryInfo();
  auto di_back = dht_back.GetDirectoryInfo();
  auto di_self = dht_single_self.GetDirectoryInfo();
  auto di_other = dht_single_other.GetDirectoryInfo();
  
  EXPECT_EQ(G.mpi_size,   di.members.size());
  EXPECT_EQ(G.mpi_size/2, di_front.members.size());
  EXPECT_EQ(G.mpi_size - G.mpi_size/2, di_back.members.size());
  EXPECT_EQ(1, di_self.members.size());
  EXPECT_EQ(1, di_other.members.size());

  if(di_self.members.size()==1) EXPECT_EQ(my_id, di_self.members[0].node);
  if(di_other.members.size()==1) EXPECT_EQ(G.nodes[G.mpi_size-1], di_other.members[0].node);

  //Verify our node is actually in self lists. Assumes we're rank 0 (see main).
  EXPECT_TRUE(  di.ContainsNode(my_id) );
  EXPECT_TRUE(  di_front.ContainsNode(my_id) );
  EXPECT_FALSE( di_back.ContainsNode(my_id) );
  EXPECT_TRUE(  di_self.ContainsNode(my_id) );
  EXPECT_FALSE( di_other.ContainsNode(my_id) );
}


// Self DHT test: Write to a single-node DHT that is local and read results. This
// is useful for verifying that the basic operations work, even if they aren't going
// out to the network.
TEST_F(MPIDHTTest, BasicSingleSelfTest) {

  kelpie::Pool dht = dht_single_self; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "single_self_data");
  checkInfo(dht, kvs, true, kelpie::Availability::InLocalMemory);
  checkNeed(dht, kvs); //Do Need on all ops  
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}


//
TEST_F(MPIDHTTest, BasicSingleOtherNeed) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataA");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops
  
  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicSingleOtherWantBounded) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataB");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length
   
  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicSingleOtherWantUnbounded) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataC");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
   
  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length
}

TEST_F(MPIDHTTest, BasicFullNeed) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicFullWantBounded) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicFullWantUnbounded) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}


TEST_F(MPIDHTTest, BasicHalf1Need) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf1WantBounded) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf1WantUnbounded) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}

TEST_F(MPIDHTTest, BasicHalf2Need) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf2WantBounded) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf2WantUnbounded) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}

TEST_F(MPIDHTTest, ListTestSingle) {

  kelpie::Pool dht = dht_single_self;
  auto kvs = generateAndPublish(dht, "list_test1");
  checkInfo(dht, kvs, false, kelpie::Availability::InLocalMemory);

  kelpie::ObjectCapacities oc;
  rc = dht.List(kelpie::Key("row_list_test1_1","*"), &oc );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);

  //cout <<"Got list with num items="<<oc.keys.size()<<endl;

  //for(int i=0;i<oc.keys.size(); i++) {
  //  cout <<"  " << oc.keys[i].str() << oc.capacities[i]<<endl;
  //}
}

TEST_F(MPIDHTTest, ListTestRowWildcardSingleSelf) {

  kelpie::Pool dht = dht_single_self;
  auto kvs = generateAndPublish(dht, "list_test2");
  checkInfo(dht, kvs, false, kelpie::Availability::InLocalMemory);

  //Query1: See if we get everything with two wildcards: "row_list_test2*, *"
  kelpie::ObjectCapacities oc;
  rc = dht.List(kelpie::Key("row_list_test2*","*"), &oc );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows*P.num_cols, oc.keys.size()); // rows x cols worth of data
  for(int i=0;i<oc.keys.size(); i++) {
    //cout << "  " << oc.keys[i].str() << "  " << oc.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc.capacities[i]);
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K1(), "row_list_test2"));
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K2(), "col_list_test2"));
  }


  //Query2: See if we get everything with col wildcard: "row_list_test2, *"
  kelpie::ObjectCapacities oc2;
  rc = dht.List(kelpie::Key("row_list_test2_01","*"), &oc2 );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_cols, oc2.keys.size());  //Num cols worth of data
  for(int i=0;i<oc2.keys.size(); i++) {
    //cout << "  " << oc2.keys[i].str() << "  " << oc2.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc2.capacities[i]);
    EXPECT_EQ(   oc2.keys[i].K1(), "row_list_test2_01");
    EXPECT_TRUE( StringBeginsWith(oc2.keys[i].K2(), "col_list_test2"));
  }

  //Query3: See if we get a col wildcard: "*, col_list_test2_03
  kelpie::ObjectCapacities oc3;
  rc = dht.List(kelpie::Key("row_list_test2*","col_list_test2_03"), &oc3 );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows, oc3.keys.size());  //Num cols worth of data
  for(int i=0;i<oc3.keys.size(); i++) {
    //cout << "  " << oc3.keys[i].str() << "  " << oc3.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc3.capacities[i]);
    EXPECT_TRUE( StringBeginsWith( oc3.keys[i].K1(), "row_list_test2"));
    EXPECT_EQ(oc3.keys[i].K2(), "col_list_test2_03");
  }

}


TEST_F(MPIDHTTest, ListTestRowWildcardFull) {

  kelpie::Pool dht = dht_full;
  auto kvs = generateAndPublish(dht, "list_test3");
  checkInfo(dht, kvs, false, kelpie::Availability::InLocalMemory);

  //Query1: See if we get everything with two wildcards: "row_list_test3*, *"
  kelpie::ObjectCapacities oc;
  rc = dht.List(kelpie::Key("row_list_test3*","*"), &oc );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows*P.num_cols, oc.keys.size()); // rows x cols worth of data
  for(int i=0;i<oc.keys.size(); i++) {
    //cout << "  " << oc.keys[i].str() << "  " << oc.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc.capacities[i]);
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K1(), "row_list_test3"));
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K2(), "col_list_test3"));
  }

  //Query2: See if we get everything with col wildcard: "row_list_test3, *"
  kelpie::ObjectCapacities oc2;
  rc = dht.List(kelpie::Key("row_list_test3_01","*"), &oc2 );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_cols, oc2.keys.size());  //Num cols worth of data
  for(int i=0;i<oc2.keys.size(); i++) {
    //cout << "  " << oc2.keys[i].str() << "  " << oc2.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc2.capacities[i]);
    EXPECT_EQ(   oc2.keys[i].K1(), "row_list_test3_01");
    EXPECT_TRUE( StringBeginsWith(oc2.keys[i].K2(), "col_list_test3"));
  }

  //Query3: See if we get a col wildcard: "*, col_list_test3_3
  kelpie::ObjectCapacities oc3;
  rc = dht.List(kelpie::Key("row_list_test3*","col_list_test3_03"), &oc3 );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows, oc3.keys.size());  //Num cols worth of data
  for(int i=0;i<oc3.keys.size(); i++) {
    //cout << "  " << oc3.keys[i].str() << "  " << oc3.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc3.capacities[i]);
    EXPECT_TRUE( StringBeginsWith( oc3.keys[i].K1(), "row_list_test3"));
    EXPECT_EQ(oc3.keys[i].K2(), "col_list_test3_03");
  }

}

TEST_F(MPIDHTTest, DropItemTestIndividual) {

  kelpie::Pool dht = dht_full;
  auto kvs = generateAndPublish(dht, "drop_test1");
  checkInfo(dht, kvs, false, kelpie::Availability::InLocalMemory);


  //Query1: See if we get everything with two wildcards: "row_drop_test1*, *"
  kelpie::ObjectCapacities oc;
  rc = dht.List(kelpie::Key("row_drop_test1*","*"), &oc );
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows*P.num_cols, oc.keys.size()); // rows x cols worth of data
  for(int i=0;i<oc.keys.size(); i++) {
    //cout << "  " << oc.keys[i].str() << "  " << oc.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc.capacities[i]);
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K1(), "row_drop_test1"));
    EXPECT_TRUE( StringBeginsWith(oc.keys[i].K2(), "col_drop_test1"));
  }

  auto rng = std::default_random_engine {};
  std::shuffle(kvs.begin(), kvs.end(), rng);

  //Remove each one, item by item
  for(size_t i=kvs.size(); i>0; ) {
    i--;
    //cout <<string(80,'-')<<endl;
    //cout <<"NEWTEST: Looking for "<<kvs[i].first.str()<<endl;
    kelpie::object_info_t col_info;
    //Verify this item exists
    rc = dht.Info(kvs[i].first, &col_info);
    //cout <<"Looking result "<<col_info.str()<<endl;
    EXPECT_NE( kelpie::Availability::Unavailable, col_info.col_availability);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);

    //Blocking drop of individual item
    //cout <<"Dropping "<<kvs[i].first.str()<<endl;
    rc = dht.BlockingDrop(kvs[i].first);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);


    //Verify this item exists
    //cout <<"Checking for "<<kvs[i].first.str()<<endl;
    rc = dht.Info(kvs[i].first, &col_info);
    EXPECT_EQ(kelpie::Availability::Unavailable, col_info.col_availability);
    //EXPECT_EQ(kelpie::KELPIE_ENOENT, rc);
    //cout <<"Check result was "<<col_info.str()<<endl;
  }

  //Sanity check: do the same list and make sure no entries
  //cout<<"Sanity check: verifying no entries\n";
  kelpie::ObjectCapacities oc2;
  rc = dht.List(kelpie::Key("row_drop_test1*","*"), &oc2 );
  //cout<<"Number of items originally: "<<oc.keys.size() <<" after: " <<oc2.keys.size()<<endl;
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(0,oc2.keys.size());
}

TEST_F(MPIDHTTest, DropItemTestWildCol) {

  kelpie::Pool dht = dht_full;
  auto kvs = generateAndPublish(dht, "drop_test2");
  checkInfo(dht, kvs, false, kelpie::Availability::InLocalMemory);

  //Query1: See if we get everything with two wildcards: "row_drop_test2*, *"
  kelpie::ObjectCapacities oc;
  rc = dht.List(kelpie::Key("row_drop_test2*", "*"), &oc);
  EXPECT_EQ(kelpie::KELPIE_OK, rc);
  EXPECT_EQ(P.num_rows*P.num_cols, oc.keys.size()); // rows x cols worth of data
  for(int i = 0; i<oc.keys.size(); i++) {
    //cout << "  " << oc.keys[i].str() << "  " << oc.capacities[i] << endl;
    EXPECT_EQ(P.ldo_size, oc.capacities[i]);
    EXPECT_TRUE(StringBeginsWith(oc.keys[i].K1(), "row_drop_test2"));
    EXPECT_TRUE(StringBeginsWith(oc.keys[i].K2(), "col_drop_test2"));
  }

  //Drop type 1: Remove all of row0.  ie row_00, *
  {
    kelpie::object_info_t row_info;
    kelpie::Key  row_key("row_drop_test2_00");
    kelpie::Key drop_key("row_drop_test2_00","*");
    rc = dht.RowInfo(row_key, &row_info);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);
    EXPECT_EQ(P.num_cols, row_info.row_num_columns);

    rc = dht.BlockingDrop(drop_key);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);

    rc = dht.RowInfo(row_key, &row_info);
    EXPECT_EQ(kelpie::KELPIE_ENOENT, rc);
    EXPECT_EQ(0, row_info.row_num_columns);
  }

  //Drop Type 2: Remove first 10 cols of row1. ie row_01, col_0*
  {
    kelpie::object_info_t row_info;
    kelpie::Key  row_key("row_drop_test2_01");
    kelpie::Key drop_key("row_drop_test2_01","col_drop_test2_0*");
    rc = dht.RowInfo(row_key, &row_info);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);
    EXPECT_EQ(P.num_cols, row_info.row_num_columns); //Should be 10 cols there

    //Do the drop
    rc = dht.BlockingDrop(drop_key);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);

    //Check for the whole row. Should be missing 10 columns
    rc = dht.RowInfo(row_key, &row_info);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);
    EXPECT_EQ(P.num_cols-10, row_info.row_num_columns);

    //ReCheck but use the drop wildcard. Should show 0
    rc = dht.RowInfo(drop_key, &row_info);
    EXPECT_EQ(kelpie::KELPIE_ENOENT, rc);
    EXPECT_EQ(0, row_info.row_num_columns);


  }

}

TEST_F(MPIDHTTest, ResultCollect) {


  kelpie::Pool pools[] = { dht_full, dht_front, dht_back, dht_single_self, dht_single_other };
  int num_pools = 5;


  int num_to_sync=10;
  kelpie::ResultCollector sync1(num_to_sync);
  lunasa::DataObject ldo1(1024);
  set<kelpie::Key> keys;
  for(int i=0; i<num_to_sync; i++) {
    kelpie::Key key("sync-test-"+to_string(i));
    rc = pools[ i%num_pools ].Publish(key, ldo1, sync1);
    keys.insert(key);
    EXPECT_EQ(kelpie::KELPIE_OK, rc);
  }

  //Wait for all ops to complete
  sync1.Sync();

  //Verify contents of sync
  for(int i=0; i<num_to_sync; i++){
    EXPECT_EQ(kelpie::ResultCollector::RequestType::Publish, sync1.results[i].request_type); //Should all be publish
    EXPECT_EQ(kelpie::KELPIE_OK, sync1.results[i].rc);
    EXPECT_EQ( 1024, sync1.results[i].info.col_user_bytes);
    //cout <<"col: "<<kelpie::availability_to_string(sync1.results[i].info.col_availability)<<endl;
  }

  //Use info to make sure everything exists
  for(int i=0; i<num_to_sync; i++) {
    rc = pools[ i%num_pools ].Info(kelpie::Key("sync-test-"+to_string(i)), nullptr);
    EXPECT_EQ(0, rc);
  }


  kelpie::ResultCollector sync2(num_to_sync);
  for(int i=0; i<num_to_sync; i++) {
    rc = pools[i%num_pools].Want(kelpie::Key("sync-test-"+to_string(i)), sync2);
    EXPECT_EQ(0,rc);
  }

  sync2.Sync();
  for(int i=0; i<num_to_sync; i++) {
    EXPECT_EQ(kelpie::ResultCollector::RequestType::Want, sync2.results[i].request_type);
    EXPECT_EQ(kelpie::KELPIE_OK, sync2.results[i].rc);
    EXPECT_EQ( 1024, sync2.results[i].info.col_user_bytes);
    EXPECT_EQ( 1024, sync2.results[i].ldo.GetDataSize());
    EXPECT_EQ(1, keys.count( sync2.results[i].key));
    keys.erase(sync2.results[i].key);
  }
  EXPECT_EQ(0, keys.size());

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
    //Register the dht
    DirectoryInfo dir_info1("dht:/dht_full",         "This is My DHT");
    DirectoryInfo dir_info2("dht:/dht_front_half",   "This DHT is on the first half of ranks");
    DirectoryInfo dir_info3("dht:/dht_back_half",    "This DHT is on the second half of ranks");
    DirectoryInfo dir_info4("dht:/dht_single_self",  "Single node, same as writer");
    DirectoryInfo dir_info5("dht:/dht_single_other", "Single node, different than writer");
    
    for(int i=0; i<G.mpi_size; i++){
      dir_info1.Join(G.nodes[i]);
      if(i<(G.mpi_size/2)) dir_info2.Join(G.nodes[i]);
      else                 dir_info3.Join(G.nodes[i]);
      if(i==0)             dir_info4.Join(G.nodes[i]);
      if(i==G.mpi_size-1)  dir_info5.Join(G.nodes[i]);
    }
    dirman::HostNewDir(dir_info1);
    dirman::HostNewDir(dir_info2);
    dirman::HostNewDir(dir_info3);
    dirman::HostNewDir(dir_info4);
    dirman::HostNewDir(dir_info5);
    
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
