// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <stdlib.h>
#include <iostream>
#include <libgen.h>
#include <iomanip>

#include "faodel-common/StringHelpers.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

//Global
int global_verbose_level=0;
int global_rank=0;

//Helper functions
bool dumpSpecificHelp(string subcommand, const string options[5]) {

  //cout <<"Checking "<<subcommand<<" against "<<options[0]<<endl;

  if(subcommand.empty() || (subcommand==options[0]) || (subcommand==options[1]) ) {

    //Dump short version
    cout << "  " << setw(17) << std::left << options[0]
         << "| " << setw(7) << std::left << options[1]
         << " "  << setw(7) << std::left << options[2]
         << ": " << options[3] << endl;

    //Dump long version if this was really for us
    if(!subcommand.empty()) {
      cout << endl << options[4] << endl;
    }
    return true;
  }
  return false;
}

void info(const string &s) {
  if(global_verbose_level>0) cout <<"I cli: "<<s<<endl;
}
void dbg(const string &s) {
  if(global_verbose_level>1) cout <<"D cli: "<<s<<endl;
}
void warn(const string &s){
  cerr << "\033[1;31m" << "Warning:" << ":\033[0m "<<s<<endl;
}

void info0(const string &s) {
  if(global_rank==0) info(s);
}
void dbg0(const string &s) {
  if(global_rank==0) dbg(s);
}
void warn0(const string &s) {
  if(global_rank==0) warn(s);
}

void modifyConfigLogging(Configuration *config, const vector<string > &basic_service_names, const vector<string> &very_verbose_service_names) {

  for(auto s : basic_service_names) {
    if(global_verbose_level>0) config->Append(s+".log.info", "true");
    if(global_verbose_level>1) config->Append(s+".debug",    "true");
  }

  //Some services like dirman you can turn on lower level components, like the caches
  if(global_verbose_level>2) {
    for(auto s : very_verbose_service_names) {
      config->Append(s + ".debug", "true");
    }
  }

}


int dumpHelp(string subcommand) {
  cout <<"faodel <options> COMMAND <args>\n"
       <<"\n"
       <<" options:\n"
       <<"  -v/-V or --verbose/--very-verbose : Display runtime/debug info\n"
       <<"  --dirman-node id                  : Override config and use id for dirman\n"
       <<"\n"
       <<" commands:\n";

  bool found=false;
  found |= dumpHelpBuild(subcommand);
  found |= dumpHelpConfig(subcommand);
  found |= dumpHelpWhookieClient(subcommand);
  found |= dumpHelpDirman(subcommand);
  found |= dumpHelpResource(subcommand);
  found |= dumpHelpKelpieServer(subcommand);
  found |= dumpHelpKelpieClient(subcommand);
  found |= dumpHelpKelpieBlast(subcommand);
  found |= dumpHelpAllInOne(subcommand);
  found |= dumpHelpPlay(subcommand);

  string help_help[5] = {
          "help", "help", "<cmd>", "Provide more info about specific commands\n",
          R"(
Provide more information about specific commands.

Example:

  faodel help dirman-start
)"
  };

  found |= dumpSpecificHelp(subcommand, help_help);

  if(!found) {
    cout <<"Error: help command '"<<subcommand<<"' not found?\n";
  }
  return 0;
}


void setDefaultEnvVars() {

  //Check and set our default env vars for pointing to dirman. The order in which
  //dirman root gets set is:
  //  config has dirman.root_node
  //  config has dirman.root_node.file
  //  config has dirman.root_node.file.env_name  (user sets config to check env_name)
  //  command has -d nodeid
  //  env FAODEL_DIRMAN_ROOT_NODE specified
  //  env FAODEL_DIRMAN_ROOT_NODE_FILE specified
  //  file ./.faodel-dirman
  char *env_nodeid;
  char *env_filename;
  env_nodeid = getenv("FAODEL_DIRMAN_ROOT_NODE");
  env_filename = getenv("FAODEL_DIRMAN_ROOT_NODE_FILE");

  if(env_nodeid != nullptr) {
    //Always prioritize the node setting. When it exists, wipe out file setting
    unsetenv("FAODEL_DIRMAN_ROOT_NODE_FILE");
  } else if (env_filename == nullptr) {
    //Neither were provided. Plug in our default file:  ./.faodel-dirman
    setenv("FAODEL_DIRMAN_ROOT_NODE_FILE", "./.faodel-dirman", true);
  }

  //Note: option parsing will override both of these if a user supplies "-d nodeid" as option
}

int main(int argc, char **argv) {

  int rc = 0;

  string cmd;
  vector<string> args;

  //See if we're called from a symlink to a specific tool. These commands all have
  //a filename that starts with faodel (eg faodel-binfo), so we can strip the prefix
  //off the name and get the command from what's left.
  string prefix="faodel-";
  string exe_name = string(basename(argv[0]));
  if((exe_name != "faodel") && (faodel::StringBeginsWith(exe_name, prefix))) {
    if(argc<1) return dumpHelp("");
    cmd = exe_name.erase(0, prefix.length());
    cmd = faodel::ToLowercase(cmd);
  }

  //Set some default env vars so we pick up dirman info right. We may change in option parsing
  setDefaultEnvVars();

  //Extract out simple args that are common to all commands
  for(int i = 1; i<argc; i++) {
    string sarg = string(argv[i]);
    if(     (sarg == "-v")  || (sarg == "--verbose"))           global_verbose_level = 1;
    else if((sarg == "-V")  || (sarg == "--very-verbose"))      global_verbose_level = 2;
    else if((sarg == "-VV") || (sarg == "--very-very-verbose")) global_verbose_level = 3;
    else if( /* no -d */       (sarg == "--dirman-node")) { //Note: -d is common for --dir, so don't use it
      i++;
      if(i>=argc) {
        cerr <<"Error: provided --dirman-node, but did not provide a node id\n";
        return -1;
      }
      //Change env vars so this overrides.. this does not override anything in config file
      setenv("FAODEL_DIRMAN_ROOT_NODE", argv[i], 1);
      unsetenv("FAODEL_DIRMAN_ROOT_NODE_FILE"); //dirman would look for this first. Remove it.
    }
    else if(cmd.empty()) cmd = faodel::ToLowercase(sarg); //This is our command
    else args.push_back(argv[i]); //This is an arg
  }

  if(cmd.empty())  {
    cout<<"No command found.\n";
    return dumpHelp("");
  }

  try {

    faodel::Configuration config;
    rc=ENOENT;

    //Convert starts and stops into standard service-action format
    if(((cmd=="start") || (cmd=="stop")) && (!args.empty())) {
      string service = faodel::ToLowercase(args[0]);
      args.erase(args.begin());
      cmd = service + "-" + cmd; //eg dirman-start
    }

    //Figure out which command this is and process it
    if(rc==ENOENT) rc = checkAllInOneCommands(cmd, args);
    if(rc==ENOENT) rc = checkBuildCommands(cmd, args);
    if(rc==ENOENT) rc = checkConfigCommands(cmd, args);
    if(rc==ENOENT) rc = checkWhookieClientCommands(cmd, args);
    if(rc==ENOENT) rc = checkDirmanCommands(cmd, args);
    if(rc==ENOENT) rc = checkResourceCommands(cmd, args);
    if(rc==ENOENT) rc = checkKelpieServerCommands(cmd, args);
    if(rc==ENOENT) rc = checkKelpieClientCommands(cmd, args);
    if(rc==ENOENT) rc = checkKelpieBlastCommands(cmd, args);
    if(rc==ENOENT) rc = checkPlayCommands(cmd, args);


    //Help menus
    if((rc==ENOENT) && (cmd=="help")){
      rc=dumpHelp( (args.empty())? "" : args[0]); //normal exit
    }

    if(rc==ENOENT) {
      cout<<"No valid command found..?\n";
      dumpHelp(""); //no valid command found
    }

  } catch(const std::exception &e) {
    cout <<"Caught std exception\n" << e.what() <<endl;
    faodel::bootstrap::Finish();

  } catch(...) {
    cout<<"Caught exception\n";
  }


  return rc;

}
