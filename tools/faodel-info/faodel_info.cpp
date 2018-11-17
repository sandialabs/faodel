#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <lunasa/Lunasa.hh>

#include "faodelConfig.h"
#include "faodel-common/Common.hh"

#include "opbox/OpBox.hh"
#include "webhook/Server.hh"


void show_cmake_config(void);
void ib_sanity_check(void);


using namespace std;
using namespace faodel;



bool verbose = false;

void warn(string s){
  cerr << "\033[1;31m" << "Warning:" << ":\033[0m "<<s<<endl;
}


void show_Configuration() {

  Configuration config;

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
  if(verbose) {
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

void show_Webhook() {
  bool     running = webhook::Server::IsRunning();
  nodeid_t myid    = webhook::Server::GetNodeID();

  cout << "===========================Webhook Status=============================" << endl;
  if (running) {
      cout << " Webhook is running on " << myid.GetHttpLink() << endl;
      if(!myid.ValidIP()){
        warn("Webhook is not running with a valid IP. You may need to\n"
             "    edit your FAODEL configuration and set 'webhook.interfaces'\n"
             "    to a valid NIC for this platform. Use ifconfig or ip to\n"
             "    see a list of available NICs (eg eth0 or ib0)\n");
      }
  } else {
      cout << " Webhook is NOT running" << endl;
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
void sanity_check() {
#if NNTI_BUILD_IBVERBS
    ib_sanity_check();
#endif
}

void show_runtime(void) {

    show_Configuration();

    bootstrap::Start(Configuration(""), opbox::bootstrap);

    show_Common();
    show_Webhook();
    show_Lunasa();
    bootstrap::Finish();

    sanity_check();
}

int main(int argc, char **argv) {

    if((argc>1) && (string(argv[1])=="-v")) verbose=true;
    show_cmake_config();
    show_runtime();

    return 0;
}
