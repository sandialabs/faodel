// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_TESTS_KELPIE_GLOBALS_HH
#define FAODEL_TESTS_KELPIE_GLOBALS_HH

#include <mpi.h>

#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"
#include "whookie/Server.hh"
#include "kelpie/Kelpie.hh"


typedef struct {
  int op;
  int val;
} mpi_msg_t;

class Globals {

public:

  //Ctor only works after opbox started
  //Globals(initial_ctor_t dummy);
  Globals();
  ~Globals();

  int mpi_rank;
  int mpi_size;
  faodel::nodeid_t myid;
  faodel::nodeid_t dirman_root_nodeid;
  faodel::nodeid_t *nodes;
  opbox::net::peer_ptr_t *peers;
  int debug_level;



  void StartAll(int &argc, char **argv, faodel::Configuration &config, int minimum_ranks=2);
  void StopAll();
  void dump();
  void log(std::string s);
  void dbg(std::string s);

};


extern struct Globals G;

#endif // FAODEL_TESTS_KELPIE_GLOBALS_HH
