
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it

include_directories( mpi )
SET(EXTRA_TEST_LIBS "-lz -ldl")



# Build the test support lib-------------------------------------------------
add_library(opbox_mpi_test_support STATIC
  mpi/support/Globals.hh
  mpi/support/Globals.cpp
  )
target_link_libraries( opbox_mpi_test_support
  opbox
  ${FaodelNetlib_TARGETS}
  MPI_CXX )
set_target_properties( opbox_mpi_test_support PROPERTIES LINKER_LANGUAGE  CXX )
#----------------------------------------------------------------------------



set(SERIAL_TEST_LIBS
  opbox_mpi_test_support
  GTest::GTest
  GTest::Main
  Boost::program_options
  Boost::serialization
  ${EXTRA_TEST_LIBS}
)
set(MPI_TEST_LIBS ${SERIAL_TEST_LIBS})

#--------------+---------------------------------+-----------------------------------------------+---------+
# Format:      |  Name                           |  Directory                                    | Autorun |
#--------------+---------------------------------+-----------------------------------------------+---------+
add_serial_test( tb_OpboxTriggerOp                unit                  true )
add_serial_test( tb_OpboxSerdes                   unit                  true )
add_serial_test( tb_OpboxMessageHelpers           unit                  true )
add_serial_test( tb_OpboxOpArgs                   unit                  true )
add_serial_test( tb_OpDirManCentralized_messages  unit/dirman           true )

if( USE_MPI )

    add_mpi_test( mpi_opbox_hello          mpi  2  true )
    add_mpi_test( mpi_opbox_ping           mpi  2  true )
    add_mpi_test( mpi_dirman_centralized   mpi  2  true )


    add_mpi_test( tb_directoryCache        unit/dirman  1  true )
    add_mpi_test( tb_directoryOwnerCache   unit/dirman  1  true )
    add_mpi_test( tb_DirManCoreCentralized unit/dirman  1  true )

    add_mpi_test( tb_OpboxAtomicsTest      component 2  true )
    add_mpi_test( tb_OpboxConnectTest      component 2  true )
    add_mpi_test( tb_OpboxGetTest          component 2  true )
    add_mpi_test( tb_OpboxInitTest         component 1  true )
    add_mpi_test( tb_OpboxLongSendTest     component 2  true )
    add_mpi_test( tb_OpboxNewMessageTest   component 1  true )
    add_mpi_test( tb_OpboxPutTest          component 2  true )
    add_mpi_test( tb_OpboxRemoteBufferTest component 1  true )
    add_mpi_test( tb_OpboxSelfSendTest     component 1  true )
    add_mpi_test( tb_OpboxSendTest         component 2  true )

  #if(NETWORK_LIBRARY STREQUAL "libfabric")
  #  add_mpi_test( tb_OpboxOpPingFabTest    ${OPBOX_PROJECT_DIR}/tests/ops 2  true )
  #  add_mpi_test( tb_FabConnTest           ${OPBOX_PROJECT_DIR}/tests/ops 2  true )
  #endif()


endif()
