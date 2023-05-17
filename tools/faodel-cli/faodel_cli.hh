// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_FAODEL_CLI_HH
#define FAODEL_FAODEL_CLI_HH

#include <string>
#include <vector>

#include "faodel-common/Configuration.hh"
#include "kelpie/Kelpie.hh"

extern int global_verbose_level;
extern int global_rank;

void info(const std::string &s);
void dbg(const std::string &s);
void warn(const std::string &s);

//Same, but only print if global_rank is zero
void info0(const std::string &s);
void dbg0(const std::string &s);
void warn0(const std::string &s);




// From faodel_cli.hh
bool dumpSpecificHelp(std::string subcommand, const std::string options[5]);

void modifyConfigLogging(faodel::Configuration *config,
        const std::vector<std::string> &basic_service_names,
        const std::vector<std::string> &very_verbose_service_names);


// Consolidated from all the different subcommands
bool dumpHelpAllInOne(std::string subcommand);
bool dumpHelpBuild(std::string subcommand);
bool dumpHelpConfig(std::string subcommand);
bool dumpHelpDirman(std::string subcommand);
bool dumpHelpResource(std::string subcommand);
bool dumpHelpKelpieServer(std::string subcommand);
bool dumpHelpKelpieClient(std::string subcommand);
bool dumpHelpKelpieBlast(std::string subcommand);
bool dumpHelpPlay(std::string subcommand);
bool dumpHelpWhookieClient(std::string subcommand);


int checkAllInOneCommands(      const std::string &cmd, const std::vector<std::string> &args);
int checkBuildCommands(         const std::string &cmd, const std::vector<std::string> &args);
int checkConfigCommands(        const std::string &cmd, const std::vector<std::string> &args);
int checkDirmanCommands(        const std::string &cmd, const std::vector<std::string> &args);
int checkResourceCommands(      const std::string &cmd, const std::vector<std::string> &args);
int checkKelpieServerCommands(  const std::string &cmd, const std::vector<std::string> &args);
int checkKelpieClientCommands(  const std::string &cmd, const std::vector<std::string> &args);
int checkKelpieBlastCommands(   const std::string &cmd, const std::vector<std::string> &args);
int checkPlayCommands(          const std::string &cmd, const std::vector<std::string> &args);
int checkWhookieClientCommands( const std::string &cmd, const std::vector<std::string> &args);


//Core functions that might be called by replay
int resourceDefine_internals(const std::vector<std::string> &args);
int resourceList_internals(const std::vector<std::string> &args);
int resourceDrop_internals(const std::vector <std::string> &args);

#endif //FAODEL_FAODEL_CLI_HH
