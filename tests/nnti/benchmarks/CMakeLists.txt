
# Note: to find gtest, pass in GTEST_ROOT.
#       eg, cmake -DGTEST_ROOT=/my/install_dir
#           where install dir has include and lib in it


include_directories(
   ${CMAKE_SOURCE_DIR}/src/nnti
   ${CMAKE_BINARY_DIR}/src/nnti
)
SET(EXTRA_TEST_LIBS "-lz")


# Build the test support lib-------------------------------------------------
if(Faodel_ENABLE_MPI_SUPPORT)

   add_library(benchutils
               bench_utils.hpp
               bench_utils.cpp )

   set_target_properties(benchutils PROPERTIES LINKER_LANGUAGE  CXX )
   target_link_libraries(benchutils PUBLIC nnti  )

endif(Faodel_ENABLE_MPI_SUPPORT)
#----------------------------------------------------------------------------

set(SERIAL_TEST_LIBS
  benchutils
  GTest::GTest
  GTest::Main
  ${EXTRA_TEST_LIBS}
)

set(MPI_TEST_LIBS 
  MPI::MPI_CXX
  ${SERIAL_TEST_LIBS}
)

#--------------+-----------------------------+----------------------------------+---------+
# Format:      |  Name                       |  Directory                       | Autorun |
#--------------+-----------------------------+----------------------------------+---------+

if( Faodel_ENABLE_MPI_SUPPORT )
  add_mpi_test( MemoryRegistration           . 1 false  )

  add_mpi_test( AtomicsWithCallback          . 2 false  )
  add_mpi_test( RdmaWithCallback             . 2 false  )
  add_mpi_test( SendWithCallback             . 2 false  )
endif()
