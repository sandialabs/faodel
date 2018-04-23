
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set( SERIAL_TEST_LIBS
  common
  GTest::GTest
  GTest::Main
  )

# Wire in all the source directories so testing can run as if
# headers were dumped in flat directory
#
include_directories( ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_BINARY_DIR}/src/common )

add_serial_test( tb_common_hello      unit  true  )
add_serial_test( tb_stringhelpers     unit  true  )
add_serial_test( tb_puny              unit  true  )
add_serial_test( tb_mutex             unit  true  )
add_serial_test( tb_configuration     unit  true  )
add_serial_test( tb_backburner        unit  true  )
add_serial_test( tb_backburnerService unit  true  )
add_serial_test( tb_bootstrap         unit  true  )
add_serial_test( tb_nodeid            unit  true  )
add_serial_test( tb_serialization     unit  true  )
add_serial_test( tb_bucket            unit  true  )
add_serial_test( tb_url               unit  true  )
add_serial_test( tb_logginginterface  unit  true  )

target_link_libraries( tb_serialization Boost::serialization )
