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
add_serial_test( SblLoggerClassTest unit true )
add_serial_test( SblSourceClassTest unit true )
add_serial_test( SblCompileSpeedTest unit true )

