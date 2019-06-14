# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set(SERIAL_TEST_LIBS
  lunasa
  services            # only used in backburner_ldo test
  GTest::GTest
  GTest::Main
  ${EXTRA_TEST_LIBS}
  )

set( MPI_TEST_LIBS
  MPI::MPI_CXX
  ${SERIAL_TEST_LIBS}
  )


#--------------+-------------------------------+----------------------------------------+---------+
# Format:      |  Name                         |  Directory                             | Autorun |
#--------------+-------------------------------+----------------------------------------+---------+
add_serial_test(  tb_lunasa_configuration1_valid     unit       true  )
add_serial_test(  tb_lunasa_configuration2_reinit_tc unit       true  )
add_serial_test(  tb_lunasa_configuration3_double_tc unit       true  )
add_serial_test(  tb_lunasa_basic_allocations    component       true  )
add_serial_test(  tb_lunasa_ldo                  component       true  )
add_serial_test(  tb_lunasa_backburner_ldo       component       true  )
add_serial_test(  tb_lunasa_generic_data_bundle  component       true  )
add_serial_test(  tb_lunasa_performance          component       true  )
add_serial_test(  tb_lunasa_threaded_performance component       true  )
add_serial_test(  tb_lunasa_statistics           component       true  )
add_serial_test(  tb_lunasa_data_type_registry   component       true  )
add_serial_test(  tb_lunasa_copy_ldo             component       true  )

if( Faodel_ENABLE_MPI_SUPPORT )
if(Faodel_NETWORK_LIBRARY STREQUAL "nnti")
  add_mpi_test(  mpi_lunasa_nnti_register_memory  component/nnti 1 true  )
  add_mpi_test(  mpi_lunasa_nnti_send             component/nnti 2 true  )
  add_mpi_test(  mpi_lunasa_nnti_send_user        component/nnti 2 true  )
  add_mpi_test(  mpi_lunasa_nnti_put              component/nnti 2 true  )
endif()
endif()

#Note: don't include the standalone projects as they create 
#      a circular dependency. Instead, go to the standalone tests
#      and build their project separately
##make_lunasa_test( standalone/tb_staticBootstrap true)



