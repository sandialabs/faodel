// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

#include <gtest/gtest.h>

#include "common/Common.hh"

#include "webhook/Server.hh"
#include "webhook/client/Client.hh"
#include "webhook/common/QuickHTML.hh"

using namespace std;


int num_tests=0;

string default_config = R"EOF(
webhook.port 1996

#bootstrap.debug true
#webhook.debug true

)EOF";


class ClientServer : public testing::Test {

protected:
  virtual void SetUp(){    

    server_node = webhook::Server::GetNodeID();
    //cout <<server_node.GetHttpLink()<<endl;
    num_tests++; //Keep track so main can close out this many tests
  }
  virtual void TearDown(){
    //TODO: ideally we'd put a stop here, but when the count goes to zero, the
    //      global webhook will stop all threads and close in a way that eats
    //      the port. For now, handle 
    //webhook::stop(); //stop kills it for everyone
    
    //faodel::bootstrap::FinishSoft();
  }

  faodel::nodeid_t server_node;
};




// Register hooks that allow you to set/read a value
TEST_F(ClientServer, Simple){
  //EXPECT_EQ(desired_port, port);

  //Add a hook to let user set a variable
  string value1;
  webhook::Server::registerHook("/test_simple1", [&value1] (const map<string,string> &args, stringstream &results) {
      //cout <<"Got op\n";
      //Grab the value
      auto new_val = args.find("newval");
      if(new_val != args.end()){
        //cout <<"Set to "<<new_val->second<<endl;
        value1 = new_val->second;
      }
      html::mkHeader(results, "simple test");
      results<<"<h1>Simple Test Hook</h1><p>Value1 is now "<<value1<<"</p>\n";
      html::mkFooter(results);
    });

  //Add a hook to let user set a variable. This one doesn't put html around it.
  string value2;
  webhook::Server::registerHook("/test_simple2", [&value2] (const map<string,string> &args, stringstream &results) {
      //cout <<"Got op\n";
      //Grab the value
      auto new_val = args.find("newval");
      if(new_val != args.end()){
        //cout <<"Set to "<<new_val->second<<endl;
        value2 = new_val->second;
      }
      results<<"Value2 is now "<<value2;
    });

  
  //Now try pulling the data back
  int rc; string result;
  //stringstream ss_port;  ss_port << port;
  for(int i=0; i<10; i++){
    stringstream ss_newval; ss_newval <<i;
    stringstream ss_path;   ss_path << "/test_simple2&newval="<<i;
    rc = webhook::retrieveData(server_node, ss_path.str(), &result);
    EXPECT_EQ(0,rc);
    EXPECT_EQ(ss_newval.str(), value2);
    //cout <<"Got back "<<value2<<endl;
    //cout <<"Result is :'"<<result<<"'\n";    
  }
  
  rc=webhook::Server::deregisterHook("/test_simple1"); EXPECT_EQ(0,rc);
  rc=webhook::Server::deregisterHook("/test_simple2"); EXPECT_EQ(0,rc);
  
  result="";
  rc = webhook::retrieveData(server_node, "/test_simple2", &result);
  EXPECT_EQ(0,rc);
}

TEST_F(ClientServer, Registrations){
  int rc;
  
  //Register some simple things
  rc=webhook::Server::registerHook("/regtest1",        [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=webhook::Server::registerHook("/regtest1/thing1", [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=webhook::Server::registerHook("/regtest1/thing2", [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=webhook::Server::registerHook("/regtest2",        [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);

  //Deregister some things
  rc=webhook::Server::deregisterHook("/regtest1");        EXPECT_EQ(0,rc);
  rc=webhook::Server::deregisterHook("/regtest1/thing1"); EXPECT_EQ(0,rc);
  rc=webhook::Server::deregisterHook("/regtest1/thing2"); EXPECT_EQ(0,rc);
  rc=webhook::Server::deregisterHook("/regtest2");        EXPECT_EQ(0,rc);
}


// Generate a stock message and then read it in via a client
TEST_F(ClientServer, ReplyStream){
  //EXPECT_EQ(desired_port, port);
  
  //Add a hook to let user set a variable
  string value1;
  webhook::Server::registerHook("/test_replystream", [&value1] (const map<string,string> &args, stringstream &results) {
      webhook::ReplyStream rs(args, "ReplyStream", &results);
      
      auto new_val = args.find("newval");
      if(new_val != args.end()){
        //cout <<"Set to "<<new_val->second<<endl;
        value1 = new_val->second;
      }
      rs.mkText("Here is the top part of the page");
      rs.mkSection("New Section Header");
      rs.mkText("This is a new section for you to enter stuff in.");
      rs.mkText("Another chunk of text is here.");
      rs.mkSection("A smaller section",2);

      vector<string> v1({"a","b","c","d"});
      rs.mkList(v1,"List of ABCD");

      rs.Finish();
    });

  int rc;
  string result;
  rc = webhook::retrieveData(server_node, "/test_replystream&format=txt", &result);
  EXPECT_EQ(0,rc);
  string exp_string="Here is the top part of the page\n"
    "New Section Header\n"
    "This is a new section for you to enter stuff in.\n"
    "Another chunk of text is here.\n"
    "A smaller section\n"
    "List of ABCD\n"
    "a\n"
    "b\n"
    "c\n"
    "d\n";
  EXPECT_EQ(exp_string, result);


  rc=webhook::Server::deregisterHook("/test_replystream");        EXPECT_EQ(0,rc);
}



// Generate a stock message and then read it in via a client
TEST_F(ClientServer, ManyRequests){
  //EXPECT_EQ(desired_port, port);
  
  //Add a hook to let user set a variable
  string value1;
  webhook::Server::registerHook("/test_vals", [&value1] (const map<string,string> &args, stringstream &results) {
      webhook::ReplyStream rs(args, "ReplyStream", &results);
      
      auto new_val = args.find("newval");
      if(new_val != args.end()){
        //cout <<"Set to "<<new_val->second<<endl;
        value1 = new_val->second;
      }
      rs.mkText(value1);
      rs.Finish();
    });

  //Use our thread to launch many requests
  int rc;
  string result;
  string test_val="test_val";
  for(int i=0; i<100; i++){

    rc = webhook::retrieveData(server_node, "/test_vals&format=txt&newval="+test_val, &result);
    EXPECT_EQ(0,rc);
    EXPECT_EQ(test_val+"\n", result);
  }

  //Launch several gatherers as threads
  vector<std::thread> workers;
  for(int i=0; i<4; i++){
    auto tmp_server_node = server_node; //Wasn't being passed in right
    workers.push_back( std::thread([i,tmp_server_node]() {
          string result;
          for(int j=0; j<1000; j++) {
            string test_val="test_"+to_string(i)+"_"+to_string(j);
            int rc = webhook::retrieveData(tmp_server_node, "/test_vals&format=txt&newval="+test_val, &result);
            EXPECT_EQ(0,rc); 
            EXPECT_EQ(test_val+"\n", result);
          }

        }));
  }
  for(auto &t : workers){
    t.join();
  }

  rc=webhook::Server::deregisterHook("/test_vals");        EXPECT_EQ(0,rc);
}



int main(int argc, char **argv){

  ::testing::InitGoogleTest(&argc, argv);

  faodel::bootstrap::Start(faodel::Configuration(default_config), webhook::bootstrap );
  faodel::nodeid_t nid = webhook::Server::GetNodeID();
  cout <<"Webhook address: "<<nid.GetHttpLink()<<endl;

  
  int rc = RUN_ALL_TESTS();
 
  //faodel::bootstrap::Finish();
  
  return rc;
}
