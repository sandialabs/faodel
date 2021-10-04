// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <mpi.h>

#include <gtest/gtest.h>


#include "faodel-common/Common.hh"
#include "faodel-common/BootstrapImplementation.hh"


using namespace std;
using namespace faodel;

bool enable_debug=false;


class FaodelBootstrap : public testing::Test {
protected:
  void SetUp() override {
    bs = new faodel::bootstrap::internal::Bootstrap();
    conf = Configuration("node_role server");
    if(enable_debug) {
      conf.Append("bootstrap.debug","true");
    }
    fn_init_nop = [](Configuration *conf) {};
    fn_start_nop = []() {};
    fn_fini_nop = []() {};

  }

  void TearDown() override {
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
                        [&setval](Configuration *conf) {
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
                        [&setval](Configuration *conf) {
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
                        [&val](Configuration *conf) { EXPECT_EQ('X',val); val='a'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('a',val); val='X'; },
                        true);
  bs->RegisterComponent("b", {"a"}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('a',val); val='b'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('b',val); val='a'; },
                        true);
  bs->RegisterComponent("c", {"b"}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('b',val); val='c'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('c',val); val='b'; },
                        true);
  bs->RegisterComponent("d", {"c"}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('c',val); val='d'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('d',val); val='c'; },
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
                        [&val](Configuration *conf) { EXPECT_EQ('c',val); val='d'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('d',val); val='c'; },
                        true);
  bs->RegisterComponent("c", {"b"}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('b',val); val='c'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('c',val); val='b'; },
                        true);
  bs->RegisterComponent("b", {"a"}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('a',val); val='b'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('b',val); val='a'; },
                        true);
  bs->RegisterComponent("a", {}, {},
                        [&val](Configuration *conf) { EXPECT_EQ('X',val); val='a'; },
                        fn_start_nop,
                        [&val]() {                    EXPECT_EQ('a',val); val='X'; },
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


class A
  : public bootstrap::BootstrapInterface,
    public LoggingInterface {
public:
  int state;
  faodel::bootstrap::internal::Bootstrap *bs;

  A(faodel::bootstrap::internal::Bootstrap *bs) : LoggingInterface("A"), state(0), bs(bs) {}
  void Init(const Configuration &config) override {
    ConfigureLogging(config);
    dbg("Init");
    EXPECT_EQ(0, state);
    state=1;
  };
  void Start() override {
    dbg("Start");
    EXPECT_EQ(1, state);
    state=2;
  }
  void Finish() override {
    dbg("Finish");
    EXPECT_EQ(2, state);
    state=3;
  }
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const override {
    EXPECT_EQ(0, state);
    name="A";
    //No deps
  }
  int GetState() {
    return state;
  }
};

class B
  : public bootstrap::BootstrapInterface,
    public LoggingInterface {
public:
  int state;
  int num_times_init;
  int num_times_finish;
  faodel::bootstrap::internal::Bootstrap *bs;

  B(faodel::bootstrap::internal::Bootstrap *bs)
  : LoggingInterface("B"), state(0), num_times_init(0), num_times_finish(0), bs(bs) {}
  void Init(const Configuration &config) override {
    ConfigureLogging(config);
    num_times_init++;
    EXPECT_EQ(0, state);

    //Get a pointer to A
    BootstrapInterface *bsc = bs->GetComponentPointer("A");
    EXPECT_NE(nullptr, bsc);
    if(bsc!=nullptr) {
      dbg("Init got valid a pointer");
      A *aptr = static_cast<A *>(bsc);
      int tmp = aptr->GetState();
      EXPECT_EQ(1,tmp); //A should be init'd
    }

    state=1;
  };
  void Start() override {
    EXPECT_EQ(1, state);
    state=2;
  }
  void Finish() override {
    num_times_finish++;
    EXPECT_EQ(2, state);
    state=3;
  }
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const override {
    EXPECT_EQ(0, state);
    name="B";
    requires={"A"};
  }
};



TEST_F(FaodelBootstrap, simpleClassInterfaces) {

  B *b = new B(bs);
  A *a = new A(bs);

  bs->RegisterComponent(a,true); //static_cast<bootstrap::BootstrapInterface *>(&a));
  bs->RegisterComponent(b,true);
  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"A","B"};
  EXPECT_EQ(names_exp, names);

  Configuration config(""); //("bootstrap.debug true\nA.debug true\nB.debug true");
  config.AppendFromReferences();

  bs->Start(config);
  bs->Finish(true);
  EXPECT_EQ(3, a->state);

  delete b;
  delete a;
}

TEST_F(FaodelBootstrap, allowMultipleStarts) {
  A *a = new A(bs);
  B *b = new B(bs);

  bs->RegisterComponent(a,true); //static_cast<bootstrap::BootstrapInterface *>(&a));
  bs->RegisterComponent(b,true);

  Configuration config("bootstrap.debug true"); //("bootstrap.debug true\nA.debug true\nB.debug true");
  config.AppendFromReferences();

  bs->Start(config);  EXPECT_EQ(1, b->num_times_init); EXPECT_EQ(0, b->num_times_finish);
  bs->Start(config);  EXPECT_EQ(1, b->num_times_init); EXPECT_EQ(0, b->num_times_finish);
  EXPECT_EQ(2, bs->GetNumberOfUsers());
  bs->Finish(true);   EXPECT_EQ(1, b->num_times_init); EXPECT_EQ(0, b->num_times_finish);
  bs->Finish(true);   EXPECT_EQ(1, b->num_times_init); EXPECT_EQ(1, b->num_times_finish);

}


//==============================================================================
// Modify Configuration tests: verify a bootstrap can modify the config
//==============================================================================
class BSMod : public bootstrap::BootstrapInterface {
public:

  void Init(const Configuration &config) override {
    throw std::runtime_error("Shouldn't have called init");
  }
  void InitAndModifyConfiguration(Configuration *config) override {
    //cout <<"Doing IAM\n";
    string s;
    config->GetString(&s, "my.bogus.entry","");
    EXPECT_EQ("", s);
    config->Append("my.bogus.entry","is_now_set");
  }
  void Start() override {}
  void Finish() override {}
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const override {
    name="bsmod";
    //No deps
  }

};

class BSNoMod : public bootstrap::BootstrapInterface {
public:

  void Init(const Configuration &config) override {
    config.GetString(&val, "my.bogus.entry","");
    EXPECT_EQ("is_now_set", val);
  }
  void Start() override {}
  void Finish() override {}
  void GetBootstrapDependencies(string &name,
                                vector<std::string> &requires,
                                vector<std::string> &optional) const override {
    name="bsnomod";
    requires = { "bsmod"};
  }

  string GetVal() { return val; }
private:
  string val;
};

TEST_F(FaodelBootstrap, modifyConfiguration) {



  BSMod *b = new BSMod();
  BSNoMod *a = new BSNoMod();

  bs->RegisterComponent(a,true); //static_cast<bootstrap::BootstrapInterface *>(&a));
  bs->RegisterComponent(b,true);
  bool ok = bs->CheckDependencies();  EXPECT_TRUE(ok);

  auto names = bs->GetStartupOrder();
  vector<string> names_exp = {"bsmod","bsnomod"};
  EXPECT_EQ(names_exp, names);

  Configuration config("");
  config.AppendFromReferences();

  bs->Start(config);
  bs->Finish(true);

  EXPECT_EQ("is_now_set", a->GetVal());


}

//This test verifies that bootstrap will automatically load in config info based on FAODEL_CONFIG env var
TEST_F(FaodelBootstrap, autoUpdateConfig) {

  string config1 = R"EOF(
version.number   1
config1.specific.info  v1
default.iom.path /this/is/path1
)EOF";

  string config2 = R"EOF(
version.number   2
config2.specific.info  v2
default.iom.path /this/is/path2
)EOF";


  //Write out the second config to a file
  char fname[] = "/tmp/mytestXXXXXX";
  int fd = mkstemp(fname);
  //cout <<"Name is "<<fname<<endl;
  auto wlen = write(fd, config2.c_str(), config2.size());
  close(fd);

  Configuration t1(config1);

  //Config1 should only have stuff from config1 in it
  string version, path, conf_spec1, conf_spec2;
  t1.GetString(&version,    "version.number");
  t1.GetString(&path,       "default.iom.path");
  t1.GetString(&conf_spec1, "config1.specific.info");
  t1.GetString(&conf_spec2, "config2.specific.info");
  EXPECT_EQ("/this/is/path1", path);
  EXPECT_EQ("1", version );
  EXPECT_EQ("v1", conf_spec1);
  EXPECT_EQ("",   conf_spec2);

  //Now load in a second file. It should overwrite default.iom,path and add update
  t1.AppendFromFile(fname);
  t1.GetString(&version,    "version.number");
  t1.GetString(&path,       "default.iom.path");
  t1.GetString(&conf_spec1, "config1.specific.info");
  t1.GetString(&conf_spec2, "config2.specific.info");
  EXPECT_EQ("/this/is/path2", path);
  EXPECT_EQ("2", version );
  EXPECT_EQ("v1", conf_spec1);
  EXPECT_EQ("v2", conf_spec2);


  //Now try loading from env var
  Configuration t2(config1);

  auto fn_init_nop = [](Configuration *conf) { cout <<"Starting \n"<<conf->str();};
  auto fn_start_nop = []() {};
  auto fn_fini_nop = []() {};

  //Init function checks for config1
  auto fn_init_expect_c1 = [] (Configuration *conf) {
      //cout << "Checking for c1: "<<conf->str()<<endl;
      string version, path, conf_spec1, conf_spec2;
      conf->GetString(&version,    "version.number");
      conf->GetString(&path,       "default.iom.path");
      conf->GetString(&conf_spec1, "config1.specific.info");
      conf->GetString(&conf_spec2, "config2.specific.info");
      EXPECT_EQ("/this/is/path1", path);
      EXPECT_EQ("1", version );
      EXPECT_EQ("v1", conf_spec1);
      EXPECT_EQ("",   conf_spec2);
  };

  //Init function checks for config1 + config2
  auto fn_init_expect_merged = [] (Configuration *conf) {
      //cout << "Checking for c2: "<<conf->str()<<endl;
      string version, path, conf_spec1, conf_spec2;
      conf->GetString(&version,    "version.number");
      conf->GetString(&path,       "default.iom.path");
      conf->GetString(&conf_spec1, "config1.specific.info");
      conf->GetString(&conf_spec2, "config2.specific.info");
      EXPECT_EQ("/this/is/path2", path);
      EXPECT_EQ("2", version );
      EXPECT_EQ("v1", conf_spec1);
      EXPECT_EQ("v2", conf_spec2);
  };

  //Clear out FAODEL_CONFIG to make sure we don't load additional test
  unsetenv("FAODEL_CONFIG");
  bootstrap::RegisterComponent("tmp", {}, {}, fn_init_expect_c1, fn_start_nop, fn_fini_nop, true);
  bootstrap::Start(t2, [](){return "tmp";} );
  bootstrap::Finish();

  //Point FAODEL_CONFIG to config2, so bootstrap will merge them
  int rc = setenv("FAODEL_CONFIG", fname, 1);
  bootstrap::RegisterComponent("tmp", {}, {}, fn_init_expect_merged, fn_start_nop, fn_fini_nop, true);
  bootstrap::Start(t2, [](){return "tmp";} );
  bootstrap::Finish();

  unlink(fname);

}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if(mpi_rank==0) cout <<"Beginning tests.\n";
  int rc = RUN_ALL_TESTS();

  MPI_Finalize();
  if(mpi_rank==0) cout <<"All complete. Exiting.\n";
  return rc;
}
