
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it

include_directories(
   ${CMAKE_SOURCE_DIR}/src/nnti
   ${CMAKE_BINARY_DIR}/src/nnti
)
SET(EXTRA_TEST_LIBS "-lz")


# Build the test support lib-------------------------------------------------
add_library(ctestutils
            test_utils.h
            test_utils.cpp
)
set_target_properties(ctestutils PROPERTIES LINKER_LANGUAGE  CXX )
target_link_libraries( ctestutils PUBLIC nnti )
#----------------------------------------------------------------------------

set(SERIAL_TEST_LIBS 
  ctestutils
  GTest::GTest
  GTest::Main
  ${EXTRA_TEST_LIBS}
)

set(MPI_TEST_LIBS 
  ${SERIAL_TEST_LIBS}
  MPI_C
)

#--------------+-----------------------------+----------------------------------+---------+
# Format:      |  Name                       |  Directory                       | Autorun |
#--------------+-----------------------------+----------------------------------+---------+
add_serial_test( NntiAllocFreeTest           .  true  )
add_serial_test( NntiInitTest1               .  true  )
add_serial_test( NntiInitTest2               .  true  )
add_serial_test( NntiInitTest3               .  true  )
add_serial_test( NntiInitTest4               .  true  )
add_serial_test( NntiLoggerTest1             .  true  )
add_serial_test( NntiLoggerTest2             .  true  )
add_serial_test( NntiLoggerTest3             .  true  )
