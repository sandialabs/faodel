// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <unistd.h>
#include <assert.h>

#include "common/Common.hh"
#include "opbox/OpBox.hh"

#include "opbox/services/dirman/DirectoryManager.hh"


#include "Globals.hh"


//The Globals class just holds basic communication vars we use in these
//examples (ie mpi ranks, etc). It has a generic hook for starting/stopping
//all nodes in this mpi run to make the OpBox codes easier to understand.
Globals G;



std::string default_config_string = R"EOF(
# Select a transport to use for nnti (laptop tries ib if not forced to mpi)
nnti.transport.name   mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

# Put the 'master' node on a separate port so it won't get bumped around by
# the others on a single-node multi-rank run.
#
# note: node_role is set by Globals based on rank.
#
master.webhook.port   7777
server.webhook.port   1992

# Select the type of dirman to use. Currently we only have centralized, which
# just sticks all the directory info on one node (called root). We use roles
# to designate which node is actually the root.
dirman.type           centralized
dirman.root_role      master

# Turn these on if you want to see more debug messages
#bootstrap.debug           true
#webhook.debug             true
#opbox.debug               true
#dirman.debug              true
#dirman.cache.others.debug true
#dirman.cache.mine.debug   true

)EOF";

using namespace std;
using namespace faodel;
using namespace opbox;

//All the examples (simpler than dealing with headers)
void example1_create_and_fetch();
void example2_prepopulate_with_children();
void example3_remote_create();
void example4_dynamic_joining();
void example5_polling();

int main(int argc, char **argv){

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();

  G.StartAll(argc, argv, config); //

  example1_create_and_fetch();
  example2_prepopulate_with_children();
  example3_remote_create();
  example4_dynamic_joining();
  example5_polling();

  G.StopAll();

  return 0;
}
