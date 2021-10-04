// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_EXPERIMENTLAUNCHER_H
#define FAODEL_EXPERIMENTLAUNCHER_H

#include <functional>

const int CMD_NEW_KELPIE_START  =  -1; //Here's a config, launch with whookie+mpisync
const int CMD_TEARDOWN          =  -2;
const int CMD_KILL              =  -3;

using fn_cmd_t = std::function<int (const std::string &)>;

typedef struct {
  int command;
  int message_length;
  char message[4*1024];
} test_command_t;




void el_registerCommand(int cmd, fn_cmd_t fn);

void el_bcastConfig(int cmd, std::string s);
int el_bcastCommand(int cmd, std::string s="");


void el_targetLoop();

int el_defaultMain(int argc, char **argv);

#endif //FAODEL_EXPERIMENTLAUNCHER_H
