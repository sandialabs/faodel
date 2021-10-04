
include_directories( mpi )

# Build the test support lib-------------------------------------------------
if( Faodel_ENABLE_MPI_SUPPORT )
    add_library(opbox_mpi_test_support
        component/support/Globals.hh
        component/support/Globals.cpp
    )
    target_link_libraries( opbox_mpi_test_support
        opbox
        ${FaodelNetlib_TARGETS}
        MPI::MPI_CXX )
    set_target_properties( opbox_mpi_test_support PROPERTIES LINKER_LANGUAGE  CXX )
endif()

#----------------------------------------------------------------------------


set(SERIAL_TEST_LIBS
  opbox
  GTest::GTest
  GTest::Main
  Boost::program_options
  Boost::serialization
)

set(MPI_TEST_LIBS 
  opbox_mpi_test_support
  ZLIB::ZLIB
  GTest::GTest
  GTest::Main
  Boost::program_options
  Boost::serialization
)

#--------------+------------------------+-----------+-------+---------+
# Format:      |  Name                  | Directory | Ranks | Autorun |
#--------------+------------------------+-----------+-------+---------+
if( Faodel_HAVE_CRC32 )
if( Faodel_ENABLE_MPI_SUPPORT )

    add_mpi_test( mpi_opbox_triggerop       unit      1  true )
    add_mpi_test( mpi_opbox_message_helpers unit      1  true )

    add_mpi_test( mpi_opbox_net_init        unit      1  true )
    add_mpi_test( mpi_opbox_opargs          unit      1  true )

    add_mpi_test( mpi_opbox_hello           component 2  true )
    add_mpi_test( mpi_opbox_ping            component 2  true )

    add_mpi_test( mpi_opbox_atomics         component 2  true )
    add_mpi_test( mpi_opbox_connect         component 2  true )
    add_mpi_test( mpi_opbox_get             component 2  true )
    add_mpi_test( mpi_opbox_long_send       component 2  true )

    add_mpi_test( mpi_opbox_put             component 2  true )
    add_mpi_test( mpi_opbox_remote_buffer   component 1  true )
    add_mpi_test( mpi_opbox_self_send       component 1  true )
    add_mpi_test( mpi_opbox_send            component 2  true )

  #if(Faodel_NETWORK_LIBRARY STREQUAL "libfabric")
  #  add_mpi_test( tb_OpboxOpPingFab    ${OPBOX_PROJECT_DIR}/tests/ops 2  true )
  #  add_mpi_test( tb_FabConn           ${OPBOX_PROJECT_DIR}/tests/ops 2  true )
  #endif()


endif( Faodel_ENABLE_MPI_SUPPORT )
endif( Faodel_HAVE_CRC32 )
