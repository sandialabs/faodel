// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <string>
#include <vector>
#include <sstream>
#include "gtest/gtest.h"


#include "faodel-common/Common.hh"


using namespace std;
using namespace faodel;


extern char **environ;

class FaodelConfiguration : public testing::Test {
protected:
  void SetUp() override {
    //Wipe env?
    char **env; int i;
    for(env = environ; *env !=0; env++) {
      char *src = *env;
      for(i=0; !((src[i]==0)||(src[i]=='=')); i++) ;
      if(src[i]=='=') {
        char *name=strdup(src);
        name[i]='\0';
        //printf("Killing %s\n",name);
        unsetenv(name);
        free(name);
      }
    }
  }

};

//Test to make sure config can be passed around
string ConstConfig(const Configuration &config){
  return config.GetRole();
}
TEST_F(FaodelConfiguration, constConfig){
  Configuration c0;
  Configuration c1("node_role dummy");
  Configuration c2("node_role dummy", "MY_ENV_VAR");

  EXPECT_EQ("default", ConstConfig(c0));
  EXPECT_EQ("dummy",   ConstConfig(c1));
  EXPECT_EQ("dummy",   ConstConfig(c2));

  string s;
  s=""; c0.GetString(&s, "config.additional_files.env_name.if_defined"); EXPECT_EQ("FAODEL_CONFIG", s);
  s=""; c1.GetString(&s, "config.additional_files.env_name.if_defined"); EXPECT_EQ("FAODEL_CONFIG", s);
  s=""; c2.GetString(&s, "config.additional_files.env_name.if_defined"); EXPECT_EQ("MY_ENV_VAR", s);

}


TEST_F(FaodelConfiguration, appendString) {

  Configuration c;

  c.Append("myobject2 dummy");
  c.Append("myobject2 goodval");

  rc_t rc;
  string s;
  rc = c.GetString(&s, "myobject2", "xxxx");
  EXPECT_EQ("goodval", s);
  EXPECT_EQ(0, rc);

  //Overwrite with a Cap name
  c.Append("MyObject2 nextval");
  rc = c.GetString(&s, "myobject2", "xxxx");
  EXPECT_EQ("nextval", s);
  EXPECT_EQ(0,rc);

  rc = c.GetString(&s, "MyOBJECT2", "xxxx");
  EXPECT_EQ("nextval", s);
  EXPECT_EQ(0,rc);

  //Try appending when value doesn't exist
  s="";
  rc = c.GetString(&s, "nothere");
  EXPECT_EQ(ENOENT,rc);
  EXPECT_EQ("",s);
  rc = c.AppendIfUnset("nothere", "set-by-first-aiu");
  rc = c.GetString(&s, "nothere");
  EXPECT_EQ(0,rc);
  EXPECT_EQ("set-by-first-aiu", s);



  //Try conditional appending when value does exist
  rc = c.AppendIfUnset("nothere", "should-not-overwrite");
  rc = c.GetString(&s, "nothere");
  EXPECT_EQ(0, rc);
  EXPECT_EQ("set-by-first-aiu", s);


}

TEST_F(FaodelConfiguration, tabs_vs_spaces) {
  Configuration c;
  c.Append("thing1 value1");
  c.Append("thing2      value2");
  c.Append("thing3\tvalue3");
  c.Append("thing4 \tvalue4");
  c.Append("thing5   \t\t   value5");
  c.Append("thing6  value6\tpow");
  c.Append("thing7 \t value7\t pow");
  c.Append("  thing8 value8");
  c.Append("\tthing9 value9");


  string s; rc_t rc;
  rc = c.GetString(&s, "thing1"); EXPECT_EQ(0,rc); EXPECT_EQ("value1",s);
  rc = c.GetString(&s, "thing2"); EXPECT_EQ(0,rc); EXPECT_EQ("value2",s);
  rc = c.GetString(&s, "thing3"); EXPECT_EQ(0,rc); EXPECT_EQ("value3",s);
  rc = c.GetString(&s, "thing4"); EXPECT_EQ(0,rc); EXPECT_EQ("value4",s);
  rc = c.GetString(&s, "thing5"); EXPECT_EQ(0,rc); EXPECT_EQ("value5",s);
  rc = c.GetString(&s, "thing6"); EXPECT_EQ(0,rc); EXPECT_EQ("value6 pow",s);
  rc = c.GetString(&s, "thing7"); EXPECT_EQ(0,rc); EXPECT_EQ("value7 pow",s);
  rc = c.GetString(&s, "thing8"); EXPECT_EQ(0,rc); EXPECT_EQ("value8",s);
  rc = c.GetString(&s, "thing9"); EXPECT_EQ(0,rc); EXPECT_EQ("value9",s);


}

TEST_F(FaodelConfiguration, from_env_and_file) {

  stringstream ss;
  ss << "loglevel info\n"
     << "anotherobject boingo\n";

  //Write out to a tmp file
  char namebuf[24];
  memset(namebuf,0,sizeof(namebuf));
  strncpy(namebuf,"/tmp/ktst-XXXXXX",16);
  int fd = mkstemp(namebuf);
  ssize_t wlen = write(fd, ss.str().c_str(), ss.str().length());
  close(fd);

  rc_t rc;

  Configuration c;
  rc=c.AppendFromFile(string(namebuf)); //tmp_fname));
  EXPECT_EQ(0, rc);

  string s2;
  rc = c.GetString(&s2,"anotherobject","xxxxx");
  EXPECT_EQ("boingo", s2);
  EXPECT_EQ(0, rc);

  string s3;
  rc = c.GetString(&s3, "UnknownObject", "xxxx");
  EXPECT_EQ(s3, "xxxx");
  EXPECT_NE(0, rc);

  //Delete tmp file
  unlink(namebuf);
}

TEST_F(FaodelConfiguration, filename_expansion1) {

  stringstream ss;
  ss << "loglevel info\n"
     << "anotherobject boingo\n";

  //Write out to a tmp file
  char namebuf[24];
  memset(namebuf,0,sizeof(namebuf));
  strncpy(namebuf,"/tmp/ktst-XXXXXX",16);
  int fd = mkstemp(namebuf);
  auto wlen = write(fd, ss.str().c_str(), ss.str().length());
  close(fd);

  // put TEST_TMP in the environment
  setenv("TEST_TMP", "/tmp", 1);

  rc_t rc;

  // append from an external file after substituting $TEST_TMP
  Configuration c;
  rc=c.AppendFromFile(string("$TEST_TMP") + string(&namebuf[4]));
  EXPECT_EQ(0, rc);

  string s2;
  rc = c.GetString(&s2,"anotherobject","xxxxx");
  EXPECT_EQ("boingo", s2);
  EXPECT_EQ(0, rc);

  string s3;
  rc = c.GetString(&s3, "UnknownObject", "xxxx");
  EXPECT_EQ(s3, "xxxx");
  EXPECT_NE(0, rc);

  //Delete tmp file
  unlink(namebuf);
}

TEST_F(FaodelConfiguration, filename_expansion2) {

  stringstream ss1;
  ss1 << "loglevel info\n"
     << "anotherobject boingo\n";

  //Write out to a tmp file
  char namebuf[24];
  memset(namebuf,0,sizeof(namebuf));
  strncpy(namebuf,"/tmp/ktst-XXXXXX",16);
  int fd = mkstemp(namebuf);
  auto wlen = write(fd, ss1.str().c_str(), ss1.str().length());
  close(fd);

  // create a Configuration that will read an external file after substituting $TEST_TMP
  stringstream ss2;
  ss2 << "config.additional_files" << " " << string("$TEST_TMP") + string(&namebuf[4]) << endl;

  // put TEST_TMP in the environment
  setenv("TEST_TMP", "/tmp", 1);

  rc_t rc;

  Configuration c(ss2.str());
  rc=c.AppendFromReferences();
  EXPECT_EQ(0, rc);

  string s2;
  rc = c.GetString(&s2,"anotherobject","xxxxx");
  EXPECT_EQ("boingo", s2);
  EXPECT_EQ(0, rc);

  string s3;
  rc = c.GetString(&s3, "UnknownObject", "xxxx");
  EXPECT_EQ(s3, "xxxx");
  EXPECT_NE(0, rc);

  //Delete tmp file
  unlink(namebuf);
}

TEST_F(FaodelConfiguration, getStrings) {

  Configuration c;
  c.Append("thing1 Large");
  c.Append("thing2 Small");
  c.Append("Thing3 giANT");

  rc_t rc;
  string val;

  rc = c.GetString(&val,"thing1");
  EXPECT_EQ(0, rc);
  EXPECT_EQ("Large", val);

  rc = c.GetString(&val,"thing2","MyMissingItem");
  EXPECT_EQ(0, rc);
  EXPECT_EQ("Small", val);

  rc = c.GetString(&val,"thing_not_here","MyMissingItem");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ("MyMissingItem",val);


  rc = c.GetLowercaseString(&val, "thing3");
  EXPECT_EQ(0, rc);
  EXPECT_EQ("giant", val);

  rc = c.GetLowercaseString(&val, "thing2", "MyMissingItem");
  EXPECT_EQ(0, rc);
  EXPECT_EQ("small", val);

  rc = c.GetLowercaseString(&val, "thing_not_here2", "MyMissingItem");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ("mymissingitem", val);

}

TEST_F(FaodelConfiguration, getInt) {

  Configuration c;
  c.Append("thing1 2112");
  c.Append("thing2 -46");
  c.Append("Thing3 whoops");
  c.Append("twomeg 2M");
  c.Append("sevengig 7G");

  rc_t rc;
  int64_t val;

  rc = c.GetInt(&val, "thing1");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(2112, val);

  rc = c.GetInt(&val, "thing2");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(-46, val);

  rc = c.GetInt(&val, "NotHere", "8192");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ(8192, val);

  rc = c.GetInt(&val, "NotHere2", "4k");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ(4096, val);

  rc = c.GetInt(&val, "twomeg");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(2*1024*1024, val);

}

TEST_F(FaodelConfiguration, getUInt) {

  Configuration c;
  c.Append("thing1 2112");
  c.Append("thing2 -46");
  c.Append("Thing3 whoops");
  c.Append("twomeg 2M");
  c.Append("sevengig 7G");

  rc_t rc;
  uint64_t val;

  rc = c.GetUInt(&val, "thing1");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(2112, val);

  rc = c.GetUInt(&val, "thing2");
  EXPECT_EQ(EINVAL, rc);
  EXPECT_EQ(0, val);

  rc = c.GetUInt(&val, "NotHere", "8192");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ(8192, val);

  rc = c.GetUInt(&val, "NotHere2", "4k");
  EXPECT_EQ(ENOENT, rc);
  EXPECT_EQ(4096, val);

  rc = c.GetUInt(&val, "twomeg");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(2*1024*1024, val);

}

TEST_F(FaodelConfiguration, getPtr) {

  uint32_t v1 = 2112;
  uint32_t v2 = 5150;
  uint32_t v3 = 42;

  Configuration c;
  c.Set("album1", (void*)&v1);
  c.Set("album2", (void*)&v2);
  c.Set("answer", (void*)&v3);

  rc_t rc;
  void *val;

  rc = c.GetPtr(&val, "album1");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(2112, *(uint32_t*)val);

  rc = c.GetPtr(&val, "album2");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(5150, *(uint32_t*)val);

  rc = c.GetPtr(&val, "answer");
  EXPECT_EQ(0, rc);
  EXPECT_EQ(42, *(uint32_t*)val);

}

TEST_F(FaodelConfiguration, stringAppend) {
  Configuration c;
  string val; rc_t rc;

  c.Append("mylongitem1<>","bubbles");
  c.Append("mylongitem1<>","sangria");
  c.Append("mylongitem1<>","toast");
  rc = c.GetString(&val, "mylongitem1"); EXPECT_EQ(0, rc);  EXPECT_EQ("bubbles;sangria;toast", val);

  c.Append("mylongitem2<> bubbles2");
  c.Append("mylongitem2<> sangria2");
  c.Append("mylongitem2<> toast2");
  rc = c.GetString(&val,"mylongitem2"); EXPECT_EQ(0, rc);  EXPECT_EQ("bubbles2;sangria2;toast2", val);

  c.Append("mylongitem3<>","bubbles3");
  c.Append("mylongitem3<> sangria3");
  c.Append("mylongitem3<>","toast3");
  rc = c.GetString(&val,"mylongitem3"); EXPECT_EQ(0, rc);  EXPECT_EQ("bubbles3;sangria3;toast3", val);

  c.Append("mylongitem4","bubbles4");
  c.Append("mylongitem4","sangria4");
  c.Append("mylongitem4","toast4");
  rc = c.GetString(&val,"mylongitem4"); EXPECT_EQ(0, rc);  EXPECT_EQ("toast4", val);

  c.Append("mylongitem5<>","bubbles5");
  c.Append("mylongitem5","sangria5");
  c.Append("mylongitem5<>","toast5");
  rc = c.GetString(&val,"mylongitem5"); EXPECT_EQ(0, rc);  EXPECT_EQ("sangria5;toast5", val);

  c.Append("mylongitem6","bob;frank");
  c.Append("mylongitem6<>","tim");
  c.Append("mylongitem6<>","ed;jed");
  rc = c.GetString(&val,"mylongitem6"); EXPECT_EQ(0, rc);  EXPECT_EQ("bob;frank;tim;ed;jed", val);
  //cout << c.str() <<endl;
}

TEST_F(FaodelConfiguration, stringVector) {
  Configuration c;
  int num;
  rc_t rc;

  vector<string> my_stuff;
  c.Append("my_stuff[]", "item1");
  c.Append("my_stuff[]", "item2");
  c.Append("my_stuff[]", "item3");
  num = c.GetStringVector(&my_stuff, "my_stuff");

  EXPECT_EQ(3, num);
  EXPECT_EQ(3, my_stuff.size());
  EXPECT_EQ("item1", my_stuff[0]);
  EXPECT_EQ("item2", my_stuff[1]);
  EXPECT_EQ("item3", my_stuff[2]);

  string s;
  rc = c.GetString(&s, "my_stuff.2","");  EXPECT_EQ(0, rc); EXPECT_EQ("item3", s);
  rc = c.GetString(&s, "my_stuff.3","");  EXPECT_EQ(ENOENT, rc); EXPECT_EQ("", s);

  //Second time around should append to the back
  num = c.GetStringVector(&my_stuff, "my_stuff");
  EXPECT_EQ(3, num);
  EXPECT_EQ("item3", my_stuff[5]);
}


TEST_F(FaodelConfiguration, parseStringblock) {


  string default_config = R"EOF(
default.kelpie.core_type nonet

server.my_capacity 32M

#Client only one specified
client.fake_thing   bob


default.mutex_type  default_selected:wrong
server.mutex_type   server_selected:right
client.mutex_type   client_selected:wrong

server.security_bucket bobbucket

node_role server
)EOF";

  Configuration c(default_config);

  string val; int64_t ival;
  int rc;

  val = c.GetRole();                                      EXPECT_EQ("server", val);
  rc = c.GetString(&val, "node_role");   EXPECT_EQ(0,rc); EXPECT_EQ("server", val);
  rc = c.GetDefaultSecurityBucket(&val); EXPECT_EQ(0,rc); EXPECT_EQ("bobbucket", val);

  rc = c.GetString(&val, "mutex_type"); EXPECT_EQ(0,rc); EXPECT_EQ("server_selected:right", val);

  //Make sure we don't pick client's fake thing unless we mean it
  rc = c.GetString(&val, "fake_thing","frank"); EXPECT_EQ(2,rc); EXPECT_EQ("frank", val);
  rc = c.GetString(&val, "client.fake_thing");  EXPECT_EQ(0,rc); EXPECT_EQ("bob", val);

  rc = c.GetInt(&ival, "my_capacity"); EXPECT_EQ(0,rc); EXPECT_EQ(32*1024*1024, ival);

}



TEST_F(FaodelConfiguration, extractComponent) {

  string default_config = R"EOF(



iom.writer1.type  PosixIndividualObjects
iom.writer1.path  /tmp/foo/bar

iom.writer2.type  Hdf5Single
iom.writer2.path  /tmp/foo/myfile.h5
iom.writer2.thing 6


iom.BOSSTONE.type Mighty
iom.Bosstone.path /they/came/to/boston

default.iom.type          dummy
default.iom.extra_setting this_is_the_default_extra_setting

dht_server.ioms  writer1;writer2

server.security_bucket bobbucket

node_role dht_server
)EOF";

  Configuration c(default_config);

  map<string,string> settings1;
  c.GetComponentSettings(&settings1, "iom.writer1");
  EXPECT_EQ(2, settings1.size());
  EXPECT_EQ("PosixIndividualObjects", settings1["type"]);
  EXPECT_EQ("/tmp/foo/bar",           settings1["path"]);

  map<string,string> settings2;
  c.GetComponentSettings(&settings2, "iom.writer2");
  EXPECT_EQ(3, settings2.size());
  EXPECT_EQ("Hdf5Single",             settings2["type"]);
  EXPECT_EQ("/tmp/foo/myfile.h5",     settings2["path"]);
  EXPECT_EQ("6",                      settings2["thing"]);

  map<string,string> settings3;
  c.GetComponentSettings(&settings3, "iom.BossTone");
  EXPECT_EQ(2, settings3.size());
  EXPECT_EQ("Mighty",                 settings3["type"]);
  EXPECT_EQ("/they/came/to/boston",   settings3["path"]);
  EXPECT_EQ("",                       settings3["extra_setting"]);

  //Load defaults and then load specifics
  map<string,string> settings4;
  c.GetComponentSettings(&settings4, "default.iom");
  EXPECT_EQ("dummy",                             settings4["type"]);
  EXPECT_EQ("this_is_the_default_extra_setting", settings4["extra_setting"]);
  c.GetComponentSettings(&settings4, "iom.bosstone");
  EXPECT_EQ("Mighty",                            settings4["type"]); //<--note type gets overwritten
  EXPECT_EQ("this_is_the_default_extra_setting", settings4["extra_setting"]); //<--Extra still there
  EXPECT_EQ("/they/came/to/boston",              settings4["path"]); //<--path gets added

  //Get our ioms
  string all_ioms;
  c.GetString(&all_ioms, "ioms");
  EXPECT_EQ("writer1;writer2", all_ioms);
  auto ioms = Split(all_ioms,';',true);
  EXPECT_EQ(2, ioms.size());
  if(ioms.size()==2) {
    EXPECT_EQ("writer1", ioms[0]);
    EXPECT_EQ("writer2", ioms[1]);

    auto settings5 = c.GetComponentSettings("iom."+ioms[0]);
    EXPECT_EQ(2, settings5.size());
    EXPECT_EQ("PosixIndividualObjects", settings5["type"]);
    EXPECT_EQ("/tmp/foo/bar",           settings5["path"]);

    auto settings6 = c.GetComponentSettings("iom."+ioms[1]);
    EXPECT_EQ(3, settings6.size());
    EXPECT_EQ("Hdf5Single",             settings6["type"]);
    EXPECT_EQ("/tmp/foo/myfile.h5",     settings6["path"]);
    EXPECT_EQ("6",                      settings6["thing"]);
  }

}

TEST_F(FaodelConfiguration, autoUpdateConfig) {

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
  //cout <<"File Name is "<<fname<<endl;
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



  //ENV VAR test: Try when no env var set. Make sure it just gets conf1
  unsetenv("FAODEL_CONFIG");
  Configuration t2(config1);
  t2.AppendFromReferences();

  t2.GetString(&version,    "version.number");
  t2.GetString(&path,       "default.iom.path");
  t2.GetString(&conf_spec1, "config1.specific.info");
  t2.GetString(&conf_spec2, "config2.specific.info");
  EXPECT_EQ("/this/is/path1", path);
  EXPECT_EQ("1", version );
  EXPECT_EQ("v1", conf_spec1);
  EXPECT_EQ("",   conf_spec2);

  //ENV VAR test: Set the env var. This should be the merged configs
  setenv("FAODEL_CONFIG", fname, 1);
  Configuration t3(config1);
  t3.AppendFromReferences();
  t3.GetString(&version,    "version.number");
  t3.GetString(&path,       "default.iom.path");
  t3.GetString(&conf_spec1, "config1.specific.info");
  t3.GetString(&conf_spec2, "config2.specific.info");
  EXPECT_EQ("/this/is/path2", path);
  EXPECT_EQ("2", version );
  EXPECT_EQ("v1", conf_spec1);
  EXPECT_EQ("v2", conf_spec2);

  //Get rid of test file
  unlink(fname);

}