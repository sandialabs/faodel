// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// Kelpie Compute Example: Adding a User-Defined Function
//
// Users can create their own user-defined function and register them with
// servers. All you need to do is:
//
//  1. Crate a UDF that follows the fn_compute_t API specified in faodel/common/Types.hh
//  2. Register the function after init but before start
//  3. Call pool.Compute(key, function_name, args, ldos, return_ldo) to dispatch

#include <iostream>
#include <mpi.h>
#include <algorithm>

#include "faodel-common/Common.hh"
#include "faodel-services/MPISyncStart.hh"
#include "lunasa/common/Helpers.hh"
#include "kelpie/Kelpie.hh"

//The configuration used in this example
std::string default_config_string = R"EOF(

# Use mpisyncstart to create a DHT that is spread across all our nodes
dirman.type            centralized
dirman.root_node_mpi   0
dirman.resources_mpi[] dht:/myplace ALL

# Uncomment these options to get debug info for each component
bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.debug    true
)EOF";

using namespace std;
using namespace kelpie;

// This is an example of a user-defined function that simply cats all the objects together
// into a new string object that can be sent to the user
rc_t fn_udf_merge(faodel::bucket_t, const Key &key, const std::string &args,
                  std::map<Key, lunasa::DataObject> ldos, lunasa::DataObject *ext_ldo) {
  stringstream ss;
  for(auto &key_blob : ldos) {
    ss<<"Key: "<< key_blob.first.str()<<endl;
    ss<<lunasa::UnpackStringObject(key_blob.second) << endl;
  }
  *ext_ldo = lunasa::AllocateStringObject(ss.str());
  return KELPIE_OK;
}

// This user-defined function converts the merged text to all-caps.
rc_t fn_udf_caps(faodel::bucket_t, const Key &key, const std::string &args,
                 std::map<Key, lunasa::DataObject> ldos, lunasa::DataObject *ext_ldo) {

  stringstream ss;
  for(auto &key_blob : ldos) {
    ss<<"Key: "<< key_blob.first.str()<<endl;
    ss<< lunasa::UnpackStringObject(key_blob.second) << endl;
  }
  string s = ss.str();
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);
  *ext_ldo = lunasa::AllocateStringObject(s);
  return KELPIE_OK;
}



int main(int argc, char **argv) {

  //Initialize MPI
  int provided, mpi_rank,mpi_size;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  //Startup. Any UDF functions that are used have to be registered after Init but before Start.
  faodel::mpisyncstart::bootstrap();
  faodel::bootstrap::Init(faodel::Configuration(default_config_string), kelpie::bootstrap);
  kelpie::RegisterComputeFunction("my_merge", fn_udf_merge);
  kelpie::RegisterComputeFunction("my_caps", fn_udf_caps);
  faodel::bootstrap::Start();


  //Connect to the pool and write our rank's contribution to the row
  auto pool = kelpie::Connect("/myplace");
  kelpie::Key k1("myrow",to_string(mpi_rank));
  auto ldo1 = lunasa::AllocateStringObject("This is an object from rank "+
                                           to_string(mpi_rank)+string(mpi_size-mpi_rank,'!'));
  pool.Publish(k1, ldo1);

  //Wait for everyone to be done, then have rank issue 'pick' operations to get first/last items in row
  MPI_Barrier(MPI_COMM_WORLD);
  if(mpi_rank==0) {
    kelpie::Key key_myrow("myrow", "*"); //Look at all row entries
    lunasa::DataObject ldo2,ldo3;
    pool.Compute(key_myrow,"my_merge","", &ldo2);
    pool.Compute(key_myrow,"my_caps","", &ldo3);

    cout<<"Merged Item:\n"<< lunasa::UnpackStringObject(ldo2); //Should be a merge of all items
    cout<<"Caps Item:\n"<< lunasa::UnpackStringObject(ldo3); //Should be a merge of all items
  }

  MPI_Barrier(MPI_COMM_WORLD);
  faodel::bootstrap::Finish();
  MPI_Finalize();
  return 0;
}

