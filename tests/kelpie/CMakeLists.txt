include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/mpi )

# Build the test support lib-------------------------------------------------
if(Faodel_ENABLE_MPI_SUPPORT)

  add_library(kelpie_mpi_test_support
        component/support/Globals.hh
        component/support/Globals.cpp
        component/support/ExperimentLauncher.hh
        component/support/ExperimentLauncher.cpp)


  target_link_libraries( kelpie_mpi_test_support
    opbox
    MPI::MPI_CXX
    GTest::GTest
    GTest::Main
    )
  set_target_properties( kelpie_mpi_test_support PROPERTIES LINKER_LANGUAGE  CXX )

endif()

#----------------------------------------------------------------------------

set(COMMON_TEST_LIBS
  kelpie
  ${FaodelNetlib_TARGETS}
  Boost::serialization
)

set(SERIAL_TEST_LIBS
  ${COMMON_TEST_LIBS}
  GTest::GTest
  GTest::Main
)

set(MPI_TEST_LIBS 
  kelpie_mpi_test_support
  ${COMMON_TEST_LIBS}
)



#--------------+--------------------------------+---------------+---------+
# Format:      |  Name                          |  Directory    | Autorun |
#--------------+--------------------------------+---------------+---------+
add_serial_test( tb_kelpie_types                 unit            true )
add_serial_test( tb_kelpie_key                   unit            true )
add_serial_test( tb_kelpie_localkv               unit            true )
add_serial_test( tb_kelpie_message_direct        unit/messages   true )
add_serial_test( tb_kelpie_multiple_hidden_inits component       true )
add_serial_test( tb_kelpie_nonet_compute         component/nonet true )
add_serial_test( tb_kelpie_nonet_iompio          component/nonet true )
add_serial_test( tb_kelpie_nonet_iom_from_url    component/nonet true )

add_serial_test( tb_kelpie_iom_pio_basic         unit/ioms       true )
if( Faodel_HAVE_GTEST_TYPED_TEST_SUITE_API )
    add_serial_test( tb_kelpie_iom_all_basic     unit/ioms       true )
    add_serial_test( tb_kelpie_iom_service_basic unit/ioms       false )
endif()

if( Faodel_ENABLE_MPI_SUPPORT )
    add_mpi_test( mpi_kelpie_compute         component 2 true)
    add_mpi_test( mpi_kelpie_simple_peer     component 2 true)
    add_mpi_test( mpi_kelpie_many_items      component 2 true)
    add_mpi_test( mpi_kelpie_dht             component 4 true)
    add_mpi_test( mpi_kelpie_rft             component 16 true)
    add_mpi_test( mpi_kelpie_tft             component 4 true)
    add_mpi_test( mpi_kelpie_behaviors       component 2 true)
    add_mpi_test( mpi_kelpie_iom_dht         component 16 true)
endif()




