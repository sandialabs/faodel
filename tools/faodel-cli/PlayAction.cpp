// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <string>
#include <iostream>

#include "faodel-common/StringHelpers.hh"

#include "PlayAction.hh"

using namespace std;



int PlayAction::emsg(const string &condition) {
  error_message = condition;
  return EINVAL;
}


int PlayAction::parseCommandLine(const string &my_rank, string *default_pool, string *default_rank,
                                 const string &file_tag, const string &command_line) {


  
  //Remove empty lines
  vector <string> tokens = faodel::Split(command_line, ' ', true);
  if(tokens.empty()) return ENOENT; //Nothing to parse. Just skip

  //Set the args
  filename_line = file_tag;
  command = faodel::ToLowercase(tokens[0]);
  vector<string> kargs;
  if(tokens.size()>1) {
    kargs = {tokens.begin()+1, tokens.end()};
  }

  //Check for rank specification
  bool found_rank_option=false;
  for(int i=0; i<kargs.size(); i++) {
    if(kargs[i]=="-r") {
      if(i+1>=kargs.size()) return emsg("Didn't have enough arguments for -r flag?");
      if((my_rank.empty()) || (kargs[i+1]==my_rank)) {
        //Hit: just delete these two spots
        kargs.erase(kargs.begin()+i, kargs.begin()+i+2 );
        found_rank_option=true;
      } else {
        //Miss: just bail
        return ENOENT;
      }
      break;
    }
  }

  //Deal with exit
  if(command=="exit") {
    return EAGAIN;
  }


  //Handle set. These should just override the default settings for later actions
  if(command == "set") {
    if(kargs.size() != 2) return emsg("Set needs two arguments");
    if(kargs[0] == "pool") {
      *default_pool = kargs[1];
      return ENOENT;
    }
    if(kargs[0] == "rank") {
      *default_rank = kargs[1];
      return ENOENT;
    }
    return emsg("Did not recognize set '"+kargs[0]+"'");
  }


  //Everyone listens to barrier
  if(command == "barrier") {
    args.emplace_back("barrier");
    return 0;
  }


  //If not -r specified, see if our rank matches the default rank
  if((!found_rank_option) && (*default_rank != my_rank)) {
    return ENOENT;
  }

  //Allow printing of static text line
  if(command == "print") {
    args.emplace_back((command_line.length() > 5) ? command_line.substr(6) : "");
    return 0;
  }


  //Delays
  if((command == "delay") || (command=="delayfor")) {
    if(kargs.size() != 1) return emsg("delay needs exactly one argument");
    args.emplace_back("delayfor");
    args.emplace_back(kargs[0]);
    return 0;
  }

  //Check resource commands
  resource_action = ResourceAction(command);
  if(resource_action.Valid()) {
    resource_action.ParseArgs(kargs);
    if(kelpie_action.HasError()) return emsg(resource_action.error_message);
    return 0;
  }

  //Check kelpie commands
  kelpie_action = KelpieClientAction(command);
  if(kelpie_action.Valid()) {
    kelpie_action.ParseArgs(kargs, *default_pool);
    if(kelpie_action.HasError()) return emsg(kelpie_action.error_message);
    return 0;
  }


  return emsg("Unknown command?");
}

