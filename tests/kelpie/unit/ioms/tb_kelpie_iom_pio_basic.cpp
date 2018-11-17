// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <vector>
#include <algorithm>

#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/ioms/IomRegistry.hh"
#include "kelpie/ioms/IomPosixIndividualObjects.hh"


using namespace std;
using namespace faodel;
using namespace kelpie;

//The configuration used in this example
std::string default_config_string = R"EOF(

# Uncomment these options to get debug info for each component
#bootstrap.debug true
#webhook.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.debug    true

kelpie.iom_registry.debug true

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class IomPosixIOSimple : public testing::Test {
protected:
  void SetUp() override {
    config.Append(default_config_string);
    bootstrap::Init(config, lunasa::bootstrap);
    lkv = new LocalKV();
    lkv->Init(config);
    bootstrap::Start();
  }

  void TearDown() override {
    delete lkv;
    bootstrap::Finish();
  }

  internal_use_only_t iuo;
  Configuration config;
  LocalKV *lkv;
};

struct test_data_t {
  uint32_t block_id;
  uint32_t data_bytes;
  char name[256];
  uint8_t data[0];
};

lunasa::DataObject createLDO(int id, string name, int data_bytes) {
  
  lunasa::DataObject ldo(sizeof(test_data_t),
                         sizeof(test_data_t)+data_bytes,
                         lunasa::DataObject::AllocatorType::eager);

  auto mptr = ldo.GetMetaPtr<test_data_t *>();
  auto dptr = ldo.GetDataPtr<test_data_t *>();

  mptr->block_id = id;
  mptr->data_bytes=0;

  memset(dptr->name, 0, 256);
  memset(dptr->name, 0, 256);

  string meta_name = "id-"+std::to_string(id);
  strncpy(mptr->name, meta_name.c_str(), 255);
  strncpy(dptr->name, name.c_str(), 255);

  dptr->block_id = id;
  dptr->data_bytes=data_bytes;
  for(uint32_t i=0; i<data_bytes; i++)
    dptr->data[i] = (i&0x0FF);

  return ldo;
}

bool checkLDO(const lunasa::DataObject &ldo, int id) {
  EXPECT_EQ(sizeof(test_data_t), ldo.GetMetaSize());
  auto mptr = ldo.GetMetaPtr<test_data_t *>();
  auto dptr = ldo.GetDataPtr<test_data_t *>();

  EXPECT_EQ(id, mptr->block_id);
  EXPECT_EQ(id, dptr->block_id);
  EXPECT_EQ(0,  mptr->data_bytes);

  string mid = string(mptr->name);
  EXPECT_EQ("id-"+std::to_string(id), mid);

  int bad_count=0;
  for(int i=0; i<dptr->data_bytes; i++)
    if( dptr->data[i] != (i&0x0FF) ) bad_count++;
  EXPECT_EQ(0, bad_count);

  return true;
}

//Make sure generators are working ok
TEST_F(IomPosixIOSimple, ldo_gentest) {
  vector<lunasa::DataObject> ldos;
  for(int i=0; i<10; i++)
    ldos.push_back(createLDO(i, "bozo-"+to_string(i), i*100));
  for(int i=0; i<10; i++)
    checkLDO(ldos[i], i);
}

TEST_F(IomPosixIOSimple, write_direct) {

  char p[] = "/tmp/gtestXXXXXX";
  string path = mkdtemp(p);

  internal::IomBase *iom = new kelpie::internal::IomPosixIndividualObjects("myiom", {{"path",path}});

  vector<Key> keys;
  bucket_t bucket("my_bucket");
  for(int i=0; i<10; i++){
    auto ldo = createLDO(i, "bozo-"+to_string(i), i*2);
    kelpie::Key k("mybigitem",std::to_string(i));
    iom->WriteObject(bucket, k, ldo);
    keys.push_back(k);
  }

  vector<std::pair<Key, lunasa::DataObject>> found_items;
  vector<Key> missing_keys;
  iom->ReadObjects(bucket, keys, &found_items, &missing_keys);
  EXPECT_EQ(keys.size(), found_items.size());
  EXPECT_EQ(0, missing_keys.size());
  for(auto &key_ldo : found_items) {
    int i=0;
    while((i<keys.size()) && (keys[i] != key_ldo.first))
      i++;
    if(i<keys.size()){
      checkLDO(key_ldo.second, i);
    } else {
      EXPECT_TRUE(false);
    }
  }
  delete iom;
}
TEST_F(IomPosixIOSimple, UsingConfigurationByRole){

  char px1[] = "/tmp/gtestXXXXXX";
  char px2[] = "/tmp/gtestXXXXXX";
  string p1 = mkdtemp(px1);
  string p2 = mkdtemp(px2);

  //Create two ioms for this node only
  faodel::Configuration config(default_config_string);
  config.Append("myrole.iom.myiom1.type PosixIndividualObjects");
  config.Append("myrole.iom.myiom2.type PosixIndividualObjects");
  config.Append("myrole.iom.myiom1.path",p1);
  config.Append("myrole.iom.myiom2.path",p2);
  config.Append("myrole.ioms", "myiom1;myiom2");
  config.Append("node_role", "myrole");
  config.AppendFromReferences();
  
  internal::IomRegistry registry;
  registry.init(config);
  registry.start();
  cout <<registry.str(2);
  
  kelpie::internal::IomBase * ioms[2];
  cout<<"About to look\n";
  ioms[0] = registry.Find("myiom1"); ASSERT_NE(nullptr, ioms[0]);
  ioms[1] = registry.Find("myiom2"); ASSERT_NE(nullptr, ioms[1]);
  auto iomX = registry.Find("myiomX"); EXPECT_EQ(nullptr, iomX);

  cout <<registry.str(100);

  //Verify we get the right settings back from the iom
  auto sp1=ioms[0]->Setting("path"); EXPECT_EQ(p1, sp1);
  auto sp2=ioms[1]->Setting("path"); EXPECT_EQ(p2, sp2);
  auto settings1 = ioms[0]->Settings();
  auto settings2 = ioms[1]->Settings();
  EXPECT_EQ(p1, settings1["path"]);
  EXPECT_EQ(p2, settings2["path"]);
  EXPECT_EQ(1, settings1.size());
  EXPECT_EQ(1, settings2.size());

  registry.finish();
}



//Use a registry to track three separate storage locations
TEST_F(IomPosixIOSimple, iom_registry) {

  internal::IomRegistry registry;
  registry.init(Configuration(""));

  char px1[] = "/tmp/gtestXXXXXX";
  char px2[] = "/tmp/gtestXXXXXX";
  char px3[] = "/tmp/gtestXXXXXX";
  string p1 = mkdtemp(px1);
  string p2 = mkdtemp(px2);
  string p3 = mkdtemp(px3);

  EXPECT_NE(p1, p2);
  EXPECT_NE(p2, p3);
  EXPECT_NE(p1, p3);
  
  registry.RegisterIom("PosixIndividualObjects", "myiom1", {{"path",p1}} );
  registry.RegisterIom("PosixIndividualObjects", "myiom2", {{"path",p2}} );

  registry.start(); //Make the next one go in mutex-controlled section
  registry.RegisterIom("PosixIndividualObjects", "myiom3", {{"path",p3}} );

  //See if we can locate each one
  kelpie::internal::IomBase * ioms[3];
  ioms[0] = registry.Find("myiom1"); ASSERT_NE(nullptr, ioms[0]);
  ioms[1] = registry.Find("myiom2"); ASSERT_NE(nullptr, ioms[1]);
  ioms[2] = registry.Find("myiom3"); ASSERT_NE(nullptr, ioms[2]);
  auto iomX = registry.Find("myiomX"); EXPECT_EQ(nullptr, iomX);

  bucket_t buckets[3];
  buckets[0]=bucket_t("mybucketA");
  buckets[1]=bucket_t("mybucketB");
  buckets[2]=bucket_t("mybucketC");

  //Write an item to each iom, using the same bucket
  vector<Key> keys;
  for(int i=0; i<3; i++) {
    auto ldo = createLDO(i, "bozo-"+to_string(i), i*2);
    kelpie::Key k("mybigitem",std::to_string(i));
    ioms[i]->WriteObject(buckets[0], k, ldo);
    keys.push_back(k);
  }

  //Verify we can get each item back
  for(int i=0; i<3; i++) {
    lunasa::DataObject ldo;
    rc_t rc = ioms[i]->ReadObject(buckets[0], keys[i], &ldo);
    EXPECT_EQ(KELPIE_OK, rc);
    checkLDO(ldo, i);
  }

  registry.finish();
}
