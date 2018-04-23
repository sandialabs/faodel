# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set(SERIAL_TEST_LIBS
  lunasa
  GTest::GTest
  GTest::Main
  ${EXTRA_TEST_LIBS}
  )

set( MPI_TEST_LIBS
  MPI_CXX
  ${SERIAL_TEST_LIBS}
  )


#--------------+-------------------------------+----------------------------------------+---------+
# Format:      |  Name                         |  Directory                             | Autorun |
#--------------+-------------------------------+----------------------------------------+---------+
add_serial_test(  tb_lunasa_hello               component       true  )
add_serial_test(  tb_LunasaBootstrap            component       true  )
add_serial_test(  tb_LunasaConfiguration        component       true  )
add_serial_test(  tb_LunasaConfiguration2       component       true  )
add_serial_test(  tb_LunasaConfiguration3       component       true  )
add_serial_test(  tb_LunasaTest1                component       true  )
add_serial_test(  tb_LunasaHealth               component       true  )
add_serial_test(  tb_LunasaPerformance          component       true  )
add_serial_test(  tb_LunasaThreadedPerformance  component       true  )
add_serial_test(  tb_LunasaStatistics           component       true  )
add_serial_test(  tb_LunasaDataObjectTest       component       true  )

if( HAVE_MPI )
if(NETWORK_LIBRARY STREQUAL "nnti")
  add_mpi_test(  tb_LunasaRegisterMemoryTest   component 1 true  )
  add_mpi_test(  tb_LunasaSendTest             component  2 true  )
  add_mpi_test(  tb_LunasaSendUserTest         component  2    true  )
  add_mpi_test(  tb_LunasaPutUserTest         component   2 true  )
endif()
endif()

#Note: don't include the standalone projects as they create 
#      a circular dependency. Instead, go to the standalone tests
#      and build their project separately
##make_lunasa_test( standalone/tb_staticBootstrap true)



