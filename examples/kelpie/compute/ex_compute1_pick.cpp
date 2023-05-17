// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// Kelpie Compute Example: Built-in 'pick' function
//
// Kelpie now provides a basic way to perform compute operations on the
// node that owns an object within a pool. Users may register a
// user-defined function that takes one or more key/blobs as input and
// produces a single DataObject output. When a user calls the
// pool.Compute() operation, the command is dispatched to the proper
// server, data is atomically retrieved out of its local in-memory store,
// the function is applied, and the result is returned.
//
// Some useful features:
//  1. User may specify a key with a column wildcard. For example, if
//     you ask for key("mything", "foo*"), you'll get all entries in
//     row mything that have a column name starting with foo.
//  2. User supply a string argument for the computation. The string
//     is brought to the remote node and supplied into the function.
//  3. Kelpie provides a built-in function called 'pick', which has
//     four options for arguments: 'first', 'last','smallest', and
//     biggest. This function returns the object that has a keyname
//     that is alphabetically first or last in the wildcard list,
//     or the object that is smallest/largest in size.
//
// In this example, we use the built-in 'pick' function to select an
// object from the row. This example uses mpisyncstart to simplify
// starting up a dht on all the nodes in the mpi job. Each node writes a
// string object to the same row (but different column). Rank 0 then
// uses the 'pick' function and a wildcard to read different objects.

#include <iostream>
#include <mpi.h>

#include "faodel-common/Common.hh"
#include "faodel-services/MPISyncStart.hh"
#include "lunasa/common/Helpers.hh"
#include "kelpie/Kelpie.hh"

//The configuration used in this example
std::string default_config_string = R"EOF(

# Use mpisyncstart to create a DHT that is spread across all our nodes
mpisyncstart.enable    true
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

int main(int argc, char **argv) {

  //Initialize MPI
  int provided, mpi_rank,mpi_size;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  //Start faodel. Use mpisyncstart to setup dirman and create a dht named /myplace
  faodel::mpisyncstart::bootstrap();
  faodel::bootstrap::Start(faodel::Configuration(default_config_string), kelpie::bootstrap);

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
    lunasa::DataObject ldo_first, ldo_last, ldo_smallest, ldo_largest;
    pool.Compute(key_myrow,"pick","first",    &ldo_first);
    pool.Compute(key_myrow,"pick","last",     &ldo_last);
    pool.Compute(key_myrow,"pick","smallest", &ldo_smallest);
    pool.Compute(key_myrow,"pick","largest",  &ldo_largest);

    cout<<"First item:    "<< lunasa::UnpackStringObject(ldo_first)    << endl; //Should be from rank 0
    cout<<"Last item:     "<< lunasa::UnpackStringObject(ldo_last)     << endl; //Should be from last rank
    cout<<"Smallest item: "<< lunasa::UnpackStringObject(ldo_smallest) << endl; //Should be from last rank
    cout<<"Largest item:  "<< lunasa::UnpackStringObject(ldo_largest)  << endl; //Should be from last rank
  }

  MPI_Barrier(MPI_COMM_WORLD);
  faodel::bootstrap::Finish();
  MPI_Finalize();
  return 0;
}

