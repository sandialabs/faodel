// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <vector>
#include <algorithm>
#include "gtest/gtest.h"

#include "kelpie/localkv/LocalKV.hh"

using namespace std;
using namespace faodel;
using namespace kelpie;


string default_config = R"EOF(

#kelpie.core_type nonet
#kelpie.debug true
#kelpie.lkv.debug true

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class LocalKVTest : public testing::Test {
protected:
  virtual void SetUp() {

    config.Append(default_config);

    bootstrap::Init(config, lunasa::bootstrap);

    //Create and configure our lkv
    lkv = new LocalKV();
    lkv->Init(config);

    DIM=32;
    for(int i=0; i<DIM; i++)
      for(int j=0; j<DIM; j++)
        ids.push_back(pair<int,int>(i,j));
    random_shuffle(ids.begin(), ids.end());

    bootstrap::Start(); //Do the actual start
  }

  void TearDown() override {
    delete lkv;          //This deregisters whookies, so do it before bootstrap finishes
    bootstrap::Finish();
  }

  internal_use_only_t iuo;
  vector<pair<int,int>> ids;
  int DIM;
  Configuration config;
  LocalKV *lkv; //Note: we keep this as a pointer because this needs to be destroyed *before* whookie shuts down

};
void setBuf(int *buf, int num, int owner, int x, int y){
  for(int i=0; i<num; i++)
    buf[i]=(x<<24) | (owner<<16) | (y<<8) | i;
}
int checkBuf(int *buf, int num, int owner, int x, int y){
  int bad=0;
  //printf("Checking for %d/ %d %d\n", num, x,y);
  for(int i=0; i<num; i++){
    //printf("Checking %d %x vs %x\n", i, buf[i], ((x<<24) | (y<<16) | i) );
    if(buf[i]!= ((x<<24) | (owner<<16) | (y<<8) | i))
      bad++;
  }
  return bad;
}

TEST_F(LocalKVTest, basics){

  int bufsize = 1024;
  bucket_t owner = bucket_t(36, iuo);
  int buf[bufsize];
  int rc;

  //Store things in random order
  random_shuffle(ids.begin(), ids.end());
  for(auto i_j : ids) {
    setBuf(buf, bufsize, owner.bid, i_j.first, i_j.second);

    lunasa::DataObject ldo(bufsize*sizeof(int));

    memcpy(ldo.GetDataPtr(), buf, bufsize*sizeof(int));
    Key key=KeyGen<int,int>(i_j.first, i_j.second);
    object_info_t info;
    rc = lkv->put(owner, key, ldo, PoolBehavior::DefaultBaseClass, nullptr, &info);
    EXPECT_EQ(KELPIE_OK,rc);
    EXPECT_EQ(Availability::InLocalMemory, info.col_availability);
    //EXPECT_EQ(ldo.GetUserCapacity(), info.col_user_bytes);
    EXPECT_EQ(bufsize*sizeof(int), info.col_user_bytes);
    EXPECT_GE(ldo.GetUserCapacity(), info.col_user_bytes);  //Note: ldo may be allocated more, usually for alignment
  }

  memset(buf, 4, sizeof(buf));
  //cout << "lkv is\n" << lkv->str(10); //Dumps all info about table out


  //Pull things out in a different random order
  random_shuffle(ids.begin(), ids.end());
  for(auto i_j : ids) {
    size_t ret_size=0;

    rc = lkv->getData(owner, KeyGen<int,int>(i_j.first,i_j.second), (void *)buf, sizeof(buf), &ret_size, nullptr);
    EXPECT_EQ(KELPIE_OK,rc);
    EXPECT_EQ(ret_size, sizeof(buf));

    rc = checkBuf(buf, bufsize, owner.bid, i_j.first, i_j.second);
    EXPECT_EQ(0,rc);

  }
}
TEST_F(LocalKVTest, ldoRefCount){
  //Verify that ref count goes up by one when we insert into lkv

  bucket_t owner = bucket_t(36, iuo);
  int blob_ints=1024;
  int blob_bytes=blob_ints*sizeof(int);
  int ref_count=-1;
  int rc;

  //Create an object, verify there is only one copy
  lunasa::DataObject ldo1(0, blob_bytes, lunasa::DataObject::AllocatorType::eager);

  int *x = ldo1.GetDataPtr<int *>();
  for(int i=0; i<blob_ints; i++) x[i]=i;
  ref_count = ldo1.internal_use_only.GetRefCount();          EXPECT_EQ(1, ref_count);

  //Create a second pointer to the object, verify ref count is 2
  lunasa::DataObject ldo1_copy = ldo1;
  ref_count = ldo1.internal_use_only.GetRefCount();          EXPECT_EQ(2, ref_count);
  ref_count = ldo1_copy.internal_use_only.GetRefCount();     EXPECT_EQ(2, ref_count);

  //Insert into lkv, verify ref count is 3
  rc = lkv->put(owner, Key("booya"), ldo1, PoolBehavior::DefaultBaseClass, nullptr, nullptr); EXPECT_EQ(KELPIE_OK, rc);
  ref_count = ldo1.internal_use_only.GetRefCount();        EXPECT_EQ(3, ref_count);
  ref_count = ldo1_copy.internal_use_only.GetRefCount();   EXPECT_EQ(3, ref_count);

  //Modify the original, should affect everyone
  x[0] = 2112;
  int *x2 = static_cast<int *>(ldo1.GetDataPtr());
  EXPECT_EQ(2112, x2[0]);
  EXPECT_EQ(x, x2); //Points should match

  //Get a reference from lkv That makes 4 references
  lunasa::DataObject ldo3_lkv;
  rc = lkv->get(owner, Key("booya"), &ldo3_lkv, nullptr); EXPECT_EQ(KELPIE_OK, rc);
  int *x3 = ldo3_lkv.GetDataPtr<int *>();
  ref_count = ldo1.internal_use_only.GetRefCount();
  EXPECT_EQ(x3[0], x[0]);
  EXPECT_EQ(x, x3);
  EXPECT_EQ(4, ref_count);

  //Drop the entry for lkv That should free up a reference
  rc = lkv->drop(owner, Key("booya"));
  ref_count = ldo1.internal_use_only.GetRefCount();
  EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(3, ref_count);
}

TEST_F(LocalKVTest, access){

  bucket_t owners[] = { bucket_t(100,iuo),
                        bucket_t(101,iuo),
                        bucket_t(102,iuo),
                        BUCKET_UNSPECIFIED };

  int bufsize = 1024;
  int buf[bufsize];
  int rc;

  //Store a bunch of things with same names under different owners
  for(int i=0; owners[i]!=BUCKET_UNSPECIFIED; i++){
    for(int r=0; r<10; r++){
      for(int c=0; c<10; c++){

        lunasa::DataObject ldo(0, bufsize*sizeof(int), lunasa::DataObject::AllocatorType::eager);
        setBuf(ldo.GetDataPtr<int *>(), bufsize, owners[i].bid, r,c);

        rc = lkv->put(owners[i], KeyGen<int, int>(r, c), ldo, PoolBehavior::DefaultBaseClass, nullptr, nullptr);
        EXPECT_EQ(KELPIE_OK,rc);
      }
    }
  }
  //Make sure they're all ok
  for(int i=0; owners[i]!=BUCKET_UNSPECIFIED; i++){
    for(int r=0; r<10; r++){
      for(int c=0; c<10; c++){
        size_t ret_size=0;

        rc = lkv->getData(owners[i], KeyGen<int,int>(r,c), (void *)buf, sizeof(buf), &ret_size, nullptr);
        EXPECT_EQ(KELPIE_OK,rc);
        EXPECT_EQ(ret_size, sizeof(buf));

        rc = checkBuf(buf,bufsize, owners[i].bid, r,c);
        EXPECT_EQ(0,rc);
      }
    }
  }

  //Try getting at things via different owners
  bucket_t b;
  for(int i=90; i<99; i++){
    b=bucket_t(i,iuo);
    for(int r=0; r<10; r++)
      for(int c=0; c<10; c++){
        size_t ret_size=0;
        rc = lkv->getData(b, KeyGen<int,int>(r,c), (void *)buf, sizeof(buf), &ret_size, nullptr);
        EXPECT_EQ(KELPIE_ENOENT,rc);
      }
  }
}

TEST_F(LocalKVTest, IgnoreWrites) {

  int rc;
  bucket_t bucket("bucky");
  Key k1("nothere");
  lunasa::DataObject ldo1(1024);

  lunasa::DataObject ldo_return;

  //LKV Should only do a write if the WriteToLocal bit is set. It still checks dependencies, but otherwise returns w/ no entry
  rc = lkv->put(bucket, k1, ldo1, PoolBehavior::NoAction,      nullptr, nullptr ); EXPECT_EQ(KELPIE_ENOENT, rc);
  rc = lkv->put(bucket, k1, ldo1, PoolBehavior::WriteToRemote, nullptr, nullptr ); EXPECT_EQ(KELPIE_ENOENT, rc);
  rc = lkv->put(bucket, k1, ldo1, PoolBehavior::WriteToIOM,    nullptr, nullptr ); EXPECT_EQ(KELPIE_EIO, rc);

  //Double check there's no data
  rc = lkv->get(bucket, k1, &ldo_return, nullptr);                                 EXPECT_EQ(KELPIE_ENOENT, rc);


  //Make sure a local write works
  rc = lkv->put(bucket, k1, ldo1, PoolBehavior::WriteToLocal,  nullptr, nullptr ); EXPECT_EQ(KELPIE_OK, rc);
  rc = lkv->get(bucket, k1, &ldo_return, nullptr);                                 EXPECT_EQ(KELPIE_OK, rc);
  EXPECT_EQ(1024, ldo_return.GetUserSize());

}

TEST_F(LocalKVTest, ListRowStar) {

  int rc;
  bucket_t bucket("bucky");

  string row_names[]={"nothing1","nothing2","nothing3","nothing4",
                      "thing1","thing2","thing3",
                      "nothing5","nothing6",
                      "thing4"};

  map<string,int> row_to_size;
  int i=0;
  for( auto s : row_names) {
    lunasa::DataObject ldo(100+i);
    rc = lkv->put(bucket, kelpie::Key(s), ldo, PoolBehavior::WriteToLocal, nullptr,nullptr); EXPECT_EQ(KELPIE_OK, rc);
    row_to_size[s] = 100+i;
    i++;
  }

  { //Get thing*|"" (four rows)
    ObjectCapacities oc;
    rc = lkv->list(bucket, kelpie::Key("thing*"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(oc.keys.size(), oc.capacities.size());
    EXPECT_EQ(4, oc.keys.size());
    for(int i = 0; i<oc.keys.size(); i++) {
      string k1 = oc.keys[i].K1();
      EXPECT_TRUE(StringBeginsWith(k1, "thing"));
      EXPECT_EQ(row_to_size[k1], oc.capacities[i]);
    }
  }

  { //Get specific row/col: "thing3|"
    ObjectCapacities oc;
    rc = lkv->list(bucket, kelpie::Key("thing3"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(oc.keys.size(), oc.capacities.size());
    EXPECT_EQ(1, oc.keys.size());
    if(oc.keys.size()>0) {
      EXPECT_EQ(Key("thing3"), oc.keys[0]);
      EXPECT_EQ(row_to_size["thing3"], oc.capacities[0]);
    }

  }

}
TEST_F(LocalKVTest, ListRowColStars) {

  int rc;
  bucket_t bucket("bucky");

  string row_names[] = {"some", "random", "column", "names", "go", "heree", "sowhat"};

  string col_names[] = {"nothing1", "nothing2", "nothing3", "nothing4",
                        "thing1", "thing2", "thing3",
                        "nothing5", "nothing6",
                        "",
                        "thing4"};

  map<kelpie::Key, int> keymap_sizes; //records how big the ldo is

  //Insert a bunch or keys into lkv
  int i = 0;
  for(auto r : row_names) {
    for(auto c : col_names) {
      Key k(r,c);
      lunasa::DataObject ldo(100 + i);
      rc = lkv->put(bucket, k, ldo, PoolBehavior::WriteToLocal, nullptr, nullptr);
      EXPECT_EQ(KELPIE_OK, rc);
      keymap_sizes[k] = 100 +i;
      i++;
    }
  }


  //Good keys: Look for a list of exact matches
  vector<kelpie::Key> exact_keys;
  exact_keys.push_back(Key("names", "thing3"));
  exact_keys.push_back(Key("random","nothing1"));
  exact_keys.push_back(Key("go",    ""));
  exact_keys.push_back(Key("some",   "thing4"));
  for(auto k : exact_keys) {
    ObjectCapacities oc;
    rc = lkv->list(bucket, k, nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(1, oc.keys.size()); EXPECT_EQ(1, oc.capacities.size());
    if((oc.keys.size()==1) && (oc.capacities.size() == 1)) {
      EXPECT_EQ( k, oc.keys[0]);
      EXPECT_EQ( keymap_sizes[k], oc.capacities[0]);
    }
  }

  //Missing keys: shouldn't find anything
  vector<kelpie::Key> missing_keys;
  missing_keys.push_back(Key("Xnames", "thing3"));  //bad row
  missing_keys.push_back(Key("names",  "thing3X")); //bad col
  missing_keys.push_back(Key("Xname",  "Xthing3")); //bad row/col
  for(auto k : missing_keys) {
    ObjectCapacities oc;
    rc = lkv->list(bucket, k, nullptr, &oc);
    EXPECT_EQ(KELPIE_ENOENT, rc);
    EXPECT_EQ(0, oc.keys.size()); EXPECT_EQ(0,oc.capacities.size());
  }


  { //Exact Row, Col*
    ObjectCapacities oc;
    rc = lkv->list(bucket, Key("go", "thing*"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(4, oc.keys.size()); EXPECT_EQ(4,oc.capacities.size());
    vector<string> found_cols;
    for(int i=0; i<oc.keys.size(); i++) {
      EXPECT_EQ(keymap_sizes[oc.keys[i]], oc.capacities[i]);
      EXPECT_EQ("go", oc.keys[i].K1());
      EXPECT_TRUE(StringBeginsWith(oc.keys[i].K2(), "thing"));
      found_cols.push_back(oc.keys[i].K2());
    }
    sort(found_cols.begin(), found_cols.end());
    vector<string> expected_cols{"thing1","thing2","thing3", "thing4"};
    EXPECT_TRUE( (found_cols==expected_cols));
  }


  { //Row*, Exact col
    ObjectCapacities oc;
    rc = lkv->list(bucket, Key("so*", "thing3"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(2, oc.keys.size()); EXPECT_EQ(2,oc.capacities.size());
    vector<string> found_rows;
    for(int i=0; i<oc.keys.size(); i++) {
      EXPECT_EQ(keymap_sizes[oc.keys[i]], oc.capacities[i]);
      EXPECT_TRUE(StringBeginsWith(oc.keys[i].K1(), "so"));
      EXPECT_EQ(oc.keys[i].K2(), "thing3");
      found_rows.push_back(oc.keys[i].K1());
    }
    sort(found_rows.begin(), found_rows.end());
    vector<string> expected_rows{"some","sowhat"};
    EXPECT_TRUE( (found_rows==expected_rows));
  }

  { //Row*, Col*
    ObjectCapacities oc;
    rc = lkv->list(bucket, Key("so*", "thing*"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(8, oc.keys.size()); EXPECT_EQ(8,oc.capacities.size());
    vector<string> found_rows;
    vector<string> found_cols;
    for(int i=0; i<oc.keys.size(); i++) {
      EXPECT_EQ(keymap_sizes[oc.keys[i]], oc.capacities[i]);
      EXPECT_TRUE(StringBeginsWith(oc.keys[i].K1(), "so"));
      EXPECT_TRUE(StringBeginsWith(oc.keys[i].K2(), "thing"));
      found_rows.push_back(oc.keys[i].K1());
      found_cols.push_back(oc.keys[i].K2());
    }
    sort(found_rows.begin(), found_rows.end());
    sort(found_cols.begin(), found_cols.end());
    vector<string> expected_rows{"some","some","some","some","sowhat","sowhat","sowhat","sowhat"};
    vector<string> expected_cols{"thing1","thing1","thing2", "thing2","thing3","thing3","thing4", "thing4"};
    EXPECT_TRUE( (found_rows==expected_rows));
    EXPECT_TRUE( (found_cols==expected_cols));
  }

}

TEST_F(LocalKVTest, DropIndividual) {

  int rc;
  bucket_t bucket("bucky");

  string row_names[] = {"some", "random", "column", "names", "go", "heree", "sowhat"};

  string col_names[] = {"nothing1", "nothing2", "nothing3", "nothing4",
                        "thing1", "thing2", "thing3",
                        "nothing5", "nothing6",
                        "",
                        "thing4"};

  map<kelpie::Key, int> keymap_sizes; //records how big the ldo is

  //Insert a bunch or keys into lkv
  int i = 0;
  for(auto r : row_names) {
    for(auto c : col_names) {
      Key k(r, c);
      lunasa::DataObject ldo(100 + i);
      rc = lkv->put(bucket, k, ldo, PoolBehavior::WriteToLocal, nullptr, nullptr);
      EXPECT_EQ(KELPIE_OK, rc);
      keymap_sizes[k] = 100 + i;
      i++;
    }
  }



  //Try dropping each one
  for(auto &key_size : keymap_sizes) {
    //Verify object is there
    //cout <<"Key is "<<key_size.first.str()<<" size is "<<key_size.second<<endl;
    ObjectCapacities oc;
    rc = lkv->list(bucket, key_size.first, nullptr, &oc);

    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(1, oc.keys.size());
    EXPECT_EQ(1, oc.capacities.size());
    if(oc.capacities.size()==1) {
      EXPECT_EQ(key_size.second, oc.capacities[0]);
    }
    //Drop it
    rc = lkv->drop(bucket,key_size.first);
    EXPECT_EQ(KELPIE_OK, rc);

    //Make sure it doesn't show up
    oc.Wipe();
    rc = lkv->list(bucket, key_size.first, nullptr, &oc);

    EXPECT_EQ(KELPIE_ENOENT, rc);
    EXPECT_EQ(0, oc.capacities.size());
    //cout<<lkv->str(10);

  }

}

TEST_F(LocalKVTest, DropRow) {

  int rc;
  bucket_t bucket("bucky");

  string row_names[] = {"ignore", "random", "row1", "bob", "row2", "go","stuff1","stuff2"};

  string col_names[] = {"nothing1", "nothing2", "nothing3", "nothing4",
                        "thing1", "thing2", "thing3",
                        "nothing5", "nothing6",
                        "",
                        "thing4"};

  map<kelpie::Key, int> keymap_sizes; //records how big the ldo is

  //Insert a bunch or keys into lkv
  int i = 0;
  for(auto r : row_names) {
    for(auto c : col_names) {
      Key k(r, c);
      lunasa::DataObject ldo(100 + i);
      rc = lkv->put(bucket, k, ldo, PoolBehavior::WriteToLocal, nullptr, nullptr);
      EXPECT_EQ(KELPIE_OK, rc);
      keymap_sizes[k] = 100 + i;
      i++;
    }
  }

  //Remove some columns
  {
    kelpie::Key k1("bob", "thing*");
    ObjectCapacities oc;
    rc = lkv->drop(bucket, k1);      EXPECT_EQ(KELPIE_OK, rc);
    rc = lkv->list(bucket, k1, nullptr, &oc); EXPECT_EQ(KELPIE_ENOENT, rc);
    rc = lkv->list(bucket, kelpie::Key("bob","*"), nullptr, &oc); EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(7, oc.capacities.size());
  }

  //Remove all columns
  {
    kelpie::Key k1("go", "*");
    ObjectCapacities oc;
    rc = lkv->drop(bucket, k1);      EXPECT_EQ(KELPIE_OK, rc);
    rc = lkv->list(bucket, k1, nullptr, &oc); EXPECT_EQ(KELPIE_ENOENT, rc);
    rc = lkv->list(bucket, kelpie::Key("go","*"), nullptr, &oc);
    EXPECT_EQ(KELPIE_ENOENT, rc);
    EXPECT_EQ(0, oc.capacities.size());
  }

  //Remove some columns on multiple rows
  {
    kelpie::Key k1("stuff*", "thing*");
    ObjectCapacities oc;
    rc = lkv->drop(bucket, k1);      EXPECT_EQ(KELPIE_OK, rc);
    rc = lkv->list(bucket, k1, nullptr, &oc); EXPECT_EQ(KELPIE_ENOENT, rc);
    rc = lkv->list(bucket, kelpie::Key("stuff*","*"), nullptr, &oc);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(14, oc.capacities.size());
  }

  //Remove all columns on multiple rows
  {
    kelpie::Key k1("stuff*", "*");
    ObjectCapacities oc;
    rc = lkv->drop(bucket, k1);      EXPECT_EQ(KELPIE_OK, rc);
    rc = lkv->list(bucket, k1, nullptr, &oc); EXPECT_EQ(KELPIE_ENOENT, rc);
  }

}

TEST_F(LocalKVTest, GetRowStar) {

  int rc;
  bucket_t bucket("bucky");

  string row_names[] = {"some", "random", "column", "names", "go", "heree", "sowhat"};

  string col_names[] = {"nothing1", "nothing2", "nothing3", "nothing4",
                        "thing1", "thing2", "thing3",
                        "nothing5", "nothing6",
                        "",
                        "thing4"};

  map<kelpie::Key, string> keymap_encodes; //records the name of the key into the ldo

  //Insert a bunch or keys into lkv
  int i = 0;
  for(auto r : row_names) {
    for(auto c : col_names) {
      Key k(r,c);

      string enc = k.pup();
      lunasa::DataObject ldo(enc.size());
      memcpy(ldo.GetDataPtr(), enc.c_str(), enc.size());
      keymap_encodes[k] = enc;

      rc = lkv->put(bucket, k, ldo, PoolBehavior::WriteToLocal, nullptr, nullptr);
      EXPECT_EQ(KELPIE_OK, rc);
      i++;
    }
  }

  //Good keys: Look for a list of exact matches
  vector<kelpie::Key> exact_keys;
  exact_keys.push_back(Key("names", "thing3"));
  exact_keys.push_back(Key("random","nothing1"));
  exact_keys.push_back(Key("go",    ""));
  exact_keys.push_back(Key("some",   "thing4"));
  for(auto k : exact_keys) {
    std::map<Key, lunasa::DataObject> ldos;
    rc = lkv->getAvailable(bucket, k, ldos);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(1, ldos.size());
    if(ldos.size()==1) {
      EXPECT_EQ( k, ldos.begin()->first);
      auto tmp_ldo = ldos.begin()->second;
      string s1(tmp_ldo.GetDataPtr<char *>(), tmp_ldo.GetDataSize());
      EXPECT_EQ(keymap_encodes[k], s1);
    }
  }

  { //Simple wildcard
    Key kstar("names", "thing*");
    vector<Key> expected_keys;
    for(int i = 1; i < 5; i++) expected_keys.push_back(Key("names", "thing"+to_string(i)));

    std::map<Key, lunasa::DataObject> ldos;
    rc = lkv->getAvailable(bucket, kstar, ldos);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(4, ldos.size());
    if(ldos.size() == 4) {
      for(auto &k : expected_keys) {
        //cout << "Checking " << k.str() << "\n";
        EXPECT_EQ(1, ldos.count(k));
        if(ldos.count(k) == 1) {
          auto tmp_ldo = ldos[k];
          string s1(tmp_ldo.GetDataPtr<char *>(), tmp_ldo.GetDataSize());
          EXPECT_EQ(keymap_encodes[k], s1);
        }
      }
    }

  }

  { //Get all cols in a row
    Key kstar2 = Key("names", "*");
    vector<Key> expected_keys2;
    for(auto c: col_names)
      expected_keys2.push_back( Key("names", c) );
    std::map<Key, lunasa::DataObject> ldos2;
    rc = lkv->getAvailable(bucket, kstar2, ldos2);
    EXPECT_EQ(KELPIE_OK, rc);
    EXPECT_EQ(11, ldos2.size());
    if(ldos2.size()==11) {
      for(auto &k : expected_keys2) {
        //cout <<"Checking "<<k.str()<<"\n";
        EXPECT_EQ(1, ldos2.count(k) );
        if(ldos2.count(k)==1) {
          auto tmp_ldo = ldos2[k];
          string s1(tmp_ldo.GetDataPtr<char *>(), tmp_ldo.GetDataSize());
          EXPECT_EQ(keymap_encodes[k], s1);
        }
      }
    }
  }

  { //Check none
    vector<Key> bogus = { Key("bogus"), Key("some","nocol"), Key("foo","bar")};
    for(auto &k : bogus) {
      std::map<Key, lunasa::DataObject> ldos;
      rc = lkv->getAvailable(bucket, k, ldos);
      EXPECT_EQ(KELPIE_ENOENT, rc);
      EXPECT_EQ(0, ldos.size());
    }
  }


}

