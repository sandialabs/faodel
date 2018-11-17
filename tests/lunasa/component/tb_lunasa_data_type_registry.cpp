// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "gtest/gtest.h"
//#include <mpi.h>

#include <stdexcept>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"
#include "lunasa/common/DataObjectTypeRegistry.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

//Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
string default_config = R"EOF(

#lkv settings for the server
server.max_capacity 32M
server.mutex_type   rwlock

node_role server
)EOF";

class LunasaTypeRegistry : public testing::Test {
protected:
  void SetUp() override {
    Configuration config(default_config);
    config.AppendFromReferences();

    bootstrap::Init(config, lunasa::bootstrap);
    bootstrap::Start();
  }

  void TearDown() override {
    bootstrap::Finish();
  }
};

struct MyThing {
  uint32_t num_things;
  uint32_t thing1;
  uint64_t thing2;
  uint8_t lens[32*1024];
  uint8_t payload[0];
};

struct MyFloat {
  char myname[256];
  int num_items;
  float starting_value;
  float x[128];
  float y[128];
  float z[128];

  void Init(string name, float start_val, int items) {
    if((name.size()>255) || (items>128)) {
      throw std::runtime_error("Init data exceeded MyFloat Capacity");
    }
    memset(myname, 0, 256);
    memcpy(&myname[0], name.c_str(), name.size());
    num_items = items;
    starting_value = start_val;
    for(int i = 0; i<items; i++) {
      x[i] = start_val + ((float) i) + 0.1;
      y[i] = start_val + ((float) i) + 0.2;
      z[i] = start_val + ((float) i) + 0.3;
    }
  }
};

const string myfloat_name = "MyFloat";
const dataobject_type_t myfloat_tag = const_hash16("MyFloat");

void fn_dumpMyFloat(const DataObject &ldo, faodel::ReplyStream &rs) {
  auto *mf = ldo.GetMetaPtr<MyFloat *>();

  vector<pair<string, string>> vals = {
          {"Name",  string(mf->myname)},
          {"Items", to_string(mf->num_items)},
          {"Start", to_string(mf->starting_value)}
  };
  rs.mkTable(vals, "MyFloat LDO Metadata", true);

  rs.tableBegin("MyFloat LDO Data");
  rs.tableTop({"id", "X", "Y", "Z"});
  for(int i = 0; i<mf->num_items; i++) {
    rs.tableRow({to_string(i), to_string(mf->x[i]), to_string(mf->y[i]), to_string(mf->z[i])});
  }
  rs.tableEnd();
}

string ldo1_msg = R"EOF(
MyFloat LDO Metadata
Name	first guy
Items	10
Start	0.000000
MyFloat LDO Data
id	X	Y	Z
0	0.100000	0.200000	0.300000
1	1.100000	1.200000	1.300000
2	2.100000	2.200000	2.300000
3	3.100000	3.200000	3.300000
4	4.100000	4.200000	4.300000
5	5.100000	5.200000	5.300000
6	6.100000	6.200000	6.300000
7	7.100000	7.200000	7.300000
8	8.100000	8.200000	8.300000
9	9.100000	9.200000	9.300000
)EOF";

TEST_F(LunasaTypeRegistry, simpleSetups) {

  bool found;
  DataObjectTypeRegistry dotr;
  dotr.RegisterDataObjectType(myfloat_tag, myfloat_name, fn_dumpMyFloat);

  //Double registration should cause an exception
  EXPECT_ANY_THROW(dotr.RegisterDataObjectType(myfloat_tag, myfloat_name, fn_dumpMyFloat));


  //Create an LDO
  DataObject ldo1(sizeof(MyFloat), 0, DataObject::AllocatorType::eager);
  ldo1.SetTypeID(myfloat_tag);

  auto myfloat1 = ldo1.GetMetaPtr<MyFloat *>();
  myfloat1->Init("first guy", 0.0, 10);

  stringstream ss;
  faodel::ReplyStream rs(faodel::ReplyStreamType::TEXT, "test", &ss);
  found = dotr.DumpDataObject(ldo1, rs);
  EXPECT_TRUE(found);
  rs.Finish();

  //Parse through both, token by token (not line by line). This fixes tabs vs spaces
  stringstream ess(ldo1_msg);
  string expected, s;
  while(ess >> expected) {
    ss >> s;
    //cout<<"'"<<expected<<"' '"<<s<<"'\n";
    EXPECT_EQ(expected, s);
  }
  //cout <<ss.str()<<endl;

  cout << "Registry items are :" << dotr.str(100) << endl;

}

