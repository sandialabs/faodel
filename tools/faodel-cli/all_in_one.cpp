// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <exception>
#include <iostream>
#include <thread>
#include <iostream>

#include "faodelConfig.h"

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "whookie/client/Client.hh"
#include "whookie/Server.hh"
#include "kelpie/Kelpie.hh"
#include "dirman/DirMan.hh"
#include "faodel-services/MPISyncStart.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

int startAllInOne(const vector<string> &args);
bool aone_keepgoing = true;

bool dumpHelpAllInOne(string subcommand) {

  string help_aone[5] = {
          "all-in-one", "aone", "<urls>", "Start nodes w/ dirman and pools",
          R"(
The all-in-one option launches an mpi job that includes a dirman server, a
collection of kelpie servers (one per rank), and any pool settings you've
defined in either your configuration or the command line.
Example:

  mpirun -N 4 faodel aone  "dht:/x ALL" "rft:/y 0-middle" "dht:/z 2"
  # Use 4 nodes with
  #    "dht:/x ALL"       dht named /x on all four ranks
  #    "dft:/y 0-middle"  rft named /y on second half of ranks
  #    "dht:/z 2"         dht named /z just on rank 2

)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_aone);

  return found;
}

int checkAllInOneCommands(const std::string &cmd, const vector<string> &args) {
  if(     (cmd == "all-in-one")    || (cmd == "aone")) return startAllInOne(args);
  //No command
  return ENOENT;
}

void KillAoneHook() {
  info("Kelpie received shutdown request");
  aone_keepgoing=false;
}

int startAllInOne(const vector<string> &args) {

  faodel::Configuration config;

#ifdef Faodel_ENABLE_MPI_SUPPORT
  //Create the dirman info
  int mpi_rank, mpi_size;
  MPI_Init(0, nullptr);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  faodel::mpisyncstart::bootstrap();
  config.Append("mpisyncstart.enable", "true");
  config.Append("dirman.root_node_mpi", "0");
  for(auto v: args) {
    config.Append("dirman.resources_mpi[]", v);
  }
#else
  //No mpi.. just look busy
  config.Append("dirman.host_root", "true");
#endif

  //Make sure we're using dirman
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") config.Append("dirman.type", "centralized");

  config.Append("whookie.app_name", "All-in-One Server");

  //Dump our id to a file
  string root_write_file;
  if((!config.Contains("dirman.write_root.file")) || (!root_write_file.empty())) {
    if(root_write_file.empty()){
      root_write_file = "./.faodel-dirman";
    }
    config.Append("dirman.write_root.file",  root_write_file);
  }


  //Modify for debugging settings
  modifyConfigLogging(&config,
                      {"kelpie", "whookie", "mpisyncstart"},
                      {"opbox",  "dirman"});

  aone_keepgoing = true;

  //Startup in a way that adds a shutdown hook
  faodel::bootstrap::Init(config, kelpie::bootstrap);
  whookie::Server::registerHook("/dirman/shutdown", [] (const map<string,string> &args, stringstream &results) {
    KillAoneHook();
  });
  faodel::bootstrap::Start();


  //Wait for someone to call our shutdown service
  do {
    this_thread::sleep_for(chrono::seconds(3));
  } while(aone_keepgoing);

  faodel::bootstrap::Finish();

#ifdef Faodel_ENABLE_MPI_SUPPORT
  MPI_Finalize();
#endif
     
  return 0;
}

