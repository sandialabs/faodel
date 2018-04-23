// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>

#include "gtest/gtest.h"
#include "common/Common.hh"

using namespace std;
using namespace faodel;

class UrlTest : public testing::Test {
protected:
  virtual void SetUp() {
    default_bucket=bucket_t(2112, internal_use_only);
  }
  bucket_t default_bucket;
  internal_use_only_t iuo;
};


TEST_F(UrlTest, SimpleByHand) {

  //One-off check by hand
  try {
    ResourceURL l1("localkv:");
    ResourceURL l2("lkv:");
    ResourceURL l3("localkv:/");
    ResourceURL l4("lkv:[this is my bucket]/");
    ResourceURL l5("localkv:[this is my bucket]/");
    ResourceURL l6("local:[bucket]");
    ResourceURL l7("local:[bucket]&myoption=foo");
    ResourceURL l8("/local/thing");
    ResourceURL l9("/local");
    //All ok
    EXPECT_TRUE(true);
  } catch(ResourceURLParseError e) {
    cout <<e.what()<<endl;
    EXPECT_FALSE(true);
  }


  ResourceURL x("dht:<0x2>[this_is_my_bucket]/a/b/x&a=1&b=2");
  ResourceURL x2;

  bucket_t b("this_is_my_bucket");
  EXPECT_EQ("dht",         x.resource_type);
  EXPECT_EQ(0x2,           x.reference_node.nid);
  EXPECT_EQ(b,             x.bucket);
  EXPECT_EQ("/a/b",        x.path);
  EXPECT_EQ("x",           x.name);
  EXPECT_EQ("a=1&b=2",     x.options);
  EXPECT_EQ("/a/b/x",      x.GetPathName());


  //Make sure a copy copies all fields, and the fields aren't aliased
  x2=x;
  EXPECT_EQ("dht",         x2.resource_type);
  EXPECT_EQ(0x2,           x2.reference_node.nid);
  EXPECT_EQ(b,             x2.bucket);
  EXPECT_EQ("/a/b",        x2.path);
  EXPECT_EQ("x",           x2.name);
  EXPECT_EQ("a=1&b=2",     x2.options);
  EXPECT_EQ(b,             x2.bucket);


  ResourceURL y("[this_is_my_bucket]/a/b/c/");
  EXPECT_EQ(b,        y.bucket);
  EXPECT_EQ("/a/b",   y.path);
  EXPECT_EQ("c",      y.name);
  EXPECT_EQ("/a/b/c", y.GetPathName());

  ResourceURL z("[this_is_my_bucket]/a");
  EXPECT_EQ(b,      z.bucket);
  EXPECT_EQ("/",    z.path);
  EXPECT_EQ("a",    z.name);

  string s;
  s = x.GetOption("a"); EXPECT_EQ("1",s);
  s = x.GetOption("b"); EXPECT_EQ("2",s);
  s = x.GetOption("X"); EXPECT_EQ("",s);
  s = x2.GetOption("a"); EXPECT_EQ("1",s);
  s = x2.GetOption("b"); EXPECT_EQ("2",s);
  s = x2.GetOption("X"); EXPECT_EQ("",s);
  s = x.GetOption("a="); EXPECT_EQ("",s);

  vector<pair<string,string>> options = x2.GetOptions();
  EXPECT_EQ(2,options.size());
  EXPECT_EQ("a", options[0].first);
  EXPECT_EQ("b", options[1].first);
  EXPECT_EQ("1", options[0].second);
  EXPECT_EQ("2", options[1].second);

}
TEST_F(UrlTest, LocalReference) {

  //Make sure references are preserved
  ResourceURL r1("<0x0>[bucket]/my/thing&op1=yes");       EXPECT_EQ("", r1.resource_type);
  ResourceURL r2("/my/thing");                            EXPECT_EQ("", r2.resource_type);
  ResourceURL r3(":/bob");                                EXPECT_EQ("", r3.resource_type);
  ResourceURL r4("/localstuff");                          EXPECT_EQ("", r4.resource_type);
  ResourceURL r5("/localstuff/bob");                      EXPECT_EQ("", r5.resource_type);


  //User-provided type overrides everything
  ResourceURL nl1("dht:/local/item");                     EXPECT_EQ("dht", nl1.resource_type);

  //Legit locals. These should all get assigned a local resource type
  ResourceURL l1("local:<0x0>[bucket]/my/thing&op1=yes"); EXPECT_EQ("local",l1.resource_type);
  ResourceURL l2("/local/iom1");                          EXPECT_EQ("local",l2.resource_type);
  ResourceURL l3("/local");                               EXPECT_EQ("local",l3.resource_type);
  ResourceURL l4("/local/stuff/bob");                     EXPECT_EQ("local",l4.resource_type);

}

TEST_F(UrlTest, LocalOptions) {
  //Local can be a special case.. More hand tests to make sure it really works

  //Without buckets-------------------------------
  ResourceURL l1("local:&my_option=foo");
  EXPECT_EQ("foo",l1.GetOption("my_option"));
  EXPECT_EQ("/",  l1.path);
  EXPECT_EQ("",   l1.name);

  ResourceURL l2("local:/thing1&my_option=foobar");
  EXPECT_EQ("foobar", l2.GetOption("my_option"));
  EXPECT_EQ("/",      l2.path);
  EXPECT_EQ("thing1", l2.name);

  ResourceURL l3("local:/place/thing1&my_option1=foobar&my_option2=barfoo");
  EXPECT_EQ("foobar", l3.GetOption("my_option1"));
  EXPECT_EQ("barfoo", l3.GetOption("my_option2"));
  EXPECT_EQ("/place", l3.path);
  EXPECT_EQ("thing1", l3.name);

  //With buckets-------------------------------
  ResourceURL l21("local:[my_stuff]&my_option=foo");
  EXPECT_EQ("foo",l21.GetOption("my_option"));
  EXPECT_EQ("/",  l21.path);
  EXPECT_EQ("",   l21.name);

  ResourceURL l22("local:[my_stuff]/thing1&my_option=foobar");
  EXPECT_EQ("foobar", l22.GetOption("my_option"));
  EXPECT_EQ("/",      l22.path);
  EXPECT_EQ("thing1", l22.name);

  ResourceURL l23("local:[my_stuff]/place/thing1&my_option1=foobar&my_option2=barfoo");
  EXPECT_EQ("foobar", l23.GetOption("my_option1"));
  EXPECT_EQ("barfoo", l23.GetOption("my_option2"));
  EXPECT_EQ("/place", l23.path);
  EXPECT_EQ("thing1", l23.name);

  //Option Sorting: sorting doesn't happen unless you request it. However, url
  //equality is determined only by bucket, path, and name being equal (NOT OPTIONS)
  ResourceURL s1("local:[beef]&option1=foo&option2=bar");
  ResourceURL s2("local:[beef]&option2=bar&option1=foo");
  EXPECT_EQ(s1,s2);
  EXPECT_NE(s1.options, s2.options);//Reverse order
  EXPECT_EQ("option1=foo&option2=bar", s1.options); //Unsorted
  EXPECT_EQ("option2=bar&option1=foo", s2.options); //Unsorted
  auto sorted_s1=s1.GetSortedOptions();
  auto sorted_s2=s2.GetSortedOptions();
  EXPECT_EQ(sorted_s1,sorted_s2);
  EXPECT_EQ("option1=foo&option2=bar", sorted_s1);
  EXPECT_EQ("option1=foo&option2=bar", sorted_s2);
}

TEST_F(UrlTest, SimpleAutomated) {


  typedef struct {
    string rtype;
    nodeid_t node;
    bucket_t bucket;
    string  path;
    string name;
    string options;
    string bucket_path;
    string full_url;
  } check_t;


  check_t items[] = {
    { "xyz", nodeid_t(12,iuo),  bucket_t(36,iuo), "/a/b/c", "thing","op1=1&op2=2", "[0x24]/a/b/c/thing", "xyz:<0xc>[0x24]/a/b/c/thing&op1=1&op2=2" },
    { "dht", nodeid_t(128,iuo), bucket_t(10,iuo), "/x/y",   "bob",  "",            "[0xa]/x/y/bob",      "dht:<0x80>[0xa]/x/y/bob" },
    { "end", nodeid_t(),   bucket_t(),"","","","","" }
  };

  vector< pair<int,ResourceURL> > entries;
  for(int i=0; items[i].rtype != "end"; i++) {
    ResourceURL u;
    u.resource_type  = items[i].rtype;
    u.reference_node = items[i].node;
    u.bucket = items[i].bucket;
    u.path = items[i].path;
    u.name = items[i].name;
    u.options = items[i].options;
    entries.push_back( pair<int,ResourceURL>(i, u) );


    ResourceURL u2(items[i].rtype, items[i].node, items[i].bucket, items[i].path, items[i].name, items[i].options);
    entries.push_back( pair<int,ResourceURL>(i, u2) );
  }

  for( vector< pair<int,ResourceURL> >::iterator it=entries.begin(); it!=entries.end(); ++it) {
    string furl = it->second.GetFullURL();
    string bp = it->second.GetBucketPathName();
    EXPECT_EQ(items[it->first].bucket, it->second.bucket);
    EXPECT_EQ(items[it->first].node, it->second.reference_node);
    EXPECT_EQ(items[it->first].path, it->second.path);
    EXPECT_EQ(items[it->first].name, it->second.name);

    EXPECT_EQ(items[it->first].full_url, furl);
    EXPECT_EQ(items[it->first].bucket_path, bp);
    EXPECT_TRUE( it->second.Valid());
    EXPECT_TRUE( it->second.IsFullURL());

    //cout << it->second.GetFullURL() << " vs " << items[it->first].full_url<<" "<<it->second.Valid()<<endl;
  }

}

TEST_F(UrlTest, BadFormats) {


  ResourceURL ru;

  string surls_ok[] = {
    "dht:<0x9>[1]/a/b/x",
    "dht:[1]<0x3>/a/b/x",
    "dht:<0x8>[1]/a/b/x&a=1&b=2",                     //missing node
    "dht:<0x9>[1]/a/b/x&a=1&b=2",
    "dht:<0x2>[1]/a/b/x&a=1&b=2",
    "dht:<0x2>[0x2112]/a/b/x&a=1&b=2",
    "dht:<0x2>[this_is_my_bucket]/a/b/x&a=1&b=2",
    ""
  };

  for(int i=0; surls_ok[i]!=""; i++) {
    try{
      ru = ResourceURL(surls_ok[i]);
      if(!ru.Valid()) cout <<"Ok url came up invalid? "<<surls_ok[i]<<endl;
      EXPECT_TRUE(ru.Valid());
    } catch(exception e) {
      cout <<"Unexpected exception for "+surls_ok[i]+"\n";
      EXPECT_FALSE(true);
    }
  }


  string surls_bad[] = {
    "dht:<a>[1]/a/b/x&a=1&b=2",                  //bad nodeid
    "dht:<0>[1]/a/b/x&a=1&b=2",                  //bad nodeid. needs 0x prefix

    "dht:<ib://10.0.0.1:500>[1]/a/b/x&a=1&b=2",  //No longer allow nodeid encoding in url
    "dht:<mpi://10.0.0.2:900>[1]/a/b/x&a=1&b=2", //No longer allow nodeid in url


    "dht:<ixb://10.0.0.1:500>[1]/a/b/x&a=1&b=2", //bad prefix
    "dht:<ib://10.0.0.1>[1]/a/b/x&a=1&b=2",      //missing port
    "dht:<mpi://1>[1]/a/b/x&a=1&b=2",            //no trailing mpi /
    "dht:<0x2>[1>]/a/b/x&a=1&b=2",               //No delims in bucket
    "dht:<0x2/a/b/x&a=1&b=2",                    //missing >
    //"dht:<0x2>/a/b/x<&a=1&b=2",                  //extra <    todo
    ""
  };

  for(int i=0; surls_bad[i]!=""; i++) {
    try {
      ru = ResourceURL(surls_bad[i]);
      if(ru.Valid()) cout <<"Didn't fail url properly: "<<surls_bad[i]<<endl;
      EXPECT_FALSE(ru.Valid());
    } catch(exception e) {
      EXPECT_TRUE(true);
    }
  }

}

TEST_F(UrlTest, BucketPath) {
  //Often we just use a url to handle a bucket and a path

  ResourceURL x("[0x1]/this/is/it/buddy");
  EXPECT_EQ((uint32_t)1, x.bucket.bid );
  EXPECT_EQ("/this/is/it", x.path );
  EXPECT_EQ("buddy", x.name );
}

TEST_F(UrlTest, Parent) {
  ResourceURL x("[0x1]/this/is/it/buddy");
  ResourceURL xp;

  xp=x.GetLineageReference(0);  EXPECT_EQ("[0x1]/this/is/it/buddy", xp.GetBucketPathName()); EXPECT_FALSE(xp.IsRootLevel());
  xp=x.GetLineageReference(1);  EXPECT_EQ("[0x1]/this/is/it",       xp.GetBucketPathName()); EXPECT_FALSE(xp.IsRootLevel());
  xp=x.GetLineageReference(2);  EXPECT_EQ("[0x1]/this/is",          xp.GetBucketPathName()); EXPECT_FALSE(xp.IsRootLevel());
  xp=x.GetLineageReference(3);  EXPECT_EQ("[0x1]/this",             xp.GetBucketPathName()); EXPECT_TRUE(xp.IsRootLevel());
  xp=x.GetLineageReference(4);  EXPECT_EQ("[0x1]/this",             xp.GetBucketPathName()); EXPECT_TRUE(xp.IsRootLevel());
}
TEST_F(UrlTest, IsRoot) {
   ResourceURL x("[0x1]/myroot");                              EXPECT_TRUE(x.IsRootLevel());
   ResourceURL x2("ref", NODE_LOCALHOST, bucket_t(90210,iuo), "/", "myroot", ""); EXPECT_TRUE(x.IsRootLevel());

}

TEST_F(UrlTest, Local) {

  ResourceURL x("local:[0x1]");
  EXPECT_EQ("local",x.resource_type);
  EXPECT_EQ(0x1,    x.bucket.bid);

}

TEST_F(UrlTest, NoRef) {
  //Sometimes we want to ask for a resource, but we don't know exactly what the
  //thing is yet. Thus, we want to make sure the reference is prepended with
  //an empty ref (ie just ":")

  ResourceURL x("/a/b/c&op1=100&op2=200");
  string s = x.GetFullURL();
  EXPECT_EQ(":<0x0>[0x0]/a/b/c&op1=100&op2=200", s);

}

TEST_F(UrlTest, PushDir) {

  ResourceURL x;
  x.PushDir("a");  EXPECT_EQ("/a",       x.GetPathName());
  x.PushDir("b");  EXPECT_EQ("/a/b",     x.GetPathName());
  x.PushDir("c");  EXPECT_EQ("/a/b/c",   x.GetPathName());
  x.PushDir("d");  EXPECT_EQ("/a/b/c/d", x.GetPathName());

  EXPECT_EQ("/a/b/c",   x.path);
  EXPECT_EQ("d",        x.name);

  //Append multiple items in one op
  ResourceURL y("/a/b");
  y.PushDir("c/d/e/f");
  EXPECT_EQ("/a/b/c/d/e/f", y.GetPathName());
  EXPECT_EQ("/a/b/c/d/e",   y.path);
  EXPECT_EQ("f",            y.name);

  //Make sure ok if new dir starts with /
  ResourceURL z("/a/b");
  z.PushDir("/c/d/e");
  EXPECT_EQ("/a/b/c/d/e",  z.GetPathName());

  //Check for empty path add
  ResourceURL w;
  w.PushDir("/a/b/c");
  EXPECT_EQ("/a/b/c", w.GetPathName());

  //No change on empty addition
  w.PushDir("/");
  EXPECT_EQ("/a/b/c", w.GetPathName());
  EXPECT_EQ("/a/b",   w.path);
  EXPECT_EQ("c",      w.name);


}
TEST_F(UrlTest, PopDir) {
  string s;

  //Empty
  ResourceURL x;
  s=x.PopDir();
  EXPECT_EQ(s,""); EXPECT_EQ("/", x.path); EXPECT_EQ("", x.name);

  x = ResourceURL("/a");
  s=x.PopDir();
  EXPECT_EQ(s,"a"); EXPECT_EQ("/", x.path); EXPECT_EQ("", x.name);

  x = ResourceURL("/a/b");
  s=x.PopDir();
  EXPECT_EQ(s,"b"); EXPECT_EQ("/", x.path); EXPECT_EQ("a", x.name);

  x = ResourceURL("/a/b/c");
  s=x.PopDir();
  EXPECT_EQ(s,"c"); EXPECT_EQ("/a", x.path); EXPECT_EQ("b", x.name);

  x = ResourceURL("/a/b/c/d");
  s=x.PopDir(); EXPECT_EQ(s,"d");
  s=x.PopDir(); EXPECT_EQ(s,"c");
  s=x.PopDir(); EXPECT_EQ(s,"b");  EXPECT_EQ("/", x.path); EXPECT_EQ("a", x.name);
  s=x.PopDir(); EXPECT_EQ(s,"a");  EXPECT_EQ("/", x.path); EXPECT_EQ("", x.name);
  s=x.PopDir(); EXPECT_EQ(s,"");


}
TEST_F(UrlTest, GetSetOptions) {
  ResourceURL a;
  ResourceURL x("/a/b&thing1=100");
  ResourceURL y("/a/b&thing1=100&thing2=200");
  ResourceURL z("/a/b&thing1=100&thing2=200&thing3=300");

  string s;
  s=a.GetOption("thing1"); EXPECT_EQ("", s);
    a.SetOption("thingX","400");
  s=a.GetOption("thingX"); EXPECT_EQ("400", s);
    a.SetOption("thingY","500");
  s=a.GetOption("thingX"); EXPECT_EQ("400", s);
  s=a.GetOption("thingY"); EXPECT_EQ("500", s);
  s=a.GetOption("thing1"); EXPECT_EQ("", s);



  s=x.GetOption("thing1"); EXPECT_EQ("100", s);
  s=x.GetOption("thing2"); EXPECT_EQ("", s);
    x.SetOption("thingX","400");
  s=x.GetOption("thingX"); EXPECT_EQ("400", s);


  s=z.GetOption("thing1"); EXPECT_EQ("100", s);
  s=z.GetOption("thing2"); EXPECT_EQ("200", s);
  s=z.GetOption("thing3"); EXPECT_EQ("300", s);

  z.SetOption("thing1","9001");
  z.SetOption("thing2","9002");
  z.SetOption("thing3","9003");

  s=z.GetOption("thing1"); EXPECT_EQ("9001", s);
  s=z.GetOption("thing2"); EXPECT_EQ("9002", s);
  s=z.GetOption("thing3"); EXPECT_EQ("9003", s);

}

TEST_F(UrlTest, RemoveOption) {
  string s;

  ResourceURL a;
  s = a.RemoveOption("thing1"); EXPECT_EQ("",s);
  EXPECT_EQ("", a.options);

  ResourceURL b("/a/b&thing1=100");
  s = b.RemoveOption("X");      EXPECT_EQ("", s);   EXPECT_EQ("thing1=100", b.options);
  s = b.RemoveOption("thing1"); EXPECT_EQ("100",s); EXPECT_EQ("", b.options);

  ResourceURL c("/a/b&thing1=100&thing2=200");
  s = c.RemoveOption("X");      EXPECT_EQ("", s);   EXPECT_EQ("thing1=100&thing2=200", c.options);
  s = c.RemoveOption("thing1"); EXPECT_EQ("100",s); EXPECT_EQ("thing2=200", c.options);
  s = c.RemoveOption("thing2"); EXPECT_EQ("200",s); EXPECT_EQ("", c.options);

  //Multiple items
  ResourceURL d("/a/b&thing1=100&thing2=200&thing2=300&thing1=400");
  s = d.RemoveOption("X");      EXPECT_EQ("", s);   EXPECT_EQ("thing1=100&thing2=200&thing2=300&thing1=400", d.options);
  s = d.RemoveOption("thing2"); EXPECT_EQ("300",s); EXPECT_EQ("thing1=100&thing1=400", d.options);
  s = d.RemoveOption("thing1"); EXPECT_EQ("400",s); EXPECT_EQ("", d.options);
  EXPECT_EQ("", c.options);

  ResourceURL e("/a/b&thing1=100&thing2=200&thing2=300&thing1=400");
  s = e.RemoveOption("X");      EXPECT_EQ("", s);   EXPECT_EQ("thing1=100&thing2=200&thing2=300&thing1=400", e.options);
  s = e.RemoveOption("thing1"); EXPECT_EQ("400",s); EXPECT_EQ("thing2=200&thing2=300", e.options);
  s = e.RemoveOption("thing2"); EXPECT_EQ("300",s); EXPECT_EQ("", e.options);
  EXPECT_EQ("", c.options);

}

