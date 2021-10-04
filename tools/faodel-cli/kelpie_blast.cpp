// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <thread>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "faodel_cli.hh"
#include "KelpieBlastParams.hh"

using namespace std;
using namespace faodel;

int kelpieBlast(const vector<string> &args);

bool dumpHelpKelpieBlast(string subcommand) {
   string help_kblast[5] = {
          "kelpie-blast", "kblast", "", "Run MPI job to generate kelpie traffic",
          R"(
kelpie-blast Flags:
 -a/--async              : send many objects per timestep before blocking
 -m/--reuse-memory       : allocate LDOs in advance and reuse them
 -r/--rank-grouping      : ensure all of rank's data lands on same server
 -s/--skip-barrier       : skip the barrier that happens at start of cycle

kelpie-blast Options:
 -t/--timesteps x        : Run for x timesteps and then stop (default = 1)
 -T/--delay x            : Delay for x seconds between timesteps

 -o/--object-sizes x,y   : List of object sizes to publish each timestep

 -p/--external-pool pool : Name of an external pool to write (eg '/my/dht')
 -P/--internal-pool pool : Type & path of internal pool to write to
                           (eg 'dht:/tmp', 'local:/tmp' for disk, or 'dht:')


The kelpie-blast command provides a parallel data generator that can produce
a variety of traffic conditions. When you run as an mpi job, each rank will
follow a bulk-sync parallel flow where each rank sleeps, dumps a collection
of objects, and then does an optional barrier. Output is in a tab-separated
format that is easy to parse.

Output Columns:
 Step:    Which timestep this is for
 Rank:    Which rank this stat is for
 Gen:     Time (US) required to generate data being published
 Issue:   Time (US) to issue the publish operation. Only useful in async mode
 Pub:     Time (US) to issue publish and receive acknowledgement from target
 Gap:     Time (US) between when pub completes and rank gets out of barrier
 All:     Time (US) to generate, publish, complete, and get through barrier
 Bytes:   Total user bytes sent by this node for the timestep
 IssueBW: How fast the issue appeared to application, in MB/s
 PubBW:   How fast the publish w/ acknowledgement appeared, in MB/s

Examples:
 mpirun -n 4 faodel kblast -P local:/tmp -t 10  # Write 10 timesteps to /tmp
 mpirun -n 4 faodel kblast -P dht:/tmp -o 1k,2M,32  # Pub 3 objects/timestep
 mpirun -n 4 faodel kblast -p /my/pool            # Connect to external pool
)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_kblast);
  return found;
}

int checkKelpieBlastCommands(const std::string &cmd, const vector<string> &args) {
  if(  (cmd == "kelpie-blast") || (cmd=="kblast")) return kelpieBlast(args);
  return ENOENT;
}

uint64_t getTimeUS(std::chrono::time_point<std::chrono::high_resolution_clock> t) {
  return std::chrono::duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count();
}



uint64_t * KelpieBlastInitializeData(const KelpieBlastParams &p, std::vector<lunasa::DataObject> &ldos) {

  //Allocate initial ldos and scratch space for data
  for(int i=0; i<p.object_sizes_per_timestep.size(); i++) {
    ldos.push_back(lunasa::DataObject(p.object_sizes_per_timestep.at(i)));
  }
  uint64_t words = p.max_object_size/sizeof(uint64_t);
  if(p.max_object_size & 0x07) words++;
  auto *user_data = new uint64_t[words];
  for(uint64_t i=0; i<words; i++){
    user_data[i]=i;
  }
  return user_data;
}

void KelpieBlastGenerateData(const KelpieBlastParams &p, std::vector<lunasa::DataObject> &ldos, uint64_t *user_data) {

  if(!p.reuse_memory) {
    ldos.clear();
    dbg("Cleared ldos.. now adding");
    for(int i=0; i<p.object_sizes_per_timestep.size(); i++) {
      ldos.push_back( lunasa::DataObject(p.object_sizes_per_timestep.at(i)) );
    }
  }
  for(int i=0; i<p.object_sizes_per_timestep.size(); i++) {
    memcpy(ldos.at(i).GetDataPtr(), user_data, p.object_sizes_per_timestep.at(i));
  }
}

static std::chrono::time_point<std::chrono::high_resolution_clock>
KelpieBlastPublishData(const KelpieBlastParams &p, kelpie::Pool &pool, std::vector<lunasa::DataObject> &ldos, uint64_t *user_data) {


  static std::chrono::time_point<std::chrono::high_resolution_clock> issued_time;

  vector<kelpie::Key> keys;
  for(int i=0;i<p.object_sizes_per_timestep.size(); i++) {
    string k1,k2;
    k1 = (p.use_rft_keys) ? (std::to_string(p.mpi_rank)) : RandomString(p.k1_length);
    k2 = RandomString(p.k2_length);
    if(k1.size()<p.k1_length) {
      //To be fair, zero pad k1 if we're using rft keys
      k1 = std::string(p.k1_length - k1.size(), '0') + k1;
    }
    keys.push_back(kelpie::Key(k1,k2));
  }

  if(!p.async_pubs) {
    //Plain old synchronous publish
    for(int i=0; i<keys.size(); i++) {
      //cout <<"Rank "<<p.mpi_rank<<" sending key "<<keys.at(i).str()<<endl;
      pool.Publish(keys.at(i), ldos.at(i));
    }
    issued_time = std::chrono::high_resolution_clock::now(); //Not used, but here to keep things organized

  } else {
    //Async publish
    kelpie::ResultCollector results(p.object_sizes_per_timestep.size());
    for(int i=0; i<keys.size(); i++) {
      pool.Publish(keys.at(i), ldos.at(i), results);
    }
    issued_time = std::chrono::high_resolution_clock::now();
    results.Sync();
  }
  return issued_time;
}


int kelpieBlast(const vector<string> &args) {

  struct io_times_t {
    uint64_t generate;
    uint64_t issued;
    uint64_t publish;
    uint64_t gap;
    uint64_t all;
    uint64_t bytes;
  };

  char sep='\t';

  KelpieBlastParams p(args);
  if(!p.IsOk()) return 0;

  //First initialize rng! if not, all your timestep keys are the same
  srand(time(NULL)+100*p.mpi_rank);


  //Try connecting to out pool
  auto pool = kelpie::Connect(p.pool_name);
  string err;
  if(!pool.Valid(&err)) {
    cout <<"Error connecting to pool:\n"<<err<<endl;
    return -1;
  }

  //Dump info about the run
  if(p.mpi_rank==0) {
    p.dumpSettings( pool.GetDirectoryInfo() );
    cout << faodel::Join({"Step","Rank","Gen","Issue","Pub","Gap","All","Bytes","IssueBW","PubBW"},sep)<<"\n";
  }


  uint64_t *user_data;
  vector<lunasa::DataObject> ldos;
  user_data = KelpieBlastInitializeData(p,ldos);


  io_times_t iot;
  io_times_t io_times[p.mpi_size];


  for(p.timestep=0; p.timestep < p.num_timesteps; p.timestep++) {

    p.SleepForComputePhase();
    if(!p.no_barrier_before_generate)
      p.barrier();

    dbg0("Generating data");
    auto t1 = std::chrono::high_resolution_clock::now();
    KelpieBlastGenerateData(p,ldos, user_data);


    dbg0("Publishing data");
    auto t2 = std::chrono::high_resolution_clock::now();
    auto t_issued = KelpieBlastPublishData(p, pool, ldos, user_data);

    auto t3 = std::chrono::high_resolution_clock::now();
    dbg0("Published. Now send waiting");
    p.barrier();

    auto t4 = std::chrono::high_resolution_clock::now();

    iot.generate = getTimeUS(t2) - getTimeUS(t1);
    iot.issued = getTimeUS(t_issued) - getTimeUS(t2); //How long it took to issue all async messages
    iot.publish = getTimeUS(t3) - getTimeUS(t2); //How long it took for all messages to be issued and completed
    iot.gap = getTimeUS(t4) - getTimeUS(t3);
    iot.all = getTimeUS(t4) - getTimeUS(t1);
    iot.bytes = p.bytes_per_rank_step;

    if(0) {
      cout << "--->" << " " << p.mpi_rank << " "
           << iot.generate << " "
           << iot.publish << " "
           << iot.gap << " "
           << iot.all << " "
           << iot.bytes << " "
           << ((double) iot.bytes / (double) iot.publish)
           << "\n";
    }

    #ifdef Faodel_ENABLE_MPI_SUPPORT
      int rc = MPI_Gather(&iot, sizeof(io_times_t),  MPI_CHAR,
                        &(io_times[0]), sizeof(io_times_t), MPI_CHAR,
                        0, MPI_COMM_WORLD);
      if(rc!=0) {
        cerr<<"Gather gave bad rc?\n";
      }
    #endif

    if(p.mpi_rank==0) {
      for(int i = 0; i < p.mpi_size; i++) {
        cout << p.timestep << sep
             << i << sep
             << io_times[i].generate << sep
             << io_times[i].issued << sep
             << io_times[i].publish << sep
             << io_times[i].gap << sep
             << io_times[i].all << sep
             << io_times[i].bytes << sep
             << ((double) io_times[i].bytes / (double) io_times[i].issued) << sep
             << ((double) io_times[i].bytes / (double) io_times[i].publish)
             << "\n";
      }
    }
    p.barrier();

  }
  dbg0("Done");


  delete[] user_data;
  //~KelpieBlastParams should get called as we exit here


  return 0;
}
