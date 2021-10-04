// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <array>
#include <iostream>
#include <unistd.h>
#include <vector>


#include "faodelConfig.h"
#include "faodel-common/Configuration.hh"
#include "lunasa/Lunasa.hh"

#include "faodel-common/Common.hh"
#include "whookie/Server.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/core/Singleton.hh"

#include "faodel_cli.hh"


using namespace std;
using namespace faodel;



int configInfo(const vector<string> &args);
int configOptions(const vector<string> &args);



bool dumpHelpConfig(string subcommand) {
  string help_cinfo[5] = {
          "config-info", "cinfo", "", "Display the Configuration tools will use",
          R"(
When FAODEL tools start, they will load configuration data from a file
specified by the FAODEL_CONFIG environment variable. You can set a number of
runtime parameters with this configuration (eg, debug levels, pre-defined
resources, and services that should run on specific nodes). This tool will
dump out the configuration that FAODEL will start with.
)"
  };
  string help_copt[5] = {
          "config-options", "copt", "", "List configuration options FAODEL inspects",
          R"(
This option dumps out all the configuration settings that were checked when
Kelpie is started. If this command fails, check to make sure your $FAODEL_CONFIG
file has the minimum state necessary for starting Kelpie (eg, remove any kelpie.ioms).
)"
  };
  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_cinfo);
  found |= dumpSpecificHelp(subcommand, help_copt);
  return found;
}

int checkConfigCommands(const std::string &cmd, const vector<string> &args) {

  if(     (cmd == "config-info")    || (cmd == "cinfo")) return configInfo(args);
  else if((cmd == "config-options") || (cmd == "copt"))  return configOptions(args);

  //No command
  return ENOENT;
}


void show_Configuration() {

  faodel::Configuration config;

  std::stringstream ss;
  ss << StringCenterTitle("Faodel Configuration Variable")<<endl;

  string ename;
  config.GetString(&ename,"config.additional_files.env_name.if_defined");
  ss << "Environment Variable Name:  "<< ename<<endl;

  const char* config_file = getenv(ename.c_str());
  ss << "Environment Variable Value: "<<((config_file) ? config_file : "(not set)")<<endl;
  ss << endl;

  if(!config_file)
    warn("Environment variable "+ename+" is not set. FAODEL\n"
                                       "          will not load any additional settings when it runs.");

  //Sometimes
  if(global_verbose_level) {
    ss << StringCenterTitle("Faodel Configuration Object (Pre-Append)") << endl;
    config.sstr(ss, 0, 0);
    ss << "\n";
  }
  ss << StringCenterTitle("Faodel Configuration Object") << endl;

  config.AppendFromReferences();
  config.sstr(ss, 0, 0);

  cout << ss.str() <<endl;
  //cout << StringCenterTitle("") << endl;
  //fprintf(stdout, "\n");
}

void show_Common() {

  cout << StringCenterTitle("Common Status") << endl;
  cout << faodel::MutexWrapperCompileTimeInfo() <<endl;

}

void show_Whookie() {
  bool     running = whookie::Server::IsRunning();
  nodeid_t myid    = whookie::Server::GetNodeID();

  cout << StringCenterTitle("Whookie Status") << endl;
  if (running) {
    cout << " Whookie is running on " << myid.GetHttpLink() << endl;
    if(!myid.ValidIP()){
      warn("Whookie is not running with a valid IP. You may need to\n"
           "    edit your FAODEL configuration and set 'whookie.interfaces'\n"
           "    to a valid NIC for this platform. Use ifconfig or ip to\n"
           "    see a list of available NICs (eg eth0 or ib0)\n");
    }
  } else {
    cout << " Whookie is NOT running" << endl;
  }
  cout << endl;
  //cout << StringCenterTitle("")<< endl;
  //fprintf(stdout, "\n");
}

void show_Lunasa() {

  cout << StringCenterTitle("Lunasa Status") << endl;

  cout <<" Lunasa Allocators: ";
  auto allocators = lunasa::AvailableAllocators();
  for(auto s : allocators)
    cout <<" "<<s;

  cout <<"\n Lunasa Cores:      ";
  auto cores = lunasa::AvailableCores();
  for(auto s : cores)
    cout <<" "<<s;
  cout <<endl <<endl;
}

void show_Kelpie() {
  cout << StringCenterTitle("Kelpie Status") << endl;

  cout << " Kelpie Core Types:  ";
  auto core_types = kelpie::internal::getCoreTypes();
  for(auto &name : core_types) {
    cout << " "<<name;
  }

  cout << "\n Kelpie Pools Types: ";
  auto pool_types = kelpie::internal::getPoolTypes();
  for(auto &name : pool_types) {
    cout << " "<<name;
  }

  cout << "\n Kelpie IOM Types:   ";
  auto iom_types = kelpie::internal::getIomTypes();
  for(auto &name : iom_types) {
    cout << " "<<name;
  }
  cout << "\n";


}

void show_ConfigOptions() {
  cout << StringCenterTitle("Configuration Options") << endl <<
R"(FAODEL services inspected Configuration for the following values when starting
their services. Not all of these may be actively used. Some configuration
settings may trigger additional options not listed here.)" <<endl <<endl;


  auto field_typevals = configlog::GetConfigOptions();

  //Plug in some values that don't show up from query
  field_typevals["node_role"] = {"string","default"};

  //Dirman hides unless you give it a root node, which will hang the startup. Just dump out
  field_typevals["dirman.host_root"] = {"bool","false"};
  field_typevals["dirman.write_root"] = {"string",""};
  field_typevals["dirman.root_node"] = {"string",""};
  field_typevals["dirman.root_node.file"] = {"string",""};
  field_typevals["dirman.root_node.file.env_name.if_defined"] = {"string",""};

  #ifdef Faodel_ENABLE_MPI_SUPPORT
  field_typevals["mpisyncstart.enable"] = {"bool","false"};
  #endif

  size_t longest1=0;
  size_t longest2=0;
  for(auto &f_tv : field_typevals) {
    if(f_tv.first.size() > longest1) longest1 = f_tv.first.size();
    if(f_tv.second[0].size() > longest2) longest2 = f_tv.second[0].size();
  }

  for(auto &f_tv : field_typevals) {
    cout.width(longest1); cout << std::left << f_tv.first <<" ";
    cout.width(longest2); cout << std::left << f_tv.second[0] << " ";
    cout << f_tv.second[1]<<endl;
  }
  cout <<endl;
}

Configuration configGetEmptyConfig() {
  #ifdef Faodel_ENABLE_MPI_SUPPORT
    //The mpi interface can be a little safer to start than other nics
    Configuration config("net.transport.name mpi", "");
  #else
    Configuration config("");
  #endif
  return config;
}
int configInfo(const vector<string> &args) {

  show_Configuration();

  Configuration config = configGetEmptyConfig();
  config.Append("dirman.host_root", "true"); //Ensure we start


  //Now try starting things up to see if the configs are usable
  bootstrap::Start(config, kelpie::bootstrap);
  show_Common();
  show_Whookie();
  show_Lunasa();
  show_Kelpie();
  bootstrap::Finish();

  return 0;
}

int configOptions(const vector<string> &args) {

  Configuration config = configGetEmptyConfig();
  config.Append("dirman.host_root", "true"); //Ensure we start

  bootstrap::Start(config, kelpie::bootstrap);
  show_ConfigOptions();
  bootstrap::Finish();

  return 0;
}