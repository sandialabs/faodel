// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include "gtest/gtest.h"
#include "common/Common.hh"

using namespace std;
using namespace faodel;

class NodeIDTest : public testing::Test {
protected:
  virtual void SetUp() {
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
    } catch (exception e) {
      EXPECT_TRUE(true);
    }
  }

}


TEST_F(NodeIDTest, BadHexNode) {
  uint64_t x;
  string s;

  //Previously had a check here for transport valid. Not and issue now.
  try {
    nodeid_t nid("0xf000000000000000");
    //cout <<nid.GetHex()<<endl;
    EXPECT_TRUE(true); //Should be a problem
  } catch(exception e) {
    EXPECT_TRUE(true);
  }

}

TEST_F(NodeIDTest, NullCtor) {

  //Users may mistakenly set the value of this node to an initial
  //value. Non-zero numbers will get caught at compile time. A
  //0 though looks like a nullptr. We catch them and throw an
  //exception since it's always (?) a mistake

  try{
    nodeid_t nid(0);
    EXPECT_TRUE(false); //Should not get here
  } catch(exception e) {
    EXPECT_TRUE(true);
  }



}
