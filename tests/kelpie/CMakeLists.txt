
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/mpi )
SET(EXTRA_TEST_LIBS "-lz -ldl")

# Build the test support lib-------------------------------------------------
add_library(kelpie_mpi_test_support STATIC
  mpi/support/Globals.hh
  mpi/support/Globals.cpp
  )
target_link_libraries( kelpie_mpi_test_support
  kelpie
  ${FaodelNetlib_TARGETS}
  MPI_CXX )
set_target_properties( kelpie_mpi_test_support PROPERTIES LINKER_LANGUAGE  CXX )
#----------------------------------------------------------------------------

set(SERIAL_TEST_LIBS
  kelpie_mpi_test_support
  GTest::GTest
  GTest::Main
  Boost::serialization
  ${EXTRA_TEST_LIBS}
)
set(MPI_TEST_LIBS ${SERIAL_TEST_LIBS})

#--------------+-----------------------------+------------------------------------------+---------+
# Format:      |  Name                       |  Directory                               | Autorun |
#--------------+-----------------------------+------------------------------------------+---------+
add_serial_test(  tb_kelpie_key              unit            true )
add_serial_test(  tb_kelpie_localkv          unit            true )
add_serial_test(  tb_msg_direct              unit/messages   true )
add_serial_test(  tb_backburner_ldo          unit/backburner true )
add_serial_test(  tb_iom_pio_basic           unit/ioms       true )
add_serial_test(  tb_localpool_iompio        component/nonet true )

if( USE_MPI )
    add_mpi_test( mpi_kelpie_hello           mpi 2 true)
    add_mpi_test( mpi_kelpie_simple_peer     mpi 2 true)
    add_mpi_test( mpi_kelpie_dht             mpi 4 true)
endif()




