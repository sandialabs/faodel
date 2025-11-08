// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_PLAYACTION_HH
#define FAODEL_PLAYACTION_HH

#include "ResourceAction.hh"
#include "KelpieClientAction.hh"

class PlayAction {
public:
  PlayAction() {}

  std::string filename_line; //Parse info about where command came from
  std::string command;
  std::string error_message;

  std::vector <std::string> args; //generic info

  ResourceAction resource_action;   //resource-specific info
  KelpieClientAction kelpie_action; //kelpie-specific info


  int parseCommandLine(const std::string &my_rank,
                       std::string *default_pool,
                       std::string *default_rank,
                       const std::string &file_tag,
                       const std::string &command_line);

private:
  int emsg(const std::string &condition);

};

#endif //FAODEL_PLAYACTION_HH
