
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set(SERIAL_TEST_LIBS
  whookie
  common
  GTest::GTest
  GTest::Main
  )

set(MPI_TEST_LIBS
  MPI::MPI_CXX
  ${SERIAL_TEST_LIBS}
  )
  

#--------------+-----------------------------+----------------------------------------+---------+
# Format:      |  Name                       |  Directory                             | Autorun |
#--------------+-----------------------------+----------------------------------------+---------+
add_serial_test(  tb_whookie_clientserver      component   true  )
add_serial_test(  simple_whookie_example       standalone  false )


if( Faodel_ENABLE_MPI_SUPPORT )
    add_mpi_test( mpi_whookie_multiprocess_threaded    component  2  false )
    add_mpi_test( mpi_whookie_restart                  component  2  false )
endif()
