// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

#include <mpi.h>

#include <gtest/gtest.h>

#include "faodel-common/Common.hh"

#include "whookie/Server.hh"
#include "whookie/client/Client.hh"
#include "faodel-common/QuickHTML.hh"

#include "faodel-common/Bootstrap.hh"

using namespace std;


int num_tests=0;

string default_config = R"EOF(
whookie.port 1996

#bootstrap.debug true
#whookie.debug true

)EOF";


class ClientServer : public testing::Test {

protected:
  void SetUp() override {

    server_node = whookie::Server::GetNodeID();
    //cout <<server_node.GetHttpLink()<<endl;
    num_tests++; //Keep track so main can close out this many tests
  }

  void TearDown() override {
    //TODO: ideally we'd put a stop here, but when the count goes to zero, the
    //      global whookie will stop all threads and close in a way that eats
    //      the port. For now, handle 
    //whookie::stop(); //stop kills it for everyone
    
    //faodel::bootstrap::FinishSoft();
  }

  faodel::nodeid_t server_node;
};




// Register hooks that allow you to set/read a value
TEST_F(ClientServer, Simple){
  //EXPECT_EQ(desired_port, port);

  //Add a hook to let user set a variable
  string value1;
  whookie::Server::registerHook("/test_simple1", [&value1] (const map<string,string> &args, stringstream &results) {
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
  whookie::Server::registerHook("/test_simple2", [&value2] (const map<string,string> &args, stringstream &results) {
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
    rc = whookie::retrieveData(server_node, ss_path.str(), &result);
    EXPECT_EQ(0,rc);
    EXPECT_EQ(ss_newval.str(), value2);
    //cout <<"Got back "<<value2<<endl;
    //cout <<"Result is :'"<<result<<"'\n";    
  }
  
  rc=whookie::Server::deregisterHook("/test_simple1"); EXPECT_EQ(0,rc);
  rc=whookie::Server::deregisterHook("/test_simple2"); EXPECT_EQ(0,rc);

  //Try again and make sure
  result="";
  rc = whookie::retrieveData(server_node, "/test_simple2", &result);
  EXPECT_EQ(-2,rc);
  EXPECT_EQ("", result);
}

TEST_F(ClientServer, Registrations){
  int rc;
  
  //Register some simple things
  rc=whookie::Server::registerHook("/regtest1",        [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=whookie::Server::registerHook("/regtest1/thing1", [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=whookie::Server::registerHook("/regtest1/thing2", [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);
  rc=whookie::Server::registerHook("/regtest2",        [] (const map<string,string> &args, stringstream &results) { cout <<"Got op\n"; }); EXPECT_EQ(0,rc);

  //Deregister some things
  rc=whookie::Server::deregisterHook("/regtest1");        EXPECT_EQ(0,rc);
  rc=whookie::Server::deregisterHook("/regtest1/thing1"); EXPECT_EQ(0,rc);
  rc=whookie::Server::deregisterHook("/regtest1/thing2"); EXPECT_EQ(0,rc);
  rc=whookie::Server::deregisterHook("/regtest2");        EXPECT_EQ(0,rc);
}


// Generate a stock message and then read it in via a client
TEST_F(ClientServer, ReplyStream){
  //EXPECT_EQ(desired_port, port);
  
  //Add a hook to let user set a variable
  string value1;
  whookie::Server::registerHook("/test_replystream", [&value1] (const map<string,string> &args, stringstream &results) {
      faodel::ReplyStream rs(args, "ReplyStream", &results);
      
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
  rc = whookie::retrieveData(server_node, "/test_replystream&format=txt", &result);
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


  rc=whookie::Server::deregisterHook("/test_replystream");        EXPECT_EQ(0,rc);
}



// Generate a stock message and then read it in via a client
TEST_F(ClientServer, ManyRequests){
  //EXPECT_EQ(desired_port, port);
  
  //Add a hook to let user set a variable
  string value1;
  whookie::Server::registerHook("/test_vals", [&value1] (const map<string,string> &args, stringstream &results) {
      faodel::ReplyStream rs(args, "ReplyStream", &results);
      
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

    rc = whookie::retrieveData(server_node, "/test_vals&format=txt&newval="+test_val, &result);
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
            int rc = whookie::retrieveData(tmp_server_node, "/test_vals&format=txt&newval="+test_val, &result);
            EXPECT_EQ(0,rc); 
            EXPECT_EQ(test_val+"\n", result);
          }

        }));
  }
  for(auto &t : workers){
    t.join();
  }

  rc=whookie::Server::deregisterHook("/test_vals");        EXPECT_EQ(0,rc);
}



int main(int argc, char **argv){

  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  faodel::bootstrap::Start(faodel::Configuration(default_config), whookie::bootstrap );
  faodel::nodeid_t nid = whookie::Server::GetNodeID();
  cout <<"Whookie address: "<<nid.GetHttpLink()<<endl;

  if(mpi_rank==0) cout <<"Beginning tests.\n";
  int rc = RUN_ALL_TESTS();

  faodel::bootstrap::Finish();

  MPI_Finalize();
  if(mpi_rank==0) cout <<"All complete. Exiting.\n";
  
  return rc;
}
