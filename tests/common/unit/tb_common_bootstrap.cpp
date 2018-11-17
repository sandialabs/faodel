// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "gtest/gtest.h"


#include "common/Common.hh"
#include "common/BootstrapInternal.hh"


using namespace std;
using namespace faodel;

bool enable_debug=false;


class FaodelBootstrap : public testing::Test {
protected:
  virtual void SetUp() {
    bs = new faodel::bootstrap::internal::Bootstrap();
    conf = Configuration("node_role server");
    if(enable_debug) {
      conf.Append("bootstrap.debug","true");
    }
    fn_init_nop = [](Configuration conf) {};
    fn_start_nop = []() {};
    fn_fini_nop = []() {};

  }
  virtual void TearDown() {
    delete bs;
  }
  faodel::bootstrap::internal::Bootstrap *bs;
  Configuration conf;

  faodel::bootstrap::fn_init  fn_init_nop;
  faodel::bootstrap::fn_start fn_start_nop;
  faodel::bootstrap::fn_fini  fn_fini_nop;

};

TEST_F(FaodelBootstrap, simple) {
  int setval=2112;
  bs->RegisterComponent("a", {}, {},
                        [&setval](Configuration conf) {
                                     EXPECT_EQ(2112,setval); setval=3113; },
                        [&setval]() { EXPECT_EQ(2113,setval); setval=3114; },
                        [&setval]() { EXPECT_EQ(2114,setval); setval=3115; },
                        true);

  //Shouldn't be set yet
  EXPECT_EQ(setval,2112);

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a"};
  EXPECT_EQ(names_exp, names);
  EXPECT_EQ(1,names.size());

  //Do the init,
  EXPECT_EQ(setval, 2112);  bs->Init(conf);   EXPECT_EQ(setval,3113);

  //Do the start
  setval=2113;              bs->Start();      EXPECT_EQ(setval,3114);

  //Do the finish
  setval=2114;              bs->Finish(true); EXPECT_EQ(setval,3115);


}
TEST_F(FaodelBootstrap, simpleCombined) {
  int setval=2112;
  bs->RegisterComponent("a", {}, {},
                        [&setval](Configuration conf) {
                                     EXPECT_EQ(2112,setval); setval=9999; },
                        [&setval]() { EXPECT_EQ(9999,setval); setval=3114; },
                        [&setval]() { EXPECT_EQ(2114,setval); setval=3115; },
                        true);

  //Shouldn't be set yet
  EXPECT_EQ(setval,2112);

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a"};
  EXPECT_EQ(names_exp, names);
  EXPECT_EQ(1,names.size());

  //Do the init,
  EXPECT_EQ(setval, 2112);  bs->Start(conf);  EXPECT_EQ(setval,3114);

  //Do the finish
  setval=2114;              bs->Finish(true); EXPECT_EQ(setval,3115);

}
TEST_F(FaodelBootstrap, multiple) {
  char val='X';
  bs->RegisterComponent("a", {}, {},
                        [&val](Configuration conf) { EXPECT_EQ('X',val); val='a'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('a',val); val='X'; },
                        true);
  bs->RegisterComponent("b", {"a"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('a',val); val='b'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('b',val); val='a'; },
                        true);
  bs->RegisterComponent("c", {"b"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('b',val); val='c'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('c',val); val='b'; },
                        true);
  bs->RegisterComponent("d", {"c"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('c',val); val='d'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('d',val); val='c'; },
                        true );


  //Shouldn't be set yet
  EXPECT_EQ(val,'X');

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a","b","c","d"};
  EXPECT_EQ(names_exp, names);
  EXPECT_EQ(4,names.size());

  //Do the startup
  bs->Start(conf);

  //Init should set
  EXPECT_EQ('d', val);

  bs->Finish(true);
  EXPECT_EQ('X', val);

}


TEST_F(FaodelBootstrap, multipleReverse) {
  char val='X';
  bs->RegisterComponent("d", {"c"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('c',val); val='d'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('d',val); val='c'; },
                        true);
  bs->RegisterComponent("c", {"b"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('b',val); val='c'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('c',val); val='b'; },
                        true);
  bs->RegisterComponent("b", {"a"}, {},
                        [&val](Configuration conf) { EXPECT_EQ('a',val); val='b'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('b',val); val='a'; },
                        true);
  bs->RegisterComponent("a", {}, {},
                        [&val](Configuration conf) { EXPECT_EQ('X',val); val='a'; },
                        fn_start_nop,
                        [&val]() {                   EXPECT_EQ('a',val); val='X'; },
                        true );

  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);


  //Shouldn't be set yet
  EXPECT_EQ(val,'X');

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a","b","c","d"};
  EXPECT_EQ(names_exp, names);
  EXPECT_EQ(4,names.size());

  //Do the startup
  bs->Start(conf);

  //Init should set
  EXPECT_EQ('d', val);

  bs->Finish(true);
  EXPECT_EQ('X', val);

}


TEST_F(FaodelBootstrap, multiDep) {

  bs->RegisterComponent("d", {"c"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("a", {}, {}, fn_init_nop, fn_start_nop,fn_fini_nop, true);
  bs->RegisterComponent("b", {"a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("c", {"b","a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("e", {"d"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("f", {"a","e","a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);

  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a","b","c","d","e","f"};
  EXPECT_EQ(names_exp, names);

  //Do the startup
  bs->Init(conf);
  bs->Start();
  bs->Finish(true);

}


TEST_F(FaodelBootstrap, multiDepIgnoredOptionals) {

  bs->RegisterComponent("a", {}, {"Gadzooks","Shazooks"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("b", {"a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("f", {"a","e","a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("c", {"b","a"}, {"Bingo","Mingo"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("d", {"c"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("e", {"d"}, {"noteye"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);

  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"a","b","c","d","e","f"};
  EXPECT_EQ(names_exp, names);

  //Do the startup
  bs->Init(conf);
  bs->Finish(true);

}

TEST_F(FaodelBootstrap, multiDepOptionals) {

  bs->RegisterComponent("a", {}, {"Gadzooks","Shazooks"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("b", {"a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("Gadzooks", {}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("f", {"a","e","a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("c", {"b","a"}, {"Bingo","Mingo"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("d", {"c"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("e", {"d"}, {"noteye"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);

  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  //Verify list of names
  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"Gadzooks","a","b","c","d","e","f"};
  EXPECT_EQ(names_exp, names);

  //Do the startup
  bs->Init(conf);
  bs->Finish(true);

}


TEST_F(FaodelBootstrap, missingDep) {

  bs->RegisterComponent("a", {}, {"Gadzooks","Shazooks"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("b", {"a"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("Gadzooks", {}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("d", {"c"}, {}, fn_init_nop, fn_start_nop, fn_fini_nop, true);
  bs->RegisterComponent("e", {"d"}, {"noteye"}, fn_init_nop, fn_start_nop, fn_fini_nop, true);

  //Missing c
  bool ok = bs->CheckDependencies();  EXPECT_FALSE(ok);

}


class A : public bootstrap::BootstrapInterface {
public:
  int state;
  faodel::bootstrap::internal::Bootstrap *bs;

  A(faodel::bootstrap::internal::Bootstrap *bs) : state(0),bs(bs) {}
  void Init(const Configuration &config) {
    EXPECT_EQ(0, state);
    state=1;
  };
  void Start() {
    EXPECT_EQ(1, state);
    state=2;
  }
  void Finish() {
    EXPECT_EQ(2, state);
    state=3;
  }
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const {
    EXPECT_EQ(0, state);
    name="A";
    //No deps
  }
  int GetState() {
    return state;
  }
};

class B : public bootstrap::BootstrapInterface {
public:
  int state;
  faodel::bootstrap::internal::Bootstrap *bs;

  B(faodel::bootstrap::internal::Bootstrap *bs) : state(0), bs(bs) {}
  void Init(const Configuration &config) {
    EXPECT_EQ(0, state);

    //Get a pointer to A
    BootstrapInterface *bsc = bs->GetComponentPointer("A");
    EXPECT_NE(nullptr, bsc);
    if(bsc!=nullptr) {
      A *aptr = static_cast<A *>(bsc);
      int tmp = aptr->GetState();
      EXPECT_EQ(1,tmp); //A should be init'd
    }

    state=1;
  };
  void Start() {
    EXPECT_EQ(1, state);
    state=2;
  }
  void Finish() {
    EXPECT_EQ(2, state);
    state=3;
  }
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const {
    EXPECT_EQ(0, state);
    name="B";
    requires={"A"};
  }
};


string default_config_string = R"EOF(
config.additional_files.env_name.if_defined   FAODEL_CONFIG
)EOF";

TEST_F(FaodelBootstrap, simpleClassInterfaces) {


  //A a; B b;
  B *b = new B(bs);
  A *a = new A(bs);

  bs->RegisterComponent(a,true); //static_cast<bootstrap::BootstrapInterface *>(&a));
  bs->RegisterComponent(b,true);
  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"A","B"};
  EXPECT_EQ(names_exp, names);

  Configuration config(default_config_string);
  config.AppendFromReferences();

  bs->Start(config);
  bs->Finish(true);
  EXPECT_EQ(3, a->state);
  //EXPECT_EQ(3, b->state);

}
