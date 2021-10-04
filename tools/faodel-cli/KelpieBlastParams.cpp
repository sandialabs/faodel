// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <functional>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif


#include "faodel-common/Configuration.hh"
#include "faodel-common/ResourceURL.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/StringHelpers.hh"
#include "faodel-services/MPISyncStart.hh"

#include "kelpie/Kelpie.hh"

#include "faodel_cli.hh"
#include "KelpieBlastParams.hh"

using namespace std;

KelpieBlastParams::KelpieBlastParams(const std::vector<std::string> &args)
        : mpi_rank(0), mpi_size(1),
          timestep(0), num_timesteps(1), delay_between_timesteps_us(1000000),
          k1_length(8), k2_length(8),
          reuse_memory(false),
          async_pubs(false),
          use_rft_keys(false),
          no_barrier_before_generate(false),
          verbose(0), failed(false) {


  //Parse our args
  failed = (parseArgs(args) != 0);
  if(failed) return;


  faodel::Configuration config;
  if(!pool_external.empty()) {
    //External pool. Just get reference from user
    pool_name = pool_external;

  } else {
    //Internal pool. Parse string
    auto vals = faodel::Split(pool_internal,':',true);
    if((vals.size()<1) || (vals.size()>2)) {
      cerr << "Problem parsing internal pool '"<<pool_internal<<"'"<<endl;
      failed = true;
      return;
    }

    //Setup an iom if user provided a path
    string url_extra;
    if(vals.size()==2) {
      config.Append("kelpie.ioms",               "localdump");
      config.Append("kelpie.iom.localdump.type", "PosixIndividualObjects");
      config.Append("kelpie.iom.localdump.path", vals.at(1));
      url_extra="&iom=localdump";
    }

    #ifdef Faodel_ENABLE_MPI_SUPPORT
      //Create the dirman info
      config.Append("mpisyncstart.enable", "true");
      config.Append("dirman.root_node_mpi", "0");
      config.Append("dirman.resources_mpi[]", vals.at(0)+":/my/pool"+url_extra+" ALL");
    #endif
    pool_name="/my/pool";
  }

  if(verbose>1) {
    config.Append("kelpie.debug", "true");
    config.Append("bootstrap.debug", "true");
  }

  //Launch MPI
  #ifdef Faodel_ENABLE_MPI_SUPPORT
    MPI_Init(nullptr, nullptr);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    global_rank=mpi_rank;
    faodel::mpisyncstart::bootstrap();
  #endif

  //Launch FAODEL
  faodel::bootstrap::Start(config, kelpie::bootstrap);

}

KelpieBlastParams::~KelpieBlastParams() {

  if(!failed) {

    #ifdef Faodel_ENABLE_MPI_SUPPORT
      MPI_Barrier(MPI_COMM_WORLD);
      faodel::bootstrap::Finish();

      MPI_Barrier(MPI_COMM_WORLD);
      dbg0("Finalizing");
      MPI_Finalize();
    #else
      faodel::bootstrap::Finish();
    #endif
    dbg0("Exiting");

  }
}

void KelpieBlastParams::dumpSettings(faodel::DirectoryInfo dir) {
  if((failed)||(mpi_rank!=0)) return;

  cout << "# Runtime Configuration\n"
       << "#   mpi_size:                   "<<mpi_size << endl
       << "#   pool_name:                  "<<pool_name << endl
       << "#   pool_dirinfo_url:           "<<dir.url.GetFullURL() << endl
       << "#   pool_dirinfo_num_members:   "<<dir.members.size() << endl
       << "#   num_timesteps:              "<<num_timesteps << endl
       << "#   delay_between_timesteps_us: "<<delay_between_timesteps_us << endl
       << "#   object_sizes_per_timestep: ";
  for(auto x:object_sizes_per_timestep) {
    cout <<" "<<x;
  }
  cout << endl
       << "#   max_object_size:            "<<max_object_size << endl
       << "#   bytes_per_rank_step:        "<<bytes_per_rank_step << endl
       << "#   reuse_memory:               "<<reuse_memory << endl
       << "#   async_pubs:                 "<<async_pubs << endl
       << "#   use_rft_keys:               "<<use_rft_keys << endl
       << "#   no_barrier_before_generate: "<<no_barrier_before_generate << endl;

}

int KelpieBlastParams::parseArgs(const std::vector<std::string> &args) {

  string s_object_sizes;
  int rc=0;

  struct ArgInfo { string short_name; string long_name; bool has_argument; std::function<int(const string&)> lambda; };
  vector<ArgInfo> options {
          { "-a", "--async",         false, [=](const string &s) { this->async_pubs = true; return 0; } },
          { "-m", "--reuse-memory",  false, [=](const string &s) { this->reuse_memory = true; return 0; } },
          { "-r", "--rank-grouping", false, [=](const string &s) { this->use_rft_keys = true; return 0; } },
          { "-s", "--skip-barrier" , false, [=](const string &s) { this->no_barrier_before_generate = true; return 0; } },
          { "-t", "--timesteps",     true,  [=](const string &s) { return faodel::StringToUInt64(&this->num_timesteps, s); } },
          { "-T", "--delay",         true,  [=](const string &s) { return faodel::StringToTimeUS(&this->delay_between_timesteps_us, s); } },
          { "-o", "--objects",       true,  [=,&s_object_sizes](const string &s) { s_object_sizes = s; return 0; } },
          { "-p", "--external-pool", true,  [=](const string &s) { this->pool_external = s; return 0; } },
          { "-P", "--internal-pool", true,  [=](const string &s) { this->pool_internal = s; return 0; } }
  };
  for(int i=0; i<args.size(); i++) {
    bool found=false;
    for(auto &option : options) {
      if((option.short_name == args[i]) || (option.long_name == args[i])) {
        if(option.has_argument) {
          i++;
          if(i >= args.size()) {
            cerr << "Not enough arguments for " << option.short_name << "/" << option.long_name << endl;
            return -1;
          }
        }
        rc = option.lambda(args[i]);
        if(rc != 0) {
          cerr << "Problem parsing " << option.short_name << "/" << option.long_name << endl;
          return -1;
        }
        found = true;
        break;
      }
    }
    if(!found) {
      cerr <<"Unknown option "<<args[i]<<endl;
      return -1;
    }
  }


  //Verify values we were given

  if(num_timesteps<1) { rc=EINVAL; cerr<<"Timesteps must be greater than 0\n"; }

  //Verify we were given a pool
  if( !(pool_internal.empty() ^ pool_external.empty()) ) {
    cerr <<"You must specify either the external pool or the internal pool (but not both)\n";
    return EINVAL;
  }


  if(s_object_sizes.empty()){
    dbg0("No object sizes specified, using 1MB");
    object_sizes_per_timestep.push_back(1024*1024);
    bytes_per_rank_step=1024*1024;
    max_object_size=1024*1024;

  } else {
    auto svals = faodel::Split(s_object_sizes,',',true);
    max_object_size=bytes_per_rank_step=0;
    for(auto s: svals) {
      uint64_t x;
      rc = faodel::StringToUInt64(&x,s);
      if(rc) {
        cerr<<"Parse error with object sizes for '"<<s<<"'\n";
        return EINVAL;
      }
      object_sizes_per_timestep.push_back(x);
      bytes_per_rank_step += x;
      if(x>max_object_size) max_object_size=x;
    }
  }

  return 0;
}

void KelpieBlastParams::barrier() {
  #ifdef Faodel_ENABLE_MPI_SUPPORT
    MPI_Barrier(MPI_COMM_WORLD);
  #endif
}
