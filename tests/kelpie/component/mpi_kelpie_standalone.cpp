// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//WIP: This test is a place holder for an example test that connects
//     to a standalone DHT

#include <iostream>
#include <fstream>
#include <memory>
#include <cstdlib>

#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "whookie/Whookie.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "kelpie/Kelpie.hh"
#include "whookie/Server.hh"

std::string default_config_string =
  R"EOF(

#bootstrap.debug true
#whookie.debug true
#opbox.debug true
#dirman.debug true
#kelpie.debug true

)EOF";




int main( int argc, char **argv ) {

  char *envp;
  std::string root_id_fname;
  std::string root_id_str;
  
  ::testing::InitGoogleTest( &argc, argv );

  faodel::Configuration config( default_config_string );
  config.AppendFromReferences();

  // Everybody needs to pick up the DHT root ID from the file
  if( ( envp = std::getenv( "FAODEL_DHT_ROOT_ID" )) == nullptr )
    root_id_fname = "FAODEL_DHT_ROOT_ID";
  else
    root_id_fname = envp;

  std::ifstream ifs( root_id_fname );
  ifs >> root_id_str;
  std::cout << "Root ID retrieved from FAODEL_DHT_ROOT_ID is '"<<root_id_str<< "'\n";

  if(root_id_str=="") {
    std::cerr<<"This test needs you to setup a standalone DHT that stores the id of its\n"
               "dirman root in a file specified by FAODEL_DHT_ROOT_ID. Either the file or\n"
               "the environment variable were not found. Aborting.\n";
    return -1;
  }

  // Everybody points to the root of the DHT (running elsewhere)
  config.Append( "dirman.root_node " + root_id_str );

  // We should now be able to bootstrap everyone and start making Kelpie API requests
  int provided;
  int mpi_rank;
  int mpi_comm_size;
  MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE, &provided );
  MPI_Comm_rank( MPI_COMM_WORLD, &mpi_rank );
  MPI_Comm_size( MPI_COMM_WORLD, &mpi_comm_size );
  
  faodel::bootstrap::Start( config, kelpie::bootstrap );
  
  kelpie::rc_t rc;
  auto test_dht = kelpie::Connect( "dht:/dht" );

  MPI_Barrier( MPI_COMM_WORLD );

  // Something basic
  lunasa::DataObject ldo_out, ldo_in;
  uint32_t *x = static_cast< uint32_t* >( ldo_out.GetDataPtr() );
  *x = mpi_rank + 1000;
  kelpie::Key k1( std::to_string( mpi_rank ), std::to_string( mpi_rank ) );

  rc = test_dht.Publish( k1, ldo_out, nullptr, nullptr );

  if( mpi_rank == 0 ) std::cerr << "Finished publish" << std::endl;

  int loc = (mpi_rank + 1) % mpi_comm_size;
  
  kelpie::Key k2( std::to_string( loc ), std::to_string( loc ) );
  rc = test_dht.Need( k2, &ldo_in );

  if( mpi_rank == 0 ) std::cerr << "Finished need" << std::endl;

  // Tell the DHT it can shut down
  if( mpi_rank == 0 ) {
    faodel::nodeid_t root_id( root_id_str );
    whookie::retrieveData( root_id, "/killme", nullptr );
  }

  MPI_Barrier( MPI_COMM_WORLD );
  faodel::bootstrap::Finish();
  MPI_Finalize();
  
  EXPECT_EQ( 0, 0 );
}
  
  

  
