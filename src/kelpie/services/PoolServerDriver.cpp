// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <vector>

#include <mpi.h>

#include "faodel-common/Common.hh"
#include "whookie/Whookie.hh"
#include "whookie/Server.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "faodel-services/MPISyncStart.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/services/PoolServerDriver.hh"

using namespace std;

namespace kelpie {


string PoolServerDriver::config_string = R"EOF(
mpisyncstart.enable true

dirman.type centralized
dirman.root_node_mpi 0

)EOF";

PoolServerDriver::PoolServerDriver(int argc, char **argv)
        : op_desc("options"),
          argc(argc), argv(argv) {}

PoolServerDriver::~PoolServerDriver() {}

void PoolServerDriver::command_line_options() {

  op_desc.add_options()
          ("help,h", "This help menu.")
          ("url,u", opts::value<std::string>(&pool_url_string)->default_value("dht:/dht"), "Resource URL")
          ("pool-info,m", opts::value<std::string>(&pool_info)->default_value("Default Pool"), "Pool info string")

          ("iom-name,i",  opts::value<std::string>(&iom_name)->default_value(""), "IOM Name")
          ("iom-dir,d",   opts::value<std::string>(&iom_dir)->default_value(""),  "IOM Storage Directory")
          ("iom-type,t",  opts::value<std::string>(&iom_type)->default_value(""), "IOM Type")

          ("dirman-file,o", opts::value<std::string>(&root_id_filename)->default_value(""), "Name of file to record the dirman root in");

  opts::store(opts::parse_command_line(argc, argv, op_desc), op_map);

  if(op_map.count("help")){
    cout <<"kelpie-server: A standalone MPI job to house Kelpie pools\n"
         <<" options:\n"
         <<"   -u|--url         <pool url>   : The name of the resource (eg dht:/my/dht1)\n"
         <<"   -m|--pool-info   <info string>: Optional description for this pool\n"
         <<"\n"
         <<"   -i|--iom-name    <iom name>   : Name for the iom\n"
         <<"   -d|--iom-dir     <file path>  : Directory for pool data (eg: ./faodel_data)\n"
         <<"   -t|--iom-type    <pio>        : The iom driver for storing data\n"
         <<"\n"
         <<"   -o|--dirman-file <filename>   : Store Dirman root to a file\n"
         <<"\n";
    exit(0);
  }

  opts::notify(op_map);
}


void PoolServerDriver::start_dirman() {


  config.Append(config_string);

  //See if we have an iom to associate with this pool
  if(iom_dir!="") {
    if(iom_name=="") iom_name="stock_iom";
    if(iom_type=="") iom_type="PosixIndividualObjects";

    config.Append("iom."+iom_name+".path",iom_dir);
    config.Append("iom."+iom_name+".type",iom_type);
    pool_url_string += "&iom="+iom_name;

    config.Append("default.ioms", iom_name);
  }

  //See if we have a description to add
  if(pool_info !=""){
    pool_url_string+="&info="+faodel::MakePunycode(pool_info);
  }

  //Plug the resource in. Assume all ranks in this job are going
  //to be part of it and use mpisync to fill in the ranks
  config.Append("dirman.resources_mpi[]", pool_url_string+" ALL");

  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

  faodel::mpisyncstart::bootstrap();
  faodel::bootstrap::Start(config, kelpie::bootstrap);


  whookie_killswitch();

}

void PoolServerDriver::whookie_killswitch() {
  whookie::Server::registerHook("/killme",
                                [&](const std::map<std::string, std::string> &args, std::stringstream &results) {
                                    keep_going = false;
                                }
  );
}

void PoolServerDriver::stop_dirman() {
  faodel::DirectoryInfo dirinfo(pool_url_string, pool_info);
  if(mpi_rank == dirroot_rank_) {
    dirman::GetRemoteDirectoryInfo(faodel::ResourceURL(pool_url_string), &dirinfo);
    for(auto &&name_node : dirinfo.members)
      if(name_node.name not_eq "root")
        whookie::retrieveData(name_node.node, "/killme", nullptr);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  faodel::bootstrap::Finish();
  MPI_Finalize();
}


int PoolServerDriver::run() {
  command_line_options();

  start_dirman();

  keep_going = true;

  while(keep_going)
    sleep(5);

  stop_dirman();

  return 0;
}

}
