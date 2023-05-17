// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <map>
#include <getopt.h>

#include "kelpie/Kelpie.hh"

#include "Job.hh"
#include "JobKeys.hh"
#include "JobLocalPool.hh"
#include "JobMemoryAlloc.hh"
#include "JobWebClient.hh"
#include "JobSerdes.hh"

using namespace std;

string default_config = R"EOF(
dirman.host_root true
net.transport.name mpi

#localpool.debug true

)EOF";

void dumpHelp() {

  cout << R"(
faodel-stress <options>

  options:
   -n num_workers    : Number of threads to use in each test (default: 1)
   -t work_duration  : Amount of time to run each test (default: 5s)
   -f test1,test2... : Filter down the tests to run (default: all)

   -x                : Generate tabular output (tab-separated)
   -v/-V             : Turn on verbose/very-verbose logging
   -l                : List all the available tests and exit

This program contains a set of simple stress tests to see how quickly a
system can complete common faodel operations. These bogus numbers help users
compare different platforms or compile options.

Note: a filter can either take a 'category:all' form (eg, 'serdes:all') or
      the name of individual tests (eg, 'PutGetDrop-1D,GenSort-Long2DKey').

)";
}

#include <cassert>
int main(int argc, char **argv) {

  int num_threads=1;
  int verbose_level=0;
  bool list_tests=false;
  bool dump_tsv=false;
  string duration="5s";
  string test_names="all";

  faodel::Configuration config(default_config);
  config.AppendIfUnset("faodel-stress.time_limit", duration);
  config.AppendIfUnset("faodel-stress.num_threads", to_string(num_threads));

  int c;
  while((c=getopt(argc,argv,"n:t:f:lxvVh")) != -1){
    switch(c){
      case 'n': num_threads = atoi(optarg); config.Append("faodel-stress.num_threads", to_string(num_threads)); break;
      case 't': duration = string(optarg);
                if((!duration.empty()) && (isdigit(duration[duration.size()-1])))
                    duration=duration+"s";
                config.Append("faodel-stress.time_limit", duration);
                break;
      case 'f': test_names = string(optarg); break;
      case 'l': list_tests = true; break;
      case 'x': dump_tsv = true; break;
      case 'v': verbose_level=1; break;
      case 'V': verbose_level=2; break;
      case 'h': dumpHelp(); exit(0); break;
      default:
        cout << "Unknown option '-"<<c<<"'\n";
        dumpHelp();
        exit(-1);
    }
  }

  //Dump output as tsv if requested
  if(dump_tsv) {
    config.Append("faodel-stress.dump_tsv true");
    cout <<"#Category\tTest\tWorkerThreads\tOpsCompleted\tTimeUS\tMOps\n";
  }

  //Make it easier to turn on all the debug info
  if(verbose_level==2) {
    config.Append("bootstrap.debug true");
    config.Append("kelpie.debug true");
    config.Append("lunasa.debug true");
  }

  //Create all the stressors
  vector<Job *> stressors;
  stressors.push_back(new JobKeys(config));
  stressors.push_back(new JobMemoryAlloc(config));
  stressors.push_back(new JobLocalPool(config));
  stressors.push_back(new JobSerdes(config));
  stressors.push_back(new JobWebClient(config));

  //List tests and exit if requested
  if(list_tests) {
    cout<<"Category       Test\n"
        <<"----------     ----------\n";
    for(auto *s : stressors) {
      s->DumpJobNames();
      delete s;
    }
    exit(0);
  }

  //Initialize/start faodel. Give each stress chance a chance to do pre-start ops
  faodel::bootstrap::Init(config, kelpie::bootstrap);
  for(auto s : stressors) {
    s->Init();
    if(verbose_level>0) s->ConfigureLoggingDebug(true);
  }
  faodel::bootstrap::Start();

  //Step through all stress test and run if needed
  for(auto s : stressors) {
    int num_jobs = s->Setup(test_names);
    if(num_jobs) {
      s->ExecuteAll();
      s->Teardown();
    }
  }
  for(auto *s : stressors)
    delete s;

  faodel::bootstrap::Finish();

}
