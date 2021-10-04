
include_directories(
   ${CMAKE_SOURCE_DIR}/src
   ${CMAKE_BINARY_DIR}/src
)

# Build the test support lib-------------------------------------------------
if( Faodel_ENABLE_MPI_SUPPORT )
   add_library(cpptestutils
            test_utils.hpp
            test_utils.cpp
   )
   set_target_properties( cpptestutils PROPERTIES LINKER_LANGUAGE  CXX )
   target_link_libraries( cpptestutils PUBLIC nnti )
endif()
#----------------------------------------------------------------------------

set(SERIAL_TEST_LIBS
  cpptestutils
  ZLIB::ZLIB
  GTest::GTest
  GTest::Main
)

set(MPI_TEST_LIBS
  cpptestutils
  ${SERIAL_TEST_LIBS}
  MPI::MPI_CXX
)

#--------------+-----------------------------+----------------------------------+---------+
# Format:      |  Name                       |  Directory                       | Autorun |
#--------------+-----------------------------+----------------------------------+---------+
if( Faodel_HAVE_CRC32 )
    # Note: Previously all serial tests were here, but the support libary
    #       has MPI code so they all were moved into the mpi section

#add_serial_test( NntiWidClassTest             .  false  )

if( Faodel_ENABLE_MPI_SUPPORT )
    add_serial_test( NntiEventCallbackClassTest   .  false  )
    add_serial_test( NntiEqClassTest              .  false  )
    add_serial_test( NntiFreelistClassTest        .  false  )
    add_serial_test( NntiLoggerClassTest          .  false  )
    add_serial_test( NntiOpClassTest              .  false  )
    add_serial_test( NntiPidClassTest             .  false  )
    add_serial_test( NntiUrlClassTest             .  false  )

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
    add_mpi_test( RdmaAlignmentTest          .  2  true  )
    add_mpi_test( RdmaLengthTest             .  2  true  )
    add_mpi_test( RdmaOpTest                 .  2  true  )
    add_mpi_test( SelfConnectTest            .  1  true  )
    add_mpi_test( ShortSendTest              .  2  true  )
    add_mpi_test( UnexpectedCallbackTest     .  2  true  )
    add_mpi_test( UnexpectedLongSendTest     .  2  true  )
    add_mpi_test( UnexpectedSendTest         .  2  true  )
    add_mpi_test( UrlPidTest                 .  1  true  )
    add_mpi_test( ZeroCopySendTest           .  2  true  )
endif( Faodel_ENABLE_MPI_SUPPORT )
endif( Faodel_HAVE_CRC32 )
