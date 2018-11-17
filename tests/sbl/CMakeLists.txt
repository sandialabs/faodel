##############################################################################
#          All Testing should be subordinate to src/tests directory          #
##############################################################################

include_directories( ${CMAKE_SOURCE_DIR}/src/sbl ${CMAKE_BINARY_DIR}/src/sbl )

set( SERIAL_TEST_LIBS
  GTest::GTest
  GTest::Main
  Boost::log
  Boost::log_setup
  sbl
  )

# add_serial_test( test-name, directory, autorun-flag )
add_serial_test( tb_sbl_logger_class unit true )
add_serial_test( tb_sbl_source_class unit true )
add_serial_test( tb_sbl_compile_speed unit true )

