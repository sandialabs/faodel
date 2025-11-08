// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// This aggregates tests for basic structures including:
//
//    Bucket        : A hash of a string
//    NodeID        : Holds host/port for whookie
//    NameAndNode   : A string label and a node id
//    DirectoryInfo : Holds a list of name/node resource


#include <string>
#include "gtest/gtest.h"
#include "faodel-common/Common.hh"

using namespace std;
using namespace faodel;

class BucketTest : public testing::Test {
protected:
  void SetUp() override {
  }
  internal_use_only_t iuo;
};
TEST_F(BucketTest, SimpleByHand) {
  bucket_t b;
}

TEST_F(BucketTest, Sizes) {
  bucket_t b1;
  bucket_t b2[10];

  EXPECT_EQ(4, sizeof(b1));
  EXPECT_EQ(sizeof(uint32_t), sizeof(b1));
  EXPECT_EQ(sizeof(uint32_t)*10, sizeof(b2));
}

TEST_F(BucketTest, Compares) {
  bucket_t bs[10];
  for(int i=0; i<10; i++)
    bs[i] = bucket_t(i, iuo);

  for(int i=1; i<10; i++) {
    EXPECT_TRUE( bs[i].Valid() );
    EXPECT_FALSE( bs[i].Unspecified() );
    EXPECT_TRUE( bs[i-1].bid <  bs[i].bid );
    EXPECT_TRUE( bs[i-1].bid <= bs[i].bid );
    EXPECT_TRUE( bs[i].bid   >  bs[i-1].bid );
    EXPECT_TRUE( bs[i].bid   >= bs[i-1].bid );

    EXPECT_TRUE( bs[i-1].GetID() <  bs[i].bid );
    EXPECT_TRUE( bs[i-1].GetID() <= bs[i].bid );
    EXPECT_TRUE( bs[i].GetID()   >  bs[i-1].bid );
    EXPECT_TRUE( bs[i].GetID()   >= bs[i-1].bid );


    EXPECT_FALSE( bs[i].bid == bs[i-1].bid );
    EXPECT_TRUE( bs[i].bid  != bs[i-1].bid );
  }
}

TEST_F(BucketTest, Copies) {
  string name="This is the string";
  bucket_t bsrc(name);
  bucket_t bdst;

  //Shouldn't be equal
  EXPECT_NE(bsrc.bid,      bdst.bid);
  EXPECT_NE(bsrc.GetHex(), bdst.GetHex());
  EXPECT_NE(bsrc,          bdst);

  //Should be the same
  bdst = bsrc;
  EXPECT_EQ(bsrc.bid,      bdst.bid);
  EXPECT_EQ(bsrc.GetHex(), bdst.GetHex());
  EXPECT_EQ(bsrc,          bdst);
}

TEST_F(BucketTest, BadHexNode) {
  string s;

  //If we already computed the hash, we can pass it around
  //as a hex string. The string has to start with 0x and
  //contain less than 8 hex digits to be valid.
  string bad_vals[] = {   "0xf000000000000000",
                          "0x123456789",
                          "0x123G5678",
                          "0x1234567x",
                          "0x123 5678",
                          ""};

  for(int i=0; bad_vals[i]!=""; i++) {
    //cout <<"Trying "<<bad_vals[i]<<endl;
    EXPECT_ANY_THROW( bucket_t b(bad_vals[i]) );
  }

}



class NodeIDTest : public testing::Test {
protected:
  void SetUp() override {
  }

};
TEST_F(NodeIDTest, SimpleByHand) {

  nodeid_t nid("10.1.2.3","1999");
  string ip   = nid.GetIP();                         EXPECT_EQ("10.1.2.3", ip);
  string port = nid.GetPort();                       EXPECT_EQ("1999", port);
  string http1 = nid.GetHttpLink();                  EXPECT_EQ("http://10.1.2.3:1999",      http1);
  string http2 = nid.GetHttpLink("do/it");           EXPECT_EQ("http://10.1.2.3:1999/do/it",http2);
  string http3 = nid.GetHttpLink("/do/it");          EXPECT_EQ("http://10.1.2.3:1999/do/it",http3);
  string html1  = nid.GetHtmlLink("/reset","BOZO");  EXPECT_EQ("<a href=\"http://10.1.2.3:1999/reset\">BOZO</a>\n", html1);

  string sip,sport;
  nid.GetIPPort(&sip,&sport); EXPECT_EQ("10.1.2.3", sip); EXPECT_EQ("1999", sport);

  uint32_t bip;
  uint16_t bport;
  nid.GetIPPort(&bip,&bport);
  EXPECT_EQ( ((10<<24)|(1<<16)|(2<<8)|3), bip);
  EXPECT_EQ(1999, bport);


}
TEST_F(NodeIDTest, IPByteOrder) {
  nodeid_t nid("1.2.3.4","1800"); //1800 = 0x0708

  string s = nid.GetHex();
  //cout <<s<<endl;
  EXPECT_EQ("0x70801020304", s);

  uint32_t ip;
  uint16_t port;
  nid.GetIPPort(&ip,&port);

  //cout <<hex<<ip<<"  "<<port<<endl;
  EXPECT_EQ(0x01020304, ip);
  EXPECT_EQ(0x0708,     port);
}
TEST_F(NodeIDTest, BinaryCtor) {
  nodeid_t nid(0x01020304, 0x0506);
  uint32_t ip;
  uint16_t port;
  nid.GetIPPort(&ip,&port);
  EXPECT_EQ(0x01020304, ip);
  EXPECT_EQ(0x0506,     port);

}


TEST_F(NodeIDTest, Sizes) {
  nodeid_t nid1;
  nodeid_t nid2[10];


  EXPECT_EQ(8, sizeof(nid1));
  EXPECT_EQ(sizeof(uint64_t), sizeof(nid1));
  EXPECT_EQ(sizeof(uint64_t)*10, sizeof(nid2));
}

TEST_F(NodeIDTest, Compares) {
  nodeid_t nids[10];
  for(int i=0; i<10; i++)
    nids[i].nid = i;

  for(int i=1; i<10; i++) {
    EXPECT_TRUE( nids[i-1].nid <  nids[i].nid );
    EXPECT_TRUE( nids[i-1].nid <= nids[i].nid );
    EXPECT_TRUE( nids[i].nid   >  nids[i-1].nid );
    EXPECT_TRUE( nids[i].nid   >= nids[i-1].nid );

    EXPECT_FALSE( nids[i].nid == nids[i-1].nid );
    EXPECT_TRUE( nids[i].nid != nids[i-1].nid );

  }
}

TEST_F(NodeIDTest, Copies) {
  nodeid_t nsrc("10.0.0.101","2010");
  nodeid_t ndst;

  //Shouldn't be equal
  EXPECT_NE(nsrc.nid,      ndst.nid);
  EXPECT_NE(nsrc.GetHex(), ndst.GetHex());
  EXPECT_NE(nsrc,          ndst);


  ndst = nsrc;
  EXPECT_EQ(nsrc.nid,      ndst.nid);
  EXPECT_EQ(nsrc.GetHex(), ndst.GetHex());
  EXPECT_EQ(nsrc,          ndst);
}

TEST_F(NodeIDTest, BadUrls) {

  string urls[] = {
    //We used to encode transport here. Try formerly valid refs to make sure they throw errors
    "ib://10.1.1.1",
    "ib://10.1.1.1:8080",
    "mpi://1",

    //Old malformed urls
    "i://19.12.12.12:1111", //bad net
    "://19.2.2.2:234",      //no net
    "ib//cnn.com:120",      //no colon
    "ib://1.2.3.4",         //no port
    "ib://1.2.3.4:65536",   //Port only 16b or <64K
    "mpi://1.2.3.4.5:10",   //Long hostname
    "mpi://:10",            //no ip
    "mpi://1:10",           //bad ip
    "ib:/10.10.10.10:100",  //bad separator
    ""
  };

  for(int i=0; urls[i] != ""; i++) {
    //cout << "Url: "<<urls[i]<<endl;
    try{
      nodeid_t node_id(urls[i]);
      cout <<node_id.GetHex()<<endl;
      EXPECT_TRUE(false);
    } catch (exception &e) {
      EXPECT_TRUE(true);
    }
  }

}


TEST_F(NodeIDTest, BadHexNode) {
  uint64_t x;

  //Previously had a check here for transport valid. Not and issue now.
  EXPECT_NO_THROW( nodeid_t nid("0xf000000000000000") );

}

TEST_F(NodeIDTest, NullCtor) {

  //Users may mistakenly set the value of this node to an initial
  //value. Non-zero numbers will get caught at compile time. A
  //0 though looks like a nullptr. We catch them and throw an
  //exception since it's always (?) a mistake

  EXPECT_ANY_THROW( nodeid_t nid(0) );
}

class NameAndNodeTest : public testing::Test {
protected:
  void SetUp() override {
  }
};

TEST_F(NameAndNodeTest, SimpleByHand) {

  NameAndNode a10("a", NodeID(0x01, internal_use_only));
  NameAndNode b10("b", NodeID(0x02, internal_use_only));
  NameAndNode a11("a", NodeID(0x03, internal_use_only));
  EXPECT_TRUE(  (a10<b10));
  EXPECT_TRUE(  (a11<b10));
  EXPECT_FALSE( (a10<a11));
  EXPECT_FALSE( (a10==b10));
  EXPECT_TRUE(  (a10==a11));
  EXPECT_EQ( "a", a10.name);
  EXPECT_EQ( "a", a11.name);
  EXPECT_EQ( "b", b10.name);
  EXPECT_EQ( "0x1", a10.node.GetHex());
  EXPECT_EQ( "0x2", b10.node.GetHex());
  EXPECT_EQ( "0x3", a11.node.GetHex());
}


class DirectoryInfoTest : public testing::Test {
protected:
  void SetUp() override {
  }

};
TEST_F(DirectoryInfoTest, SimpleByHand) {

  DirectoryInfo di1("ref:/my/thing&num=2&ag0=0x19900001&ag1=0x19900002&info=silly%20stuff");
  
  EXPECT_FALSE(di1.IsEmpty());
  EXPECT_EQ(2, di1.members.size());
  if(di1.members.size()==2){
    EXPECT_EQ("ag0", di1.members[0].name);
    EXPECT_EQ("ag1", di1.members[1].name);
    EXPECT_EQ("0x19900001", di1.members[0].node.GetHex());
    EXPECT_EQ("0x19900002", di1.members[1].node.GetHex());
  }
  EXPECT_EQ("silly stuff", di1.info);

  EXPECT_TRUE(di1.ContainsNode(nodeid_t("0x19900001")));
  EXPECT_TRUE(di1.ContainsNode(nodeid_t("0x19900002")));
  EXPECT_FALSE(di1.ContainsNode(nodeid_t("0x19900003")));
  EXPECT_FALSE(di1.ContainsNode(nodeid_t("0x19900000")));

  ResourceURL url2("thing1:/my/stuff");
  for(int i=0; i<10; i++){
    string name="ag"+to_string(i);
    url2.SetOption(name, "0x1990000"+to_string(i));
  }
  url2.SetOption("num",to_string(10));
  url2.SetOption("info",MakePunycode("rain is in the plains"));

  DirectoryInfo di2(url2);
  EXPECT_FALSE(di2.IsEmpty());
  EXPECT_EQ(10, di2.members.size());
  if(di2.members.size() == 10) {
    for(int i=0; i<10; i++) {
      string s_node = "0x1990000"+to_string(i);
      string s_name = "ag"+to_string(i);
      EXPECT_TRUE(di2.ContainsNode(nodeid_t(s_node)));
      EXPECT_EQ(s_name, di2.members[i].name);
      EXPECT_EQ(nodeid_t(s_node), di2.members[i].node);
    }
  }
  EXPECT_EQ("rain is in the plains", di2.info);
  
  DirectoryInfo di3; //Empty
  EXPECT_TRUE(di3.IsEmpty());
}

