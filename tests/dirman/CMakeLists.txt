include_directories( mpi )


set(SERIAL_TEST_LIBS
  dirman
  GTest::GTest
  GTest::Main
  Boost::program_options
  Boost::serialization
)
set(MPI_TEST_LIBS ${SERIAL_TEST_LIBS})

add_serial_test( tb_dirman_corecentralized              unit  true )
add_serial_test( tb_dirman_directorycache               unit  true )
add_serial_test( tb_dirman_directoryownercache          unit  true )

if( Faodel_ENABLE_MPI_SUPPORT )
    add_mpi_test( mpi_dirman_centralized component 2 true )
    add_mpi_test( mpi_dirman_op_messages component 2 true )
    add_mpi_test( mpi_dirman_restart     component 2 true )
endif()
