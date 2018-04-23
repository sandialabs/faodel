// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include "gtest/gtest.h"

#include "Trios_threads.h"

#include "NodeIDPacker.hh"

#include "Kelpie.hh"
#include "TimeLogger.hh"

using namespace std;
using namespace kelpie;

const string url_file = ".server-url";
typedef struct {
  string server_url;
} G_t;
G_t G = { "" };


string default_config = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server

#
security_bucket                       bobbucket

# Server: Run a dedicated server that has a resource manager server named /
server.rpc_server_type                single
#server.net.url.write_to_file          .server-url
server.resource_manager.type          server
server.resource_manager.path          /bob
server.resource_manager.write_to_file .server-url

# Client: Don't use a server, just send requests
client.rpc_server_type                 none
client.resource_manager.path           /bob/1
#client.resource_manager.read_from_file .server-url
)EOF";

void mkData(int *buf, int num_words, int tag){
  for(int i=0; i<num_words; i++){
    buf[i] = tag<<24 | i;
  }
}
int ckData(int *buf, int num_words, int tag){
  int bad=0;
  for(int i=0; i<num_words; i++){
    if(tag==0){
      if(buf[i]!=0) bad++; //expecting all zeros
    } else {
      if(buf[i]!=(tag<<24|i)) bad++;
    }
  }
  return bad;
}

class LocalResourceTest : public testing::Test {
protected:
  virtual void SetUp(){

    //Start a kelpie instance, connect a local resource, then setup some fake data

    struct timeval tv;
    gettimeofday(&tv,NULL);
    srand(tv.tv_usec);
    tag = rand() & 0x0FF;
    num_words = 1024; num_bytes=num_words*sizeof(int);
    //cout <<"Doing setup"<<tag<<"\n";
    kelpie = new Kelpie();

    Configuration conf;
    conf.Append(default_config);
    conf.Append("node_role","client");
    kelpie->Init(conf);

    r = kelpie->Connect("local:");

    buf_src = new int[num_words]; mkData(buf_src, num_words, tag);
    buf_dst = new int[num_words]; memset(buf_dst, 0, num_bytes);

    k = Key("howdy", "bob");
    k_missing= Key("not","a key that exists");

    reqs = new RequestHandle[10];
    rid=0;
  }
  virtual void TearDown(){
    //cout <<"Doing teardown\n";
    delete[] buf_src;
    delete[] buf_dst;
    delete[] reqs;
    delete kelpie;
  }

  Kelpie *kelpie;
  Resource *r;
  int *buf_src;
  int *buf_dst;
  int num_words;
  size_t num_bytes;
  int tag;
  Key k;
  Key k_missing;
  int bad;
  rc_t rc;
  RequestHandle *reqs;
  nodeid_t origin;
  size_t mem_size;
  int rid; //request id
};


TEST_F(LocalResourceTest, PutGetSimple){

  ASSERT_TRUE(r!=NULL);

  //Verify nothing lives here
  rc = r->Get(k,buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                         EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.get.found_local);
  EXPECT_EQ(0, reqs[rid].result.get.returned_bytes);
  rid++;

  //Put the data
  rc = r->Put(k, buf_src, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                          EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.put.already_existed);
  EXPECT_TRUE(reqs[rid].result.put.wrote_local);
  rid++;

  //Get the data
  rc = r->Get(k, buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                          EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_TRUE(reqs[rid].result.get.found_local);
  EXPECT_EQ(num_bytes, reqs[rid].result.get.returned_bytes);
  rid++;

  //Verify it's correct
  bad = ckData(buf_dst, num_words, tag);
  EXPECT_EQ(0,bad);

  //Get some data that doesn't exist
  rc = r->Get(k_missing,buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                                 EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.get.found_local);
  EXPECT_EQ(0, reqs[rid].result.get.returned_bytes);
  rid++;

}

TEST_F(LocalResourceTest, PutGetPartials){

  ASSERT_TRUE(r!=NULL);

  //Verify nothing lives here
  rc = r->Get(k,buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                         EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.get.found_local);
  EXPECT_EQ(0, reqs[rid].result.get.returned_bytes);
  rid++;

  //Put the full data
  rc = r->Put(k, buf_src, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                          EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.put.already_existed);
  EXPECT_TRUE(reqs[rid].result.put.wrote_local);
  rid++;

  //Get only half the data
  memset(buf_dst,0, num_bytes);
  rc = r->Get(k, buf_dst, num_bytes/2, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                            EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_TRUE(reqs[rid].result.get.found_local);
  EXPECT_EQ(num_bytes/2, reqs[rid].result.get.returned_bytes);
  EXPECT_EQ(num_bytes,   reqs[rid].result.get.available_bytes);
  rid++;

  //Verify it's correct
  bad = ckData(buf_dst, num_words/2, tag);               EXPECT_EQ(0,bad);
  bad = ckData(&buf_dst[num_words/2], num_words/2, 0);   EXPECT_EQ(0,bad);


  //Get some data that doesn't exist
  rc = r->Get(k_missing,buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                                 EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.get.found_local);
  EXPECT_EQ(0, reqs[rid].result.get.returned_bytes);
  rid++;

}


TEST_F(LocalResourceTest, GetSptr){

  ASSERT_TRUE(r!=NULL);

  //Verify nothing lives here
  rc = r->Get(k,buf_dst, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                         EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.get.found_local);
  EXPECT_EQ(0, reqs[rid].result.get.returned_bytes);
  rid++;

  //Store some data
  rc = r->Put(k, buf_src, num_bytes, &reqs[rid]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[rid].Wait();                          EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[rid].result.put.already_existed);
  EXPECT_TRUE( reqs[rid].result.put.wrote_local);
  rid++;

  //Fetch as a shared pointer
  { //encapsulate the shared pointer so we limit how long we keep it
    shared_ptr<char> mem_sptr;
    rc = r->GetLocal(k, mem_sptr, &mem_size, &origin); EXPECT_EQ(KELPIE_OK,rc);
    if(rc==KELPIE_OK){
      int *tmp = reinterpret_cast<int *>(mem_sptr.get());
      bad = ckData(tmp, num_words, tag);
      EXPECT_EQ(0, bad);
      //Check sptr count and make sure we have a copy and lkv has a copy
      int num_sptr_owners = mem_sptr.use_count();
      EXPECT_EQ(2, num_sptr_owners);
    }
  }
}

TEST_F(LocalResourceTest, PutGetSptr){

  ASSERT_TRUE(r!=NULL);

  int num_sptr_owners;

  rc = r->Get(k,buf_dst, num_bytes, &reqs[0]); EXPECT_EQ(KELPIE_OK,rc);
  rc = reqs[0].Wait();                         EXPECT_EQ(KELPIE_OK,rc);
  EXPECT_FALSE(reqs[0].result.get.found_local);
  EXPECT_EQ(0, reqs[0].result.get.returned_bytes);


  //Write shared ptr, then read a shared ptr, deallocate ptr, check the ref count
  { //encapsulate the shared pointer so we limit how long we keep it
    shared_ptr<char> mem_sptr(new char[num_bytes],std::default_delete<char[]>());
    memcpy(mem_sptr.get(), buf_src, num_bytes);

    rc = r->Put(k, mem_sptr, num_bytes, &reqs[1]); EXPECT_EQ(KELPIE_OK,rc);
    rc = reqs[1].Wait();                           EXPECT_EQ(KELPIE_OK,rc);
    EXPECT_FALSE(reqs[1].result.put.already_existed);
    EXPECT_TRUE( reqs[1].result.put.wrote_local);

    //Check sptr count and make sure we have a copy and lkv has a copy
    num_sptr_owners = mem_sptr.use_count();
    EXPECT_EQ(2, num_sptr_owners);

    //Grab a copy. Do this just to make sure we have the right counts
    { //encapsulate the shared pointer so we limit how long we keep it
      shared_ptr<char> mem_sptr2;
      rc = r->GetLocal(k, mem_sptr2, &mem_size, &origin); EXPECT_EQ(KELPIE_OK,rc);
      if(rc==KELPIE_OK){
        int *tmp = reinterpret_cast<int *>(mem_sptr2.get());
        bad = ckData(tmp, num_words, tag);
        EXPECT_EQ(0, bad);
        //Check sptr count and make sure we have 3 copies: original, lkv, and get
        num_sptr_owners = mem_sptr2.use_count();
        EXPECT_EQ(3, num_sptr_owners);
      }
    }
    //Now should just be 2 copies: put and lkv
    num_sptr_owners = mem_sptr.use_count();
    EXPECT_EQ(2, num_sptr_owners);
  }


  { //encapsulate the shared pointer so we limit how long we keep it
    shared_ptr<char> mem_sptr;
    rc = r->GetLocal(k, mem_sptr, &mem_size, &origin); EXPECT_EQ(KELPIE_OK,rc);
    if(rc==KELPIE_OK){
      int *tmp = reinterpret_cast<int *>(mem_sptr.get());
      bad = ckData(tmp, num_words, tag);
      EXPECT_EQ(0, bad);
      //Check sptr count and make sure we have a copy and lkv has a copy
      int num_sptr_owners = mem_sptr.use_count();
      EXPECT_EQ(2, num_sptr_owners);
    }
  }
}




#if 0
int main(int argc, char **argv){

  ::testing::InitGoogleTest(&argc, argv);

  Kelpie kelpie;
  Configuration conf;

  conf.Append(default_config);
  conf.Append("node_role","client");
  kelpie.Init(conf);

  Resource *r = kelpie.Connect("local:");
  rc_t rc;

  Key k("howdy", "bob");
  Key k_missing("not","a key that exists");

  int words = 1024;
  int buf1_src[words];   mkData(buf1_src, words, 96);
  int buf1_dst[words];


  //Put data into the local k/v
  RequestHandle req1;
  rc = r->Put(k, buf1_src, sizeof(buf1_src), &req1); EXPECT_EQ(KELPIE_OK,rc);
  rc = req1.Wait();                                  EXPECT_EQ(KELPIE_OK,rc);

  //Get data and make sure its the same
  RequestHandle req2;
  rc = r->Get(k, buf1_dst, sizeof(buf1_dst), &req2); EXPECT_EQ(KELPIE_OK,rc);
  rc = req2.Wait();                                  EXPECT_EQ(KELPIE_OK,rc);
  int bad = ckData(buf1_dst, words, 96);
  assert(bad==0);

  //Get a shared pointer reference to the data and check it
  RequestHandle req3;
  size_t mem_size;
  nodeid_t origin;
  { //encapsulate the shared pointer so we limit how long we keep it
    shared_ptr<char> mem_sptr;
    rc = r->GetLocal(k, mem_sptr, &mem_size, &origin); EXPECT_EQ(KELPIE_OK,rc);
    if(rc==KELPIE_OK){
      int *tmp = reinterpret_cast<int *>(mem_sptr.get());
      bad = ckData(tmp, words, 96);
      assert(bad==0);
      //cout << mem_sptr.use_count()<<endl; //should be two: one for lkv, one for us
    }
  }

  //Get local version, using copy
  RequestHandle req4;
  size_t copied_size, available_size;
  memset(buf1_dst, 0, sizeof(buf1_dst));
  rc = r->GetLocal(k, buf1_dst, sizeof(buf1_dst), &copied_size, &available_size, &origin);
  cout <<"Copied size/Avail size="<<copied_size<<"/"<<available_size<<endl;
  EXPECT_EQ(KELPIE_OK,rc);
  assert(copied_size==available_size);
  bad = ckData(buf1_dst, words, 96);

  //Get half the data
  RequestHandle req5;
  memset(buf1_dst, 0, sizeof(buf1_dst));
  rc = r->GetLocal(k, buf1_dst, sizeof(buf1_dst)/2, &copied_size, &available_size, &origin);
  assert(copied_size==sizeof(buf1_dst)/2);
  assert(available_size==sizeof(buf1_dst));
  bad = ckData(buf1_dst, words/2, 96);
  bad += ckData(&buf1_dst[words/2], words/2, 0); //Should be zero
  assert(bad==0);

  //Get a key that doesn't exist
  RequestHandle req6;
  rc = r->GetLocal(k_missing, buf1_dst, sizeof(buf1_dst), &copied_size, &available_size, &origin);
  cout <<"Cs="<<copied_size << " avail "<<available_size<<endl;
  assert(copied_size==0);
  assert(available_size==0);
  assert(rc==KELPIE_ENOENT);


  cout << "Client ended\n";
  delete r;

}
#endif
