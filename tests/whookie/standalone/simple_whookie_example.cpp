// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 



#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <thread>

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

using namespace std;

string default_config_string = R"EOF(
whookie.debug   true
whookie.port    1990
#whookie.interfaces ipogif0,eth,lo
)EOF";



/**
 * @brief Simple callback to demonstrate web request can trigger operation
 *
 * @param[in] ss info
 */
void SayHello(){
  cout<<"Hello from whookie\n";
}

int main(int argc, char* argv[]) {


  whookie::Server::registerHook("/bob", [] (const map<string,string> &args, stringstream &results){
      html::mkHeader(results, "Bob's Page");
      html::mkTable(results, args, "Bobs args");
      html::mkFooter(results);
    });

  whookie::Server::registerHook("/SayHello", [] (const map<string,string> &args, stringstream &results){
      html::mkHeader(results, "Triggering Hello");
      html::mkSection(results, "Triggering Hello");
      html::mkText(results, "Each time you go to this page, the executable should say hello.\n");
      html::mkFooter(results);
      SayHello();
    });

  


  faodel::bootstrap::Start(faodel::Configuration(default_config_string), whookie::bootstrap);

  faodel::nodeid_t nid = whookie::Server::GetNodeID();

  cout<<"Simple example that starts a webserver, registers a handler, and then waits for\n"
      <<"some time before shutting down. When running on a local desktop, you can look\n"
      <<"around in a browser by going to "
      << nid.GetHttpLink()<<endl;
  



  cout<<"Started..\n";
  for(int i=10; i>0; i--){
    this_thread::sleep_for(chrono::seconds(5));
    cout<<"Main is running. Shutting down in: "<<i<<endl;
  }
  cout <<"About to exit\n";
  faodel::bootstrap::Finish();
  
  cout <<"Should be off now.\n";
  this_thread::sleep_for(chrono::seconds(5));
  
  cout <<"Done work. Exiting.\n";

  return 0;
}
