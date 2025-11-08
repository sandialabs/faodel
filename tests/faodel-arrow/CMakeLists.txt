
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


set( SERIAL_TEST_LIBS
    lunasa
  GTest::GTest
  GTest::Main
  )


add_library(fado_test_support
        component/support/ArrowHelpers.hh
        component/support/ArrowHelpers.cpp
        )
target_link_libraries( fado_test_support
        lunasa
        arrow
        )
set_target_properties( fado_test_support PROPERTIES LINKER_LANGUAGE CXX)

set(SERIAL_TEST_LIBS
        lunasa
        fado_test_support
        GTest::GTest
        GTest::Main
        )

# Wire in all the source directories so testing can run as if
# headers were dumped in flat directory
#
include_directories( ${CMAKE_SOURCE_DIR}/src/faodel-arrow ${CMAKE_BINARY_DIR}/src/faodel-arrow)

add_serial_test( tb_fado     component  true  )



