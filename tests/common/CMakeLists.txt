
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set( SERIAL_TEST_LIBS
    common
  GTest::GTest
  GTest::Main
  )

set(webhook
    ${SERIAL_TEST_LIBS}
    )
# Wire in all the source directories so testing can run as if
# headers were dumped in flat directory
#
include_directories( ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_BINARY_DIR}/src/common)

add_serial_test( tb_common_stringhelpers     unit  true  )
add_serial_test( tb_common_structs           unit  true  )
add_serial_test( tb_common_resourceurl       unit  true  )
add_serial_test( tb_common_mutex             unit  true  )
add_serial_test( tb_common_configuration     unit  true  )
add_serial_test( tb_common_serialization     unit  true  )
add_serial_test( tb_common_logginginterface  unit  true  )
add_serial_test( tb_common_bootstrap         unit  true  )

target_link_libraries( tb_common_serialization Boost::serialization )

