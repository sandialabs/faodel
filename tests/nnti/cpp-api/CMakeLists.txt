
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it

include_directories(
   ${CMAKE_SOURCE_DIR}/src/nnti
   ${CMAKE_BINARY_DIR}/src/nnti
)
SET(EXTRA_TEST_LIBS "-lz")


# Build the test support lib-------------------------------------------------
add_library(cpptestutils STATIC
            test_utils.hpp
            test_utils.cpp
)
set_target_properties(cpptestutils PROPERTIES LINKER_LANGUAGE  CXX )
target_link_libraries( cpptestutils PUBLIC nnti )
add_dependencies(cpptestutils generate-nnti_packable)
#----------------------------------------------------------------------------

set(SERIAL_TEST_LIBS 
  cpptestutils
  GTest::GTest
  GTest::Main
  ${EXTRA_TEST_LIBS}
)

set(MPI_TEST_LIBS 
  ${SERIAL_TEST_LIBS}
  MPI_CXX
)

#--------------+-----------------------------+----------------------------------+---------+
# Format:      |  Name                       |  Directory                       | Autorun |
#--------------+-----------------------------+----------------------------------+---------+
add_serial_test( NntiEqClassTest              .  false )
add_serial_test( NntiEventCallbackClassTest   .  false  )
add_serial_test( NntiFreelistClassTest        .  false )
add_serial_test( NntiLoggerClassTest          .  false  )
add_serial_test( NntiOpClassTest              .  false  )
add_serial_test( NntiPidClassTest             .  false  )
add_serial_test( NntiUrlClassTest             .  false  )
#add_serial_test( NntiWidClassTest             .  false  )

if( USE_MPI )
    add_mpi_test( AllocFreeTest              .  1  true  )
    add_mpi_test( AtomicOpTest               .  2  true  )
    add_mpi_test( CallbackStateMachineTest   .  2  true  )
    add_mpi_test( ConnectTest                .  2  true  )
    add_mpi_test( LongSendTest               .  2  true  )
    add_mpi_test( MultiPingPongTest          .  2  true  )
    add_mpi_test( NntiOpVectorTest           .  1  true  )
    add_mpi_test( PingPongCallbackTest       .  2  true  )
    add_mpi_test( QueueSendTest1             .  2  true  )
    add_mpi_test( QueueSendTest2             .  2  true  )
    add_mpi_test( RdmaOpTest                 .  2  true  )
    add_mpi_test( ShortSendTest              .  2  true  )
    add_mpi_test( UnexpectedCallbackTest     .  2  true  )
    add_mpi_test( UnexpectedLongSendTest     .  2  true  )
    add_mpi_test( UnexpectedSendTest         .  2  true  )
    add_mpi_test( UrlPidTest                 .  1  true  )
    add_mpi_test( ZeroCopySendTest           .  2  true  )
endif()

