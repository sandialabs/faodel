
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set( SERIAL_TEST_LIBS
        common
        webhook
        services
        GTest::GTest
        GTest::Main
        )


set(MPI_TEST_LIBS
  MPI::MPI_CXX
  webhook
  services
  ${SERIAL_TEST_LIBS}
  )
# Wire in all the source directories so testing can run as if
# headers were dumped in flat directory
#
include_directories(
  ${CMAKE_SOURCE_DIR}/src/common
  ${CMAKE_BINARY_DIR}/src/common
  ${CMAKE_SOURCE_DIR}/src/services
  ${CMAKE_BINARY_DIR}/src/services )


add_serial_test( tb_service_backburner unit  true  )

if( Faodel_ENABLE_MPI_SUPPORT )
  add_mpi_test( mpi_service_mpisyncstart component 4 true)
endif()

