// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <limits.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "dirman/common/DirectoryCache.hh"

#include <stdio.h>

using namespace std;
using namespace faodel;
using namespace dirman;

bool ENABLE_DEBUG_MESSAGES=false;




class DirectoryCacheTest : public testing::Test {


protected:

  //NOTE: In old kelpie we used node id's staring with 0 for simplicity.
  //      unfortunately, 0 is also the NODE_UNSPECIFIED, so some commands
  //      panic if you set node to 0. To get around this, we add
  //      tree_starting_node_id to every node value. Make sure you subtract
  //      it if you are generating the position in a tree.
  const int tree_starting_node_id = 0x100;

  virtual pair<nodeid_t,string> MakePathPair(nodeid_t nid, string name){
    stringstream ss;
    ss<<"ref:<"<<nid.GetHex()<<">["<<def_bucket_name<<"]"<<name;
    return pair<nodeid_t,string>(nid, ss.str());
  }

  virtual string GetEntryName(unsigned int level, unsigned int row_offset){
    stringstream ss;
    char prefix = 'A'+(unsigned char)level;
    ss << prefix << row_offset;
    return ss.str();
  }

  virtual vector<pair<nodeid_t,string>> GenerateTree(unsigned int fanout, unsigned int num_rows){

    vector< pair<nodeid_t,string> > names;

    nodeid_t node(tree_starting_node_id,iuo);
    unsigned int nodes_per_row=1;
    for(unsigned int i=0; i<num_rows; i++){

      if(i==0){
        names.push_back( MakePathPair(node, "/A0") );
        node.nid++;
        nodes_per_row *= fanout;

      } else {

        //Do each node in the row
        for(unsigned int j=0; j<nodes_per_row; j++){
          //Build the path up for parents
          vector<string> parts;
          unsigned int val=j;
          for(int k=i; k>=0; k--){
            parts.insert( parts.begin(), GetEntryName(k,((k==(int)i) ? j : val)));
            val = val/fanout;
          }
          stringstream ss;
          for(auto &s : parts)
            ss << "/" << s;
          names.push_back( MakePathPair(node, ss.str()));
          node.nid++;
        }
        nodes_per_row *= fanout;
      }
    }
    return names;
  }


  virtual void GetChildInfo(unsigned int fanout, int parent_id,
                            unsigned int *child_level, unsigned int *child_id, unsigned int *child_offset){

    //Get stats for our row
    unsigned int parent_row_start=0;
    unsigned int parent_nodes_per_row=1;
    unsigned int level=0;
    while(parent_row_start+parent_nodes_per_row <= parent_id){
      parent_row_start+=parent_nodes_per_row;
      parent_nodes_per_row *= fanout;
      level++;
    }

    unsigned int parent_offset=parent_id-parent_row_start;
    unsigned int child_row_start = parent_row_start + parent_nodes_per_row;

    unsigned int first_child_offset = parent_offset*fanout;

    *child_level = level+1;
    *child_id = child_row_start+first_child_offset;
    *child_offset = first_child_offset;
    return;
  }
  virtual bool GenChildVector(unsigned int fanout, nodeid_t parent_id, unsigned int num_rows, vector<NameAndNode> *members){

    unsigned int child_level, child_id, child_offset;
    GetChildInfo(fanout, parent_id.nid-tree_starting_node_id, &child_level, &child_id, &child_offset);

    if(child_level >= num_rows) return false;

    for(unsigned int i=0; i<fanout; i++){
      members->push_back( NameAndNode(GetEntryName(child_level, child_offset+i), nodeid_t(child_id+i,iuo)) );
    }
    return true;
  }


  virtual void SetUp(){
    def_bucket_name="mine";
    def_bucket = bucket_t(def_bucket_name);

    //Note: Additional configuration settings will be loaded the file specified by FAODEL_CONFIG
    //      HOWEVER, this test uses a unique name for the cache, so you may see no difference
    stringstream ss;
    if(ENABLE_DEBUG_MESSAGES)
      ss<< "dirman.cache.test.debug true\n";

    Configuration config = Configuration(ss.str());
    config.AppendFromReferences();

    dc = new DirectoryCache("dirman.cache.test");

    dc->Init(config, "none","test");
    rnames.push_back(MakePathPair(nodeid_t(100,iuo),"/a"));
    rnames.push_back(MakePathPair(nodeid_t(101,iuo),"/a/b1"));
    rnames.push_back(MakePathPair(nodeid_t(102,iuo),"/a/b2"));
    rnames.push_back(MakePathPair(nodeid_t(103,iuo),"/a/b1/c1"));
    rnames.push_back(MakePathPair(nodeid_t(104,iuo),"/a/b1/c2"));
    rnames.push_back(MakePathPair(nodeid_t(105,iuo),"/a/b1/c3"));
    rnames.push_back(MakePathPair(nodeid_t(106,iuo),"/a/b2/c1"));
    rnames.push_back(MakePathPair(nodeid_t(107,iuo),"/a/b2/c2"));
    rnames.push_back(MakePathPair(nodeid_t(108,iuo),"/a/b2/c3"));
  }

  void TearDown() override {
    delete dc;
  }
  
  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bucket_t def_bucket;
  string   def_bucket_name;
  DirectoryCache *dc;
  vector< pair<nodeid_t,string> > rnames;
  vector< pair<int,string> > tnames;
};


/**
 * Stores a single resource into the dc and then updates it
 */
TEST_F(DirectoryCacheTest, SimpleByHand){

  //--- Create a simple reference to node 100, store, and retrieve ---
  pair<nodeid_t,string> pp=MakePathPair(nodeid_t(100,iuo),"/x/y/z");

  DirectoryInfo ri(pp.second);
  bool ok = dc->Create(ri);
  EXPECT_TRUE(ok);

  DirectoryInfo ro;
  nodeid_t node(-1,iuo);
  bool found = dc->Lookup(ResourceURL(pp.second), &ro, &node);
  EXPECT_TRUE(found);
  EXPECT_EQ(pp.first,   node);
  EXPECT_EQ(pp.first,   ro.url.reference_node);
  EXPECT_EQ("/x/y",     ro.url.path);
  EXPECT_EQ("z",        ro.url.name);
  EXPECT_EQ(def_bucket, ro.url.bucket);
  EXPECT_EQ(0,          ro.members.size());


  //--- Modify the item, push it back, and verify ----
  ro.members.push_back(NameAndNode("Bobby",nodeid_t(101,iuo)));
  ro.members.push_back(NameAndNode("Billy",nodeid_t(102,iuo)));
  ro.members.push_back(NameAndNode("Betty",nodeid_t(103,iuo)));

  ok = dc->Update(ro);
  EXPECT_TRUE(ok);

  DirectoryInfo ro2;
  found = dc->Lookup(ResourceURL(pp.second), &ro2, &node);
  EXPECT_TRUE(found);
  EXPECT_EQ(pp.first,   node);
  EXPECT_EQ(pp.first,   ro2.url.reference_node);
  EXPECT_EQ("/x/y",     ro2.url.path);
  EXPECT_EQ("z",        ro2.url.name);
  EXPECT_EQ(def_bucket, ro2.url.bucket);
  EXPECT_EQ(3,          ro2.members.size());

  //cout <<"----"<<ro2.str(2)<<"---\n";
  EXPECT_EQ("Bobby", ro2.members[0].name);
  EXPECT_EQ("Billy", ro2.members[1].name);
  EXPECT_EQ("Betty", ro2.members[2].name);
  EXPECT_EQ(nodeid_t(101,iuo),     ro2.members[0].node);
  EXPECT_EQ(nodeid_t(102,iuo),     ro2.members[1].node);
  EXPECT_EQ(nodeid_t(103,iuo),     ro2.members[2].node);
  //cout << "////////Item\n"<<dc->str(100)<<"/////Done\n";
}


TEST_F(DirectoryCacheTest, SimpleAutomated){

  bool found;

  //Register all the resources
  for(auto &id_name : rnames){
    DirectoryInfo ri(id_name.second);
    stringstream ss;
    ss << "Entry "<<id_name.first.GetHex()<<" uncoverted url: "<<id_name.second<<endl;
    //cout <<"Trying to register "<<id_name.second<<endl;
    //cout <<"After init, the ri url is "<<ri.url.GetFullURL()<<endl;
    ri.info = ss.str();
    bool ok = dc->Create(ri);
    EXPECT_TRUE(ok);

    DirectoryInfo ri2;
    nodeid_t node;
    ok = dc->Lookup(ResourceURL(id_name.second), &ri2, &node);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ri.url.GetFullURL(), ri2.url.GetFullURL());
  }

  //cout <<dc->str(100);
  random_shuffle(rnames.begin(), rnames.end());

  //Lookup via name, only care about available or not
  for(auto &id_name : rnames){
    //cout <<"Looking up "<<id_name.second<<endl;
    found = dc->Lookup(ResourceURL(id_name.second));     EXPECT_TRUE(found);
    found = dc->Lookup(ResourceURL(id_name.second));     EXPECT_TRUE(found);
    found = dc->Lookup(ResourceURL(id_name.second+"X")); EXPECT_FALSE(found);
  }

  //Lookup via name, check the guts
  for(auto &id_name : rnames) {
    DirectoryInfo ri; ResourceURL ru;
    //cout <<"Looking up "<<it->second<<endl;
    found = dc->Lookup(ResourceURL(id_name.second), &ri);      EXPECT_TRUE(found);
    ru = ResourceURL(id_name.second);                         EXPECT_EQ(ru, ri.url);
    EXPECT_EQ(ru.reference_node, ri.url.reference_node);

  }
}

TEST_F(DirectoryCacheTest, TreeUpdateMembers){

  unsigned int fanout=3;
  unsigned int num_rows=3;
  nodeid_t node;
  vector< pair<nodeid_t,string> > names = GenerateTree(fanout, num_rows);

  //random_shuffle(names.begin(), names.end());

  //Insert all, do quick check
  bool ok;
  for(auto &id_name : names){
    //cout << id_name.first <<" -> "<<id_name.second<<endl;
    DirectoryInfo ri(id_name.second);
    ok = dc->Create(ri);
    EXPECT_TRUE(ok);

    DirectoryInfo ro;
    ok = dc->Lookup(ResourceURL(id_name.second), &ro, &node);
    EXPECT_TRUE(ok);
    EXPECT_EQ(id_name.first, node);
  }

  //Modify each one to have true hierarchy
  for(auto &id_name : names){
    //cout << id_name.first <<" -> "<<id_name.second<<endl;
    DirectoryInfo ro;
    ok = dc->Lookup(ResourceURL(id_name.second), &ro, &node);
    EXPECT_TRUE(ok);
    EXPECT_EQ(id_name.first, node);
    EXPECT_EQ(0, ro.members.size());

    nodeid_t id = id_name.first;

    bool had_members = GenChildVector(fanout, id_name.first, num_rows, &ro.members);
    if(!had_members) continue;

    ok = dc->Update(ro);
    EXPECT_TRUE(ok);
  }
  //cout << "////////Item\n"<<dc->str(100)<<"/////Done\n";

  //Check all the kids..
  for(auto &id_name : names){
    DirectoryInfo ro;
    ok = dc->Lookup(ResourceURL(id_name.second), &ro, &node);
    EXPECT_TRUE(ok);
    EXPECT_EQ(id_name.first, node);

    vector<NameAndNode> expected_members;
    bool had_members = GenChildVector(fanout, id_name.first, num_rows, &expected_members);
    EXPECT_EQ(expected_members.size(), ro.members.size());
    if(expected_members.size() == ro.members.size()){
      for(unsigned int i=0; i< expected_members.size(); i++){
        EXPECT_EQ(expected_members[i], ro.members[i]);
      }
    }
  }
}



TEST_F(DirectoryCacheTest, TreeJoin){

  unsigned int fanout=3;
  unsigned int num_rows=3;
  nodeid_t node;
  vector< pair<nodeid_t,string> > names = GenerateTree(fanout, num_rows);



  bool ok;

  //First, try Joining when parent doesn't exist. Should be not ok (no idea what resource is yet)
  for(auto &id_name : names){
    DirectoryInfo ro;
    ok = dc->Join(ResourceURL(id_name.second), &ro);
    EXPECT_FALSE(ok);
  }

  //Now Register everyone
  for(auto &id_name : names){
    ok = dc->Create(DirectoryInfo(id_name.second));
    EXPECT_TRUE(ok);
  }


  //Now try having everybody but root (can't join self) join.
  for(auto &id_name : names){
    DirectoryInfo ro;
    ok = dc->Join(ResourceURL(id_name.second), &ro);
    if(id_name.first.nid !=tree_starting_node_id)  EXPECT_TRUE(ok);
    else                                           EXPECT_FALSE(ok); //Root registering self should get rejected
  }

  //Now walk through everything and verify kids are all ok
  for(auto &id_name : names){
    DirectoryInfo ro;
    ok = dc->Lookup(ResourceURL(id_name.second), &ro, &node);
    EXPECT_TRUE(ok);
    EXPECT_EQ(id_name.first, node);

    vector<NameAndNode> expected_members;
    bool had_members = GenChildVector(fanout, id_name.first, num_rows, &expected_members);


    EXPECT_EQ(expected_members.size(), ro.members.size());
    if(expected_members.size() == ro.members.size()){
      for(unsigned int i=0; i< expected_members.size(); i++){
        EXPECT_EQ(expected_members[i], ro.members[i]);
      }
    } else {
      cout <<"Child problem for "<<id_name.second<<endl;
      cout <<"Expected: \n";
      for(auto &node_name : expected_members)
        cout<<node_name.name <<" "<<node_name.node.GetHex()<<endl;

      cout <<"Actual:\n";
      for(auto node_name : ro.members)
        cout<<node_name.name <<" "<<node_name.node.GetHex()<<endl;
      exit(0);
    }
  }

}
TEST_F(DirectoryCacheTest, JoinLeave) {

  nodeid_t ref_node;
  bool ok;


  //Create an entry for /my, make /my/thing a child
  DirectoryInfo di0;
  ok = dc->Create(DirectoryInfo("<0x05>/my"));         EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL("<0x10>/my/thing"), &di0); EXPECT_TRUE(ok);
  EXPECT_EQ(1, di0.members.size());
  di0.GetChildReferenceNode("thing",&ref_node); EXPECT_EQ(nodeid_t(0x10,iuo),ref_node);


  //Create an entry and have different nodes join it
  ok = dc->Create(DirectoryInfo("<0x10>/my/thing")); EXPECT_TRUE(ok);
  ok = dc->Create(DirectoryInfo("<0x99>/my/thing")); EXPECT_FALSE(ok);
  ok = dc->Join(ResourceURL("<0x20>/my/thing/a"));   EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL("<0x21>/my/thing/b"));   EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL("<0x22>/my/thing/c"));   EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL("<0x99>/my/thing/c"));   EXPECT_FALSE(ok);
  ok = dc->Join(ResourceURL("<0x23>/my/thing/d"));   EXPECT_TRUE(ok);

  //Get the entry and inspect it to make sure it's right
  DirectoryInfo di1;
  ok = dc->Lookup(ResourceURL("/my/thing"), &di1, &ref_node); EXPECT_TRUE(ok);
  EXPECT_EQ(nodeid_t(0x10, iuo), ref_node);
  EXPECT_EQ(di1.members.size(), 4);
  di1.GetChildReferenceNode("a", &ref_node); EXPECT_EQ(nodeid_t(0x20,iuo), ref_node);
  di1.GetChildReferenceNode("b", &ref_node); EXPECT_EQ(nodeid_t(0x21,iuo), ref_node);
  di1.GetChildReferenceNode("c", &ref_node); EXPECT_EQ(nodeid_t(0x22,iuo), ref_node);
  di1.GetChildReferenceNode("d", &ref_node); EXPECT_EQ(nodeid_t(0x23,iuo), ref_node);

  //Remove something
  ok = dc->Leave(ResourceURL("/my/thing/c")); EXPECT_TRUE(ok);
  ok = dc->Leave(ResourceURL("/my/thing/c")); EXPECT_FALSE(ok);
  ok = dc->Lookup(ResourceURL("/my/thing"), &di1, &ref_node); EXPECT_TRUE(ok);
  EXPECT_EQ(nodeid_t(0x10, iuo), ref_node);
  ok = di1.GetChildReferenceNode("a", &ref_node); EXPECT_EQ(nodeid_t(0x20,iuo), ref_node);
  ok = di1.GetChildReferenceNode("b", &ref_node); EXPECT_EQ(nodeid_t(0x21,iuo), ref_node);
  ok = di1.GetChildReferenceNode("c", &ref_node); EXPECT_FALSE(ok);
  ok = di1.GetChildReferenceNode("d", &ref_node); EXPECT_EQ(nodeid_t(0x23,iuo), ref_node);

  //Get rid of all members, one-by-one
  for(auto name_node : di1.members){
    ok = dc->Leave(ResourceURL("/my/thing/"+name_node.name)); EXPECT_TRUE(ok);
  }
  ok = dc->Lookup(ResourceURL("/my/thing"), &di1, &ref_node); EXPECT_TRUE(ok);
  EXPECT_EQ(0, di1.members.size());


  //Add first tree back in
  ok = dc->Create(DirectoryInfo("<0x21>/my/thing/aa"));   EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL(    "<0x21>/my/thing/aa"));   EXPECT_TRUE(ok); //Parent link
  ok = dc->Join(ResourceURL(    "<0x22>/my/thing/aa/1")); EXPECT_TRUE(ok); //child member
  ok = dc->Join(ResourceURL(    "<0x23>/my/thing/aa/2")); EXPECT_TRUE(ok); //child member
  ok = dc->Join(ResourceURL(    "<0x24>/my/thing/aa/3")); EXPECT_TRUE(ok); //child member

  //Add second tree back in
  ok = dc->Create(DirectoryInfo("<0x31>/my/thing/bb"));   EXPECT_TRUE(ok);
  ok = dc->Join(ResourceURL(    "<0x31>/my/thing/bb"));   EXPECT_TRUE(ok); //Parent link
  ok = dc->Join(ResourceURL(    "<0x32>/my/thing/bb/1")); EXPECT_TRUE(ok); //child member
  ok = dc->Join(ResourceURL(    "<0x33>/my/thing/bb/2")); EXPECT_TRUE(ok); //child member


  //Verify parent ok
  DirectoryInfo di2;
  ok = dc->Lookup(ResourceURL("/my/thing"), &di2, &ref_node); EXPECT_TRUE(ok);
  di2.GetChildReferenceNode("bb", &ref_node); EXPECT_EQ(nodeid_t(0x31,iuo), ref_node);
  di2.GetChildReferenceNode("aa", &ref_node); EXPECT_EQ(nodeid_t(0x21,iuo), ref_node);
  EXPECT_EQ(2, di2.members.size());

  //Left child tree aa
  ok = dc->Lookup(ResourceURL("/my/thing/aa"), &di2, &ref_node); EXPECT_TRUE(ok);
  di2.GetChildReferenceNode("1", &ref_node); EXPECT_EQ(nodeid_t(0x22,iuo), ref_node);
  di2.GetChildReferenceNode("2", &ref_node); EXPECT_EQ(nodeid_t(0x23,iuo), ref_node);
  di2.GetChildReferenceNode("3", &ref_node); EXPECT_EQ(nodeid_t(0x24,iuo), ref_node);
  EXPECT_EQ(3, di2.members.size());

  //Right child tree bb
  ok = dc->Lookup(ResourceURL("/my/thing/bb"), &di2, &ref_node); EXPECT_TRUE(ok);
  di2.GetChildReferenceNode("1", &ref_node); EXPECT_EQ(nodeid_t(0x32,iuo), ref_node);
  di2.GetChildReferenceNode("2", &ref_node); EXPECT_EQ(nodeid_t(0x33,iuo), ref_node);
  EXPECT_EQ(2, di2.members.size());


  //Delete bb. Should leave /my /my/thing /my/thing/aa
  ok = dc->Remove(ResourceURL("/my/thing/bb")); EXPECT_TRUE(ok);
  EXPECT_EQ(3, dc->NumberOfResources());

  //Verify rest of stuff still there
  ok = dc->Lookup(ResourceURL("/my/thing"), &di2); EXPECT_TRUE(ok);
  di2.GetChildReferenceNode("aa", &ref_node); EXPECT_EQ(nodeid_t(0x21,iuo), ref_node);
  EXPECT_EQ(1, di2.members.size());
  ok = dc->Lookup(ResourceURL("/my/thing/bb"), &di2); EXPECT_FALSE(ok);
  ok = dc->Lookup(ResourceURL("/my/thing/aa"), &di2); EXPECT_TRUE(ok);
  di2.GetChildReferenceNode("1", &ref_node); EXPECT_EQ(nodeid_t(0x22,iuo), ref_node);
  di2.GetChildReferenceNode("2", &ref_node); EXPECT_EQ(nodeid_t(0x23,iuo), ref_node);
  di2.GetChildReferenceNode("3", &ref_node); EXPECT_EQ(nodeid_t(0x24,iuo), ref_node);
  EXPECT_EQ(3, di2.members.size());

  //vector<ResourceURL> urls1;
  //dc->GetAllURLs(&urls1);
  //for(auto u : urls1){
  //  cout << u.GetFullURL()<<endl;
  //}


  //Get rid of whole tree
  ok = dc->Remove(ResourceURL("/my")); EXPECT_TRUE(ok);
  EXPECT_EQ(0, dc->NumberOfResources());

}

TEST_F(DirectoryCacheTest, CreateAndLink ) {

  nodeid_t ref_node;
  bool ok;


  //Create an entry for /my, make /my/thing a child
  DirectoryInfo di0;
  ok = dc->CreateAndLinkParents(DirectoryInfo("dht:<0x05>[frank]/my/baloney/has/a/first/name")); EXPECT_TRUE(ok);
  ok = dc->CreateAndLinkParents(DirectoryInfo("dht:<0x06>[frank]/my/baloney/has/macaroni"));     EXPECT_TRUE(ok);
  ok = dc->CreateAndLinkParents(DirectoryInfo("dht:<0x07>[frank]/my/baloney/has/a/zamboni"));    EXPECT_TRUE(ok);

  //The parent node should be set based on the original url, until it hits
  //a known parent. This will need fixing later on..

  vector<pair<int,string>> entries {
    { 0x05, "/my"},
    { 0x05, "/my/baloney"},
    { 0x05, "/my/baloney/has"},
    { 0x05, "/my/baloney/has/a"},
    { 0x05, "/my/baloney/has/a/first"},
    { 0x05, "/my/baloney/has/a/first/name"},
    { 0x06, "/my/baloney/has/macaroni"},
    { 0x07, "/my/baloney/has/a/zamboni"}
  };

  DirectoryInfo di;
  for( auto val_name : entries){
    ok = dc->Lookup(ResourceURL("[frank]"+val_name.second), &di); EXPECT_TRUE(ok);
    EXPECT_EQ(nodeid_t(val_name.first, iuo), di.GetReferenceNode());
  }

  //Remove things
  ok = dc->Remove(ResourceURL("[frank]/my/baloney/has/a")); EXPECT_TRUE(ok);

  //Make sure none start with /my/baloney/has/a
  vector<ResourceURL> urls1;
  dc->GetAllURLs(&urls1);
  for(auto u : urls1){
    string not_good="/my/baloney/has/a";
    string s = u.GetURL();
    EXPECT_NE(not_good, s.substr(0,not_good.size()));
  }
  EXPECT_EQ(4,urls1.size());

  //Debug
  //vector<ResourceURL> urls2;
  //dc->GetAllURLs(&urls2);
  //for(auto u : urls2){
  //  cout << u.GetFullURL()<<endl;
  //}


}
