#include <unistd.h>
#include <iostream>
#include <vector>

#include "faodelConfig.h"
#include "faodel-common/Configuration.hh"
#include "lunasa/Lunasa.hh"

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"
#include "whookie/Server.hh"

#include "faodel_cli.hh"


using namespace std;
using namespace faodel;



int configInfo(const vector<string> &args);


bool dumpHelpConfig(string subcommand) {
  string help_cinfo[5] ={
          "config-info", "cinfo", "", "Display the Configuration tools will use",
          R"(
When FAODEL tools start, they will load configuration data from a file
specified by the FAODEL_CONFIG environment variable. You can set a number of
runtime parameters with this configuration (eg, debug levels, pre-defined
resources, and services that should run on specific nodes). This tool will
dump out the configuration that FAODEL will start with.
)"
  };
  return dumpSpecificHelp(subcommand, help_cinfo);
}

int checkConfigCommands(const std::string &cmd, const vector<string> &args) {

  if((cmd == "config-info")      || (cmd == "cinfo")) return configInfo(args);

  //No command
  return ENOENT;
}


void show_Configuration() {

  faodel::Configuration config;

  std::stringstream ss;
  ss << "===================Faodel Configuration Variable======================" << endl;

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
    ss << "=============Faodel Configuration Object (Pre-Append)=================" << endl;
    config.sstr(ss, 0, 0);
    ss << "\n";
  }
  ss << "====================Faodel Configuration Object=======================" << endl;

  config.AppendFromReferences();
  config.sstr(ss, 0, 0);

  cout << ss.str();
  cout << "======================================================================" << endl;
  fprintf(stdout, "\n");
}

void show_Common() {

  cout << "============================Common Status==============================" << endl;
  cout << faodel::MutexWrapperCompileTimeInfo();

}

void show_Whookie() {
  bool     running = whookie::Server::IsRunning();
  nodeid_t myid    = whookie::Server::GetNodeID();

  cout << "===========================Whookie Status=============================" << endl;
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
  cout << "======================================================================" << endl;
  fprintf(stdout, "\n");
}

void show_Lunasa() {

  cout << "===========================Lunasa Status==============================" << endl;

  cout <<" Lunasa Allocators: ";
  auto allocators = lunasa::AvailableAllocators();
  for(auto s : allocators)
    cout <<" "<<s;

  cout <<"\n Lunasa Cores:      ";
  auto cores = lunasa::AvailableCores();
  for(auto s : cores)
    cout <<" "<<s;
  cout <<endl;

}



int configInfo(const vector<string> &args) {

  show_Configuration();

  //Now try starting things up to see if the configs are usable
  bootstrap::Start(Configuration(""), opbox::bootstrap);
  show_Common();
  show_Whookie();
  show_Lunasa();
  bootstrap::Finish();

  return 0;
}