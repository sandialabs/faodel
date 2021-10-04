// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <fstream>
#include <chrono>
#include <thread>

#include "faodelConfig.h"

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#include "faodel-services/MPISyncStart.hh"
#endif

#include "faodel-common/StringHelpers.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "kelpie/pools/ResultCollector.hh"

#include "kelpie_client.hh"
#include "resource_client.hh"

#include "PlayAction.hh"
#include "faodel_cli.hh"


using namespace std;
using namespace faodel;


int playMain(const vector<string> &args);



bool dumpHelpPlay(std::string subcommand) {
  string help_play[5] = {
          "play-script", "play", "script", "Execute commands specified by a script",
          R"(
Play a series of commands that setup the FAODEL environment. A script may
contain both configuration info and (most) actions that are part of the
faodel tool. The following is a brief example that shows the basic format.
More examples can be found in faodel/examples/faodel-cli/playback-scripts.

# Hello world play script
config bootstrap.debug true                    # Turn on some debug messages
config dirman.root_node_mpi 0                  # Set first node as dirman root
config dirman.resources_mpi[] dht:/my/dht ALL  # Create a pool on all ranks

set pool /my/dht                               # Set default pool
set rank 0                                     # Set default rank

barrier                                        # Do an mpi barrier
rlist -r 0 /my/dht                             # Rank 0 lists info for dht

barrier                                        # Do an mpi barrier
kput -D 1k -k object1                          # Rank 0 writes 1 KB object
kput -p local:[abc] -D 1k -k object2           # Rank 0 write to local pool
klist -p /my/dht -k *                          # Show everything in dht
kinfo -p /my/dht -k object1                    # Get more info on object1

barrier
ksave -d ./tmp -k object1                      # Save an object to ldo file
kload -d ./tmp -k object1 -p local:[abc]       # Load and write to new pool
klist -p local:[abc] *                         # Show all local items

print ...And now to grab a file and display..  # Print some text
kput --file /etc/profile -k profile_file       # Send a plain file to pool
kget -k profile_file                           # Grab file and display
barrier


)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_play);
  return found;

}

int checkPlayCommands(const std::string &cmd, const vector<string> &args) {
  if( (cmd=="play-script") || (cmd == "play")) return playMain(args);
  return ENOENT;
}


void playInit(faodel::Configuration *config) {

  //See if dirman details are specified
  config->AppendFromReferences();


  //See if dirman is configured.
  if(!config->Contains("dirman.type")) {
    config->Append("dirman.type", "centralized");
  }
  if(!( config->Contains("dirman.host_root") ||
        config->Contains("dirman.root_node") ||
        config->Contains("dirman.root_node_mpi") )){
    config->Append("dirman.host_root","true");
  }

  modifyConfigLogging(config,
          {"kelpie","whookie"},
          {"opbox", "dirman"});

  dbg("Starting config:\n"+config->str());
#ifdef Faodel_ENABLE_MPI_SUPPORT
  faodel::mpisyncstart::bootstrap();
#endif
  faodel::bootstrap::Start(*config, kelpie::bootstrap);

}

void playFinish() {
  if(faodel::bootstrap::IsStarted()) {
    faodel::bootstrap::Finish();
  }
}

vector<string> playExtractRankInfoFromArgs(const vector<string> &args, string *my_rank) {

  //See if there's an override on the faodel command that specifies our rank
  string tmp_my_rank;
  vector<string> my_args;
  for(int i=0; i<args.size(); i++) {
    if(args[i] == "-r") {
      if(i+1>=args.size()) {
        cerr << "The -r rank option did not specify a value?\n";
        exit(-1);
      }
      i++;
      tmp_my_rank=args[i];

    } else {
      my_args.push_back(args[i]);
    }
  }

  //Normal case: we check mpi and use that value
  #ifdef Faodel_ENABLE_MPI_SUPPORT
  if(tmp_my_rank.empty()) {
    int mpi_rank;
    MPI_Init(nullptr, nullptr);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    tmp_my_rank = std::to_string(mpi_rank);
  }
  #endif

  *my_rank = tmp_my_rank;
  return my_args;
}

vector<PlayAction> playParseScripts(const vector<string> &args, faodel::Configuration *config) {

  vector<PlayAction> results; //actions to pass back
  string default_pool;
  bool needs_iomdir_defined=false;

  //Grab default pool from env
  char *env_pool = getenv("FAODEL_POOL");
  if(env_pool !=nullptr) {
    default_pool = string(env_pool);
  }

  string my_rank, default_rank; //my_rank is what user or mpi told us, default_rank is what "set rank" said to use
  vector<string> my_args = playExtractRankInfoFromArgs(args, &my_rank);
  default_rank=my_rank;

  //Parse all the files, line by line
  for(auto &fn : my_args) {
    dbg("Parsing file "+fn);
    ifstream ifs(fn);
    if(!ifs.is_open()) {
      cerr <<"Could not open script "<<fn<<endl;
      exit(-1);
    }

    string line;
    int line_num=0;
    bool hit_exit=false;
    while (std::getline(ifs, line)) {
      line_num++;
      std::istringstream iss(line);

      //First see if this line is a config string. If so, pass into out config
      if(StringBeginsWith(line,"config ")) {
        config->Append(line.substr(6));
        continue;
      }
      //Remove comments. Skip out if nothing left
      auto cmd_comment = faodel::Split(line, '#', false);
      if(cmd_comment.empty()) continue;

      //Split multiple commands stacked on one line with ';'
      auto commands = faodel::Split(cmd_comment[0],';', true);
      string file_tag = fn+":"+std::to_string(line_num);

      //Handle each command
      for(auto &cmd : commands) {

        PlayAction play_action;
        int rc = play_action.parseCommandLine(my_rank, &default_pool, &default_rank, file_tag, cmd);
        if(rc==ENOENT) continue; //No data - skip
        if(rc==EAGAIN) { hit_exit=true; break; } //got exit command
        if(rc==EINVAL) { //Parse error - bail
          cerr<<"Parse Error: "<<play_action.error_message<<"\n"
              <<play_action.filename_line<<"\t"<<line<<endl;
          exit(-1);
        }
        //Determine if we have any local iom actions
        needs_iomdir_defined |= ((play_action.command=="kload") || (play_action.command=="kstore"));
        results.push_back(play_action);
      }
      if(hit_exit) break; //got exit command, stop reading lines
    }

  }

  return results;
}

void accessPool(const string &pool_name, string *current_pool_name, kelpie::Pool *current_pool) {
  if(pool_name == *current_pool_name) return;
  dbg("Connecting to pool "+pool_name);
  *current_pool_name = pool_name;
  *current_pool = kelpie::Connect(pool_name);
  string err;
  current_pool->ValidOrDie();
  dbg("Connected");
}

vector<kelpie::Key> makeKeys(const vector<string> &key_strings) {
  vector<kelpie::Key> keys;
  for(auto &ks : key_strings) {
    auto tokens = faodel::Split(ks,'|');
    if((tokens.size()>2) || tokens.empty()) {
      cerr <<"Could not parse key: "<<ks<<endl;
      continue;
    }
    string k2;
    if(tokens.size()>1) k2 = tokens[1];
    keys.emplace_back(kelpie::Key(tokens[0], k2));
  }
  return keys;
}

int playMain(const vector<string> &args){

  int rc;
  dbg("Starting play");
  faodel::Configuration config;
  config.AppendFromReferences();

  //Build up the list of actions from the scripts we were given
  auto actions = playParseScripts(args, &config);
  dbg("Parsed actions and found "+to_string(actions.size())+" commands");

  //Start up faodel
  playInit(&config);

  string current_pool_name;
  kelpie::Pool current_pool;

  //Step through each action
  for(auto &action : actions) {

    if( action.kelpie_action.Valid() ) {
      dbg("Working on kelpie action "+action.kelpie_action.cmd);

      accessPool(action.kelpie_action.pool_name, &current_pool_name, &current_pool);
      rc = kelpieClient_Dispatch(current_pool, config, action.kelpie_action);

    } else if( action.resource_action.Valid() ) {
      dbg("Working on resource action "+action.resource_action.cmd);
      rc = resourceClient_Dispatch(action.resource_action);

    } else if(action.command == "print") { cout <<action.args[0]<<endl;
    } else if(action.command == "barrier") {
      #ifdef Faodel_ENABLE_MPI_SUPPORT
        MPI_Barrier(MPI_COMM_WORLD);
      #endif

    } else if(action.command == "delayfor") {
      uint64_t delay;
      StringToTimeUS(&delay, action.args[1]);
      dbg("Delay for "+std::to_string(delay/(1000*1000))+" seconds");
      std::this_thread::sleep_for(std::chrono::microseconds(delay));

    } else {
      cerr<<"Unknown command '"<<action.command<<"'"<<endl;
    }
  }

  //Done
  playFinish();

  return 0;
}