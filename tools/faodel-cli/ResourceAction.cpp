// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "ResourceAction.hh"

using namespace std;

ResourceAction::ResourceAction(const std::string &long_or_short_cmd) {

  //Go through all of the commands and plug in the shorthand
  vector<pair<string,string>> command_list = {
          {"resource-list",   "rlist"},
          {"resource-listr",  "rlistr"},
          {"resource-define", "rdef"},
          {"resource-drop",   "rdrop"}
          //Todo: resource-kill?
  };


  //Convert a command to its shorthand
  for(auto &big_little : command_list) {
    if(( long_or_short_cmd == big_little.first) || (long_or_short_cmd==big_little.second)) {
      cmd=big_little.second;
      break;
    }
  }

  //Tag this parse as an error
  if(cmd.empty()) {
    error_message="Command '"+long_or_short_cmd+"' not valid";
  } else {
    rank="0"; //Always default to run on first rank
  }

}

faodel::rc_t ResourceAction::ParseArgs(const vector<string> &args) {

  rargs = args;

  if(rargs.empty()) {
    if((cmd=="rlist") || (cmd=="rlistr")) rargs.emplace_back("/");
    else {
      return setError("Command '"+cmd+"' needs at least one argument");
    }
  }
  return 0;
}