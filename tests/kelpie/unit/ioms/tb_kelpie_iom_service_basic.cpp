// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 *  There is some significant overlap between this file and the iom_all_basic module.
 *  The best decomposition seems to be that those tests involve persistent storage
 *  on a locally accessible filesystem, while these require a network connection to a 
 *  service. The actual IOM testing is largely the same, but the way in which it has to
 *  integrate into GTest (a single test class) complicated things (like endpoint and keyspace name handling) 
 *  enough to warrant breaking this out. I didn't see the immediate benefit in breaking the duplicated pieces
 *  (for IOM setup etc) out into a base class from which this and the filesystem storage class
 *  would inherit, but that also seems like the right OO design thing to do.
 */

#include <iostream>
#include <vector>
#include <algorithm>

#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"
#include "kelpie/ioms/IomRegistry.hh"

#ifdef FAODEL_HAVE_CASSANDRA
#include "kelpie/ioms/IomCassandra.hh"
#endif

using namespace std;
using namespace faodel;
using namespace kelpie;

//The configuration used in this example
std::string default_config_string = R"EOF(

# Uncomment these options to get debug info for each component
#bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.iom_registry.debug true

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager  malloc
lunasa.eager_memory_manager malloc

)EOF";


template < typename T >
class IomSimple : public ::testing::Test {
protected:
  void SetUp() override {
    this->config.Append(default_config_string);
    bootstrap::Init(config, lunasa::bootstrap);
    this->lkv = new LocalKV();
    this->lkv->Init(config);
    bootstrap::Start();
  }

  void TearDown() override {
    delete this->lkv;
    bootstrap::Finish();
  }

  internal_use_only_t iuo;
  Configuration config;
  LocalKV *lkv;
};

TYPED_TEST_SUITE_P(IomSimple);


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

  std::memset(dptr->name, 0, 256);
  std::memset(dptr->name, 0, 256);

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
TYPED_TEST_P(IomSimple, ldo_gentest) {
  vector<lunasa::DataObject> ldos;
  for(int i=0; i<10; i++)
    ldos.push_back(createLDO(i, "bozo-"+to_string(i), i*100));
  for(int i=0; i<10; i++)
    checkLDO(ldos[i], i);
}

/*
 * This GTest class is supposed to be as generic as the filesystem-based one in 
 * tb_kelpie_iom_all_basic. However, we're polluting the settings keyword space
 * a little bit with keywords that not all the service-type IOMs might respond to.
 * 'endpoint' is general enough, 'keyspace' and 'table' less so. However, not all 
 * IOM subclasses have to respond to all keywords. 
 */

TYPED_TEST_P(IomSimple, write_direct) {

  string endpoint = "localhost";
  
  internal::IomBase *iom = new TypeParam("myiom", {{"endpoint",endpoint}});

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
}
TYPED_TEST_P(IomSimple, UsingConfigurationByRole){

  string endpoint = "localhost";
  string ks1 = "GTestAAA";
  string ks2 = "GTestBBB";

  //Create two ioms for this node only
  faodel::Configuration config(default_config_string);
  std::string s;

  s = "myrole.kelpie.iom.myiom1.type ";  config.Append(s + TypeParam::type_str);
  s = "myrole.kelpie.iom.myiom2.type ";  config.Append(s + TypeParam::type_str);

  config.Append("myrole.kelpie.iom.myiom1.endpoint",endpoint);
  config.Append("myrole.kelpie.iom.myiom1.keyspace",ks1);
  config.Append("myrole.kelpie.iom.myiom2.endpoint",endpoint);
  config.Append("myrole.kelpie.iom.myiom2.keyspace",ks2);
  config.Append("myrole.kelpie.ioms", "myiom1;myiom2");
  config.Append("node_role", "myrole");
  config.AppendFromReferences(); //Normally done in bootstrap

  internal::IomRegistry registry;
  registry.init(config);
  registry.start();
  cout <<registry.str(2);
  
  kelpie::internal::IomBase * ioms[2];
  ioms[0] = registry.Find("myiom1"); ASSERT_NE(nullptr, ioms[0]);
  ioms[1] = registry.Find("myiom2"); ASSERT_NE(nullptr, ioms[1]);
  auto iomX = registry.Find("myiomX"); EXPECT_EQ(nullptr, iomX);

  cout <<registry.str(100);

  //Verify we get the right settings back from the iom
  auto sp1=ioms[0]->Setting("endpoint"); EXPECT_EQ(endpoint, sp1);
  auto sk1=ioms[0]->Setting("keyspace"); EXPECT_EQ(ks1, sk1);
  auto sp2=ioms[1]->Setting("endpoint"); EXPECT_EQ(endpoint, sp2);
  auto sk2=ioms[1]->Setting("keyspace"); EXPECT_EQ(ks2, sk2);
  auto settings1 = ioms[0]->Settings();
  auto settings2 = ioms[1]->Settings();
  EXPECT_EQ(endpoint, settings1["endpoint"]);
  EXPECT_EQ(ks1,settings1["keyspace"]);
  EXPECT_EQ(endpoint, settings2["endpoint"]);
  EXPECT_EQ(ks2,settings2["keyspace"]);
  // TODO: THESE NEED TO CHANGE IF WE USE AN IOM THAT KEEPS MORE SETTINGS
  //       OTHERWISE THIS WILL CAUSE THE TEST TO FAIL
  // pmw: I'm commenting this out for now until we can take the time
  //     to think about how to best handle this. Ideally each IOM
  //     class definition would be able to report the expected size of
  //     their settings map here. But that's decoupled in those definitions
  //     from how those maps are defined (literals), so there's the possibility
  //     of those getting out of sync. That in turn could get fixed by not using
  //     literals in the IOM classes, so you could always ask a real map object
  //     what its size is, but you lose some convenience of expression and is that
  //     worth it and hopefully my choice to just comment out these test clauses maybe
  //     starts to make some sense for right now.
  //  EXPECT_EQ(1, settings1.size());
  //  EXPECT_EQ(1, settings2.size());

  registry.finish();
}



//Use a registry to track three separate storage locations
TYPED_TEST_P(IomSimple, iom_registry) {

  internal::IomRegistry registry;
  registry.init(Configuration(""));

  string endpoint = "localhost";
  char kx1[] = "GTestAAA";
  char kx2[] = "GTestBBB";
  char kx3[] = "GTestCCC";
  
  registry.RegisterIom(TypeParam::type_str, "myiom1", {{"endpoint",endpoint},{"keyspace",kx1},{"teardown","true"}} );
  registry.RegisterIom(TypeParam::type_str, "myiom2", {{"endpoint",endpoint},{"keyspace",kx2},{"teardown","true"}} );

  registry.start(); //Make the next one go in mutex-controlled section
  registry.RegisterIom(TypeParam::type_str, "myiom3", {{"endpoint",endpoint},{"keyspace",kx3}} );

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

// Must enumerate all tests in the generic fixture class here
REGISTER_TYPED_TEST_SUITE_P(IomSimple,
			   ldo_gentest,
			   write_direct,
			   UsingConfigurationByRole,
			   iom_registry);


// Must enumerate all desired IOM subclasses in this typedef
typedef ::testing::Types<
#ifdef FAODEL_HAVE_CASSANDRA
  kelpie::internal::IomCassandra
#endif
  > IomTypes;

// Since we don't have an always-tested service IOM type (like the POSIX file 
// IOM in the basic test suite), we need to make sure not to try to instantiate the
// test suite unless at least one service IOM test is defined. So in addition to enumerating
// the subclasses above, we have to check again here.

// These two things are logically connected and separating their resolution as done here is
// suboptimal. I was hoping there was a template metaprogramming/SFINAE solution here but
// I couldn't figure out a way to not do the instantiation at all if there were no
// subclass tests defined (without using the preprocessor). There's a way to figure out if 
// IomTypes is valid using std::is_class, but no way that I could find to avoid doing
// the instantiation at all in that case.

#if defined(FAODEL_HAVE_CASSANDRA) // || defined(FAODEL_HAVE_ANOTHER_SERVICE_IOM) || ...
INSTANTIATE_TYPED_TEST_SUITE_P(IomTypesInstantiation, IomSimple, IomTypes);
#endif
