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

int startDirman(const vector<string> &args);
int stopDirman(const vector<string> &args);


bool dirman_keepgoing=true;



bool dumpHelpDirman(string subcommand) {

  string help_dstart[5] = {
          "dirman-start", "dstart", "", "Start a dirman server",
          R"(
DirMan is a service for keeping track of what resources are available in a
system. A user typically launches one dirman server and then establishes
one or more resource pools for hosting data. This command launches a dirman
server and then waits for the user to issue a dstop command to stop it.

In order to make it easier to find the dirman server in later commands,
dirman-start creates a file with its nodeid when it launches. By default this
file is located at ./.faodel-dirman. You can override this location by
setting the dirman.write_root.file value in your $FAODEL_CONFIG file, or
by passing the location in through the environment variable
FAODEL_DIRMAN_ROOT_NODE_FILE.

Examples:

  # Start and generate ./.faodel-dirman
  $ faodel dstart
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=$(pwd)/.faodel-dirman
  $ faodel fstop

  # Start and specify file
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=~/.my-dirman
  $ faodel dstart
  $ faodel dstop

)"
  };

  string help_dstop[5] = {
          "dirman-stop", "dstop", "", "Stop a dirman server",
          R"(
This command communicates with a running a dirman server and issues a command
to shut it down. Stopping a dirman server does not destroy running resources,
it just makes them undiscoverable by clients. Similar to other services, the
node id for the dirman server is loaded from a file specified by environment
variables or a configuration. The service will look for:

  - $FAODEL_DIRMAN_ROOT_NODE_FILE environment variable
  - ./faodel-dirman if nothing is specified

Examples:
  # Use the default ./.faodel-dirman file
  $ faodel dstop

  # Specify a different file dirman node id file
  $ FAODEL_DIRMAN_ROOT_NODE_FILE=~/.my-dirman faodel dstop
)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_dstart);
  found |= dumpSpecificHelp(subcommand, help_dstop);
  return found;
}

int checkDirmanCommands(const std::string &cmd, const vector<string> &args) {

  if(     (cmd == "dirman-start")    || (cmd == "dstart")) return startDirman(args);
  else if((cmd == "dirman-stop")     || (cmd == "dstop"))  return stopDirman(args);

  //No command
  return ENOENT;
}

void KillDirmanHook() {
  cout<<"Dirman received shutdown request\n";
  dirman_keepgoing=false;
}

int startDirman(const vector<string> &args) {

  string root_write_file; //todo: parse args and stick in

  faodel::Configuration config;

  config.Append("whookie.app_name",          "DirMan Centralized Server");
  config.Append("dirman.host_root",          "true");
  config.Append("dirman.type",               "centralized");

  //Set logging
  modifyConfigLogging(&config, {"dirman","whookie"}, {"dirman.cache.mine", "dirman.cache.others"});

  //Dump our id to a file
  if((!config.Contains("dirman.write_root.file")) || (!root_write_file.empty())) {
    if(root_write_file.empty()){
      root_write_file = "./.faodel-dirman";
    }
    config.Append("dirman.write_root.file",  root_write_file);
  }

  dirman_keepgoing = true;

  //Startup in a way that adds a shutdown hook
  faodel::bootstrap::Init(config, dirman::bootstrap);
  whookie::Server::registerHook("/dirman/shutdown", [] (const map<string,string> &args, stringstream &results) {
       KillDirmanHook();
    });
  faodel::bootstrap::Start();

  //Wait for someone to call our shutdown service
  do {
    this_thread::sleep_for(chrono::seconds(1));
  } while(dirman_keepgoing);


  faodel::bootstrap::Finish();
  return 0;
}

int stopDirman(const vector<string> &args) {

  string dirman_type;
  faodel::Configuration config;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") {
    config.Append("dirman.type", "centralized");
  }

  //Modify for debugging settings
  modifyConfigLogging(&config, {"dirman", "whookie"}, {});

  faodel::bootstrap::Start(config, dirman::bootstrap);

  faodel::nodeid_t dirman_node = dirman::GetAuthorityNode();

  whookie::retrieveData(dirman_node, "/dirman/shutdown", nullptr);

  faodel::bootstrap::Finish();
  return 0;
}