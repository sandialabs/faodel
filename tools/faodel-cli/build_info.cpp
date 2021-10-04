// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <unistd.h>
#include <iostream>
#include <vector>

#include "lunasa/Lunasa.hh"

#include "faodelConfig.h"
#include "faodel-common/Common.hh"
#include "opbox/OpBox.hh"
#include "whookie/Server.hh"


#if USE_NNTI==1
#include "nnti/nntiConfig.h"
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

//Local prototypes
int buildInfo(const vector<string> &args);


bool dumpHelpBuild(string subcommand) {
  string help_binfo[5] = {
          "build-info", "binfo", "", "Display FAODEL build information",
          R"(
This command provides a way for you to get information about how the faodel
libraries were built (eg, was MPI used, what version of BOOST was used, which
network transports are enabled in the communication library, etc).
)"
  };

  return dumpSpecificHelp(subcommand, help_binfo);

}

int checkBuildCommands(const std::string &cmd, const vector<string> &args) {

  if((cmd == "build-info")      || (cmd == "binfo")) return buildInfo(args);

  //No command
  return ENOENT;
}


void show_found(int found, const char *target) {
  fprintf(stdout, "%20s: %s\n", target, (found ? "Found" : "Not Found"));
}

void show_found(int found, const char *target, const char *version) {
  fprintf(stdout, "%20s: %s (%s)\n", target, (found ? "Found" : "Not Found"), version);
}

void show_cmake_external_programs(void) {
  fprintf(stdout, "%20s: %s (%s)\n", "compiler", XSTR(CMAKE_CXX_COMPILER_ID), XSTR(CMAKE_CXX_COMPILER_VERSION));
  if(DOXYGEN_FOUND) {
    show_found(DOXYGEN_FOUND, "Doxygen", XSTR(DOXYGEN_VERSION));
  } else {
    show_found(DOXYGEN_FOUND, "Doxygen");
  }
  fprintf(stdout, "\n");
}

void show_cmake_tpls(void) {
  //show_found(Kokkos_FOUND, "Kokkos");
  show_found(LIBHIO_FOUND, "libhio");
  if(Boost_FOUND) {
    show_found(Boost_FOUND, "Boost", XSTR(Boost_VERSION));
  } else {
    show_found(Boost_FOUND, "Boost");
  }
  show_found(GTest_FOUND, "googletest");
  if(LIBFABRIC_FOUND) {
    show_found(LIBFABRIC_FOUND, "libfabric", XSTR(Libfabric_pc_VERSION));
  } else {
    show_found(LIBFABRIC_FOUND, "libfabric");
  }
  if(UGNI_FOUND) {
    show_found(UGNI_FOUND, "libugni", XSTR(UGNI_PC_VERSION));
  } else {
    show_found(UGNI_FOUND, "libugni");
  }
  show_found(DRC_FOUND, "CrayDRC");
  show_found(IBVerbs_FOUND, "libverbs");

  if(MPI_FOUND) {
    show_found(MPI_FOUND, "MPI", XSTR(MPI_C_VERSION));
  } else {
    show_found(MPI_FOUND, "MPI");
  }
  fprintf(stdout, "\n");
}

void show_cmake_common_config(void) {
  fprintf(stdout, "Faodel Common Config\n");
  fprintf(stdout, "%20s: %s\n", "Threading Model", XSTR(Faodel_THREADING_MODEL));
  fprintf(stdout, "\n");
}

void show_cmake_lunasa_config(void) {
  fprintf(stdout, "Lunasa Config\n");
#if Faodel_ENABLE_TCMALLOC
  fprintf(stdout, "    Building with tcmalloc from gperftools\n");
#endif
  fprintf(stdout, "\n");
}

void show_cmake_nnti_config(void) {
  fprintf(stdout, "NNTI Config\n");

#if USE_NNTI == 1

#if (NNTI_BUILD_IBVERBS)
#  if (NNTI_HAVE_VERBS_EXP_H)
  fprintf(stdout, "     Building the IBVerbs Transport with the libverbs expanded API (mlx4 or mlx5)\n");
#  else
  fprintf(stdout, "     Building the IBVerbs Transport with the libverbs standard API (mlx4 ONLY)\n");
#  endif
#else
#  if(NNTI_DISABLE_IBVERBS_TRANSPORT)
  fprintf(stdout, "     IBVerbs Transport explicitly disabled\n");
#  else
  fprintf(stdout, "     Not building the IBVerbs Transport\n");
#  endif
#endif

#if (NNTI_BUILD_UGNI)
  fprintf(stdout, "     Building the UGNI Transport\n");
#else
#  if(NNTI_DISABLE_UGNI_TRANSPORT)
  fprintf(stdout, "     UGNI Transport explicitly disabled\n");
#  else
  fprintf(stdout, "     Not building the UGNI Transport\n");
#  endif
#endif

#if (NNTI_BUILD_MPI)
  fprintf(stdout, "     Building the MPI Transport\n");
#else
#  if(NNTI_DISABLE_MPI_TRANSPORT)
  fprintf(stdout, "     MPI Transport explicitly disabled\n");
#  else
  fprintf(stdout, "     Not building the MPI Transport\n");
#  endif
#endif

#if (NNTI_USE_XDR)
  fprintf(stdout, "     Using XDR for serialization\n");
#elif (NNTI_USE_CEREAL)
  fprintf(stdout, "     Using Cereal for serialization\n");
#else
  fprintf(stdout, "     ERROR - Couldn't find a serialization library\n");
#endif

#elif USE_LIBFABRIC == 1

  fprintf(stdout, "     NNTI disabled.  Using libfabric instead.\n");

#else

  fprintf(stdout, "     NNTI disabled.  No network selected.\n");

#endif

  fprintf(stdout, "\n");
}

void show_cmake_opbox_config(void) {
  fprintf(stdout, "Opbox Config\n");
  fprintf(stdout, "%20s: %s\n", "Network Module", XSTR(Faodel_NETWORK_LIBRARY));
  fprintf(stdout, "\n");
}


#if NNTI_BUILD_IBVERBS
void ib_sanity_check();
#endif

int buildInfo(const vector<string> &args) {
  cout << "======================================================================" << endl;
  show_cmake_external_programs();
  show_cmake_tpls();
  show_cmake_common_config();
  show_cmake_lunasa_config();
  show_cmake_nnti_config();
  show_cmake_opbox_config();

  #if NNTI_BUILD_IBVERBS
  ib_sanity_check();
  #endif



  cout << "======================================================================" << endl;
  return 0;
}
