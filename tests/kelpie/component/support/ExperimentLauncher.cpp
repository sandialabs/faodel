// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <stdexcept>

#include <mpi.h>
#include <assert.h>
#include "gtest/gtest.h"

#include "faodel-services/MPISyncStart.hh"
#include "kelpie/Kelpie.hh"
#include "ExperimentLauncher.hh"
#include "ExperimentLauncher.hh"

using namespace std;
using namespace faodel;

namespace el_private {
map<int, fn_cmd_t> command_functions;
}
void el_registerCommand(int cmd, fn_cmd_t fn) {
  el_private::command_functions[cmd] = fn;
}


void el_bcastConfig(int cmd, string s){
  test_command_t msg;
  assert(s.size()<sizeof(msg.message));
  msg.command = cmd;
  msg.message_length = s.size();
  memcpy(&msg.message[0], s.c_str(), s.size());
  MPI_Bcast(&msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);
}

int el_bcastCommand(int cmd, string s){
  test_command_t msg;
  msg.command = cmd;
  msg.message_length = s.size();
  memcpy(&msg.message[0], s.c_str(), s.size());
  MPI_Bcast(&msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);

  if(cmd<0) return 0; //Built in commands don't do rc

  //Gather return codes
  int rc=0;
  int mpi_size;
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  int results[mpi_size];
  MPI_Allgather(&rc, sizeof(int), MPI_CHAR,
                results, sizeof(int), MPI_CHAR,
                MPI_COMM_WORLD);

  int sum=0;
  for(int i=0; i<mpi_size; i++){
    sum+=results[i];
  }
  return sum;
}




// All non-zero ranks just run in this loop, waiting for orders
// that tell them what to do new next. The commands are:
//
//   NEW_KELPIE_START: Here's a config, start it up
//   TEARDOWN: End of the config,  finish the bootstrap
//   KILL: all tests are done, exit out of targetloop
//
void el_targetLoop(){
  test_command_t msg;
  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  bool keep_going=true;
  do {
    MPI_Bcast( &msg, sizeof(msg), MPI_CHAR, 0, MPI_COMM_WORLD);
    switch(msg.command) {

      case CMD_NEW_KELPIE_START: {
        string cstr = string(msg.message, msg.message_length);
        mpisyncstart::bootstrap();
        bootstrap::Start(Configuration(cstr), kelpie::bootstrap);
        break;
      }
      case CMD_TEARDOWN: {
        //cout <<"Client "<<mpi_rank<<" Teardown\n";
        bootstrap::Finish();
        MPI_Barrier(MPI_COMM_WORLD);
        //cout<<"Client "<<mpi_rank<<" out of barrier\n";
        break;
      }
      case CMD_KILL:
        //cout <<"Client "<<mpi_rank<<" Kill\n";
        keep_going=false;
        break;

      default:
        //cout <<"Client "<<mpi_rank<<" Custom Command\n";
        auto it = el_private::command_functions.find(msg.command);
        if(it == el_private::command_functions.end())
          throw std::runtime_error("Unknown target loop command? id:" + to_string(msg.command));

        string cstr = string(msg.message, msg.message_length);
        int rc = it->second(cstr);

        //Gather return codes

        int results[mpi_size];
        MPI_Allgather(&rc, sizeof(int), MPI_CHAR,
                      results, sizeof(int), MPI_CHAR,
                      MPI_COMM_WORLD);
        //cout <<"Client "<<mpi_rank<<" Custom done\n";
    }

  } while(keep_going);
}

int el_defaultMain(int argc, char **argv) {

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  int mpi_rank, mpi_size, provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if(mpi_rank==0){
    cout << "ExperimentLauncher begins.\n";
    rc = RUN_ALL_TESTS();
    //cout <<"ExperimentLauncher completed all tests.\n";
    el_bcastCommand(CMD_KILL);
    sleep(1);
  } else {
    //cout <<"ExperimentLauncher Target Running\n";
    el_targetLoop();
    sleep(1);
  }

  MPI_Finalize();
  if(mpi_rank==0) {
    cout <<"All complete. Exiting\n";
    return rc;
  }
  return 0;
}
