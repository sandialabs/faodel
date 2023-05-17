// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <exception>
#include <iostream>
#include <thread>
#include <iostream>

#include "whookie/client/Client.hh"
#include "whookie/Server.hh"
#include "kelpie/Kelpie.hh"
#include "dirman/DirMan.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

int startKelpieServer(const vector<string> &args);
int stopKelpieServer(const vector<string> &args);


bool kelpie_keepgoing=true;
int  kelpie_local_num_pools=0;



bool dumpHelpKelpieServer(string subcommand) {

  string help_kstart[5] = {
          "kelpie-start", "kstart", "<urls>", "Start a kelpie server",
          R"(
After defining resource pools with the rdef command, users will need to start
nodes to run kelpie servers that can join as nodes in the pool. When launching
a kelpie server, a user specifies a list of all the pool urls that the server
will join. Internally, a server locates dirman and issues a Join command to
volunteer to be a part of the pool. The server will continue to run until
the user issues a kstop command for all of the pools that a sever initially
was configured to join.

Example:

  # Start and generate ./.faodel-dirman
  $ faodel dstart
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=$(pwd)/.faodel-dirman

  # Define a pool with two members
  $ faodel rdef "dht:/my/dht&min_members=2"
  $ faodel kstart /my/dht &
  $ faodel kstart /my/dht &

  # Stop the pool
  $ faodel kstop /my/dht

)"
  };
  string help_kstop[5] = {
          "kelpie-stop", "kstop", "<urls>", "Stop a kelpie server",
          R"(
The kstop tool can be used to shut down a resource pool, which may terminate
one or more kelpie servers. Internally kstop talks to dirman to locate info
about each of the pools the user listed. It will drop each pool from dirman
to prevent new nodes from seeing it, and then issue a request to shutdown
each node in the pool. Each server keeps track of the number of pools it
belongs to, and will terminate when the count becomes zero.

Note: Stopping a server does not propagate to clients. If you shutdown a
      server that clients are using, it is likely the clients will crash.

Example:

  # Start and generate ./.faodel-dirman
  $ faodel dstart &
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=$(pwd)/.faodel-dirman

  # Define a pool with two members
  $ faodel rdef "dht:/my/dht&min_members=2"

  # Launch nodes to serve in the pool
  $ faodel kstart /my/dht &
  $ faodel kstart /my/dht &

  # Stop the pool
  $ faodel kstop /my/dht

)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_kstart);
  found |= dumpSpecificHelp(subcommand, help_kstop);
  return found;
}

int checkKelpieServerCommands(const std::string &cmd, const vector<string> &args) {

  if(     (cmd == "kelpie-start")    || (cmd == "kstart")) return startKelpieServer(args);
  else if((cmd == "kelpie-stop")     || (cmd == "kstop"))  return stopKelpieServer(args);

  //No command
  return ENOENT;
}

void KillKelpieHook() {
  info("Kelpie received shutdown request");
  kelpie_keepgoing=false;
}

int startKelpieServer(const vector<string> &args) {

  faodel::Configuration config;

  //Make sure we're using dirman
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") config.Append("dirman.type", "centralized");

  config.Append("whookie.app_name", "Kelpie Pool Server");

  //Modify for debugging settings
  modifyConfigLogging(&config,
                      {"kelpie", "whookie"},
                      {"opbox",  "dirman"});


  kelpie_keepgoing = true;

  //Startup in a way that adds a shutdown hook
  faodel::bootstrap::Init(config, kelpie::bootstrap);
  whookie::Server::registerHook("/kelpie/shutdown", [] (const map<string,string> &args, stringstream &results) {
      KillKelpieHook();
  });
  faodel::bootstrap::Start();


  //Join any resource the user has supplied
  for(auto p : args) {
    faodel::ResourceURL url;
    try {
      url = ResourceURL(p);

      cout <<"Trying to join "<<url.GetFullURL()<<endl;

      int rc = kelpie::JoinServerPool(url);
      if(rc==0) {
        kelpie_local_num_pools++;
        info("Joined pool "+url.GetFullURL());
      } else {
        info("Did not join pool "+url.GetFullURL());
      }

    } catch(const std::exception &e) {
      warn("Could not parse or connect to pool url '"+p+"'. Ignoring");
    }
  }

  //Wait for someone to call our shutdown service
  do {
    this_thread::sleep_for(chrono::seconds(1));
  } while(kelpie_keepgoing);


  faodel::bootstrap::Finish();
  return 0;
}

int stopKelpieServer(const vector<string> &args) {

  faodel::Configuration config;

  //Make sure we're using dirman
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") config.Append("dirman.type", "centralized");


  faodel::bootstrap::Start(config, kelpie::bootstrap);

  //Locate each resource and instruct all of its nodes to drop by one
  for(auto p : args) {
    faodel::DirectoryInfo dir;
    faodel::ResourceURL url;
    try {
      url = ResourceURL(p);

      bool ok = dirman::GetDirectoryInfo(url, &dir);
      dirman::DropDir(url); //Remove so others don't use

      if(ok) {
        for(auto name_node : dir.members ) {
          whookie::retrieveData(name_node.node, "/kelpie/shutdown", nullptr);
        }
      }

    } catch(const std::exception &e) {
      warn("Could not parse url '"+p+"'. Ignoring");
    }
  }


  faodel::bootstrap::Finish();
  return 0;
}