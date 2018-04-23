// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// Webhook Bootstrap Example
//
// Bootstrap is used to start/stop Webhook in an application. Webhook is
// different than other bootstraps in that it goes live when you Init()
// it (as opposed to when you call Start()). This is useful because it
// makes the rank's nodeid available earlier.

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <thread>

#include "webhook/Server.hh"
#include "webhook/common/QuickHTML.hh"

#include "common/Common.hh"

using namespace std;

//We use a configuration string to pass common parameters into our
//services. For webhook, the only things we need to worry about
//are the network interface you'd like to launch webhook on (important
//in nodes that have multiple nics) and the tcp port you'd like webhook
//to start on (note: you may not get that port if someone else
//is already using it). You
string default_config = R"EOF(
webhook.port 2112

bootstrap.debug true
webhook.debug true
)EOF";


int main(int argc, char* argv[]) {

  
  //The simplest hook is just a static web page. We can encode all
  //the information needed for the page inside of a lambda. More
  //sophisticated handlers should call out to functions in order to make
  //the core more readable.
  webhook::Server::registerHook("/bob", [] (const map<string,string> &args, stringstream &results){
      html::mkHeader(results, "Bob's Page");
      html::mkTable(results, args, "Bobs args");
      html::mkFooter(results);
    });


  //In this example, webhook is all we need from the FAODEL
  //stack. We need to tell bootstrap that it should launch webhook
  //and all of its dependencies
  faodel::bootstrap::Init(faodel::Configuration(default_config),
                           webhook::bootstrap);

  //Once it's started, you can retrieve our node id
  faodel::nodeid_t nid = webhook::Server::GetNodeID();

  //You should be able to browse to the web page now.
  cout<<"Started. Webserver is at: "<<nid.GetHttpLink("/bob")<<endl;
  for(int i=60; i>0; i--){
    this_thread::sleep_for(chrono::seconds(1));
    if((i%10)==0)
      cout<<"Main is running. Shutting down in: "<<i<<endl;
  }

  //Call Finish when you want to shut everything down.
  faodel::bootstrap::Finish();

  //The server should be offline now. Delay is inserted in this
  //test because sometimes TCP likes to linger
  cout <<"Should be off now. Giving 5 seconds of delay\n";
  this_thread::sleep_for(chrono::seconds(5));
  
  cout <<"Done. Exiting\n";

  return 0;
}
