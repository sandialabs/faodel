cmake_minimum_required(VERSION 3.8.0)
project( Faodel
  LANGUAGES CXX C
  VERSION 1.2306.1
  )


################################
#
# Rough guide to the Faodel CMakeLists.txt structure
#
# Stanza / purpose
#
# 1 / CMake options, macros; toolchain configurations, including threading model
# 2 / Locate required TPLs without which Faodel cannot be built
# 3 / Determine which network transport library to use (NNTI/libfabric) based on
#     value of FAODEL_NETLIB environment variable, and locate any needed dependencies
#     (this stanza contained in FfodelTPLs.cmake)
# 4 / Locate optional TPLs/services
# 5 / Define Faodel targets and installation rules
# 6 / Any status output or cleanup
#
#################################

##############################
#
# Stanza 1 : CMake and toolchain configuration
#
##############################

# Policy assertions
if( POLICY CMP0074 )
    cmake_policy( SET CMP0074 NEW ) # find_package() uses PackageName_ROOT vars
endif()
if( POLICY CMP0075 )
    cmake_policy( SET CMP0075 OLD ) # check_include_file() uses CMAKE_REQUIRED_LIBRARIES var
endif()
if( POLICY CMP0115 )
    cmake_policy( SET CMP0115 NEW ) # require all executables to have extension.
endif()



# CMake built-ins that we'll use
include( FindPkgConfig )
include( FindPackageHandleStandardArgs )
include( CMakePackageConfigHelpers )
include( CheckFunctionExists )

# Macros

# This function helps us do all the build/install options for the tests. Rather
# than have the user pipe in the source and target names, it asks for
# a modified version of the path/name of the test. eg
#
#  project/tests/component/tb_mything.cpp --> component/tb_mything
#
# The other arg specifies whether it should add it to the project's test list.
#
# Note: this is the internal version. Users should call a wrapper (below) that
#       specifies whether this is a "c" or "cpp" extension
#
function( add_serial_test_internal testname extension testpath autorun_test )
  
    if( NOT SERIAL_TEST_LIBS )
      message( WARNING "add_serial_test: adding a test with SERIAL_TEST_LIBS not set" )
    endif()
    
    if( Faodel_ENABLE_MPI_SUPPORT )
        set( MPI_TEST_LIBS ${SERIAL_TEST_LIBS} )
        add_mpi_test_internal( ${testname} ${extension} ${testpath} 1 ${autorun_test} )
        return()
    endif( Faodel_ENABLE_MPI_SUPPORT )
    
    # Use testname to build the target and source names      
    set(target ${testname} )
    set(source ${testpath}/${testname}.${extension})

    add_executable( ${target} ${source} )
    target_link_libraries( ${target} ${SERIAL_TEST_LIBS} )
    if( Threads_FOUND )
    	target_link_libraries( ${target} Threads::Threads )
    endif()

    # Not all tests are meant to be run automatically
    if(autorun_test)
      add_test( NAME ${target} COMMAND $<TARGET_FILE:${target}> )
      set_tests_properties( ${testname} PROPERTIES TIMEOUT 30) 
    endif()
    
endfunction()

# Wrapper: default is to call cpp code
function( add_serial_test testname testpath autorun_test )
    add_serial_test_internal( ${testname} "cpp" ${testpath} ${autorun_test} )
endfunction()

# Wrapper: c tests need to call this version
function( add_serial_c_test testname testpath autorun_test )
    add_serial_test_internal( ${testname} "c" ${testpath} ${autorun_test} )
endfunction()





# This function helps us do all the build/install options for the tests. Rather
# than have the user pipe in the source and target names, it asks for
# a modified version of the path/name of the test. eg
#
#  project/tests/component/tb_mything.cpp --> component/tb_mything
#
# The other arg specifies whether it should add it to the project's test list.
#
# Note: this is the internal version. Users should call a wrapper that
#       specifies whether this is a "c" or "cpp" extension
#
function( add_mpi_test_internal testname extension testpath mpi_num_procs autorun_test )

    # Use testname to build the target and source names      
    set(target ${testname} )
    set(source ${testpath}/${testname}.${extension})

    add_executable( ${target} ${source} )
    target_link_libraries( ${target} ${MPI_TEST_LIBS} )
    if( Threads_FOUND )
        target_link_libraries( ${target} Threads::Threads )
    endif()

    # Not all tests are meant to be run automatically
    if(autorun_test)
      set(test_parameters ${MPIEXEC_NUMPROC_FLAG} ${mpi_num_procs} ${MPIEXEC_PREFLAGS} $<TARGET_FILE:${target}> ${MPIEXEC_POSTFLAGS})
      add_test(NAME ${target} COMMAND ${MPIEXEC} ${test_parameters})
      set_tests_properties( ${testname} PROPERTIES TIMEOUT 30) 
    endif()
    
endfunction()

#
# Wrapper: default is to call cpp code
#
function( add_mpi_test testname testpath mpi_num_procs autorun_test )
    add_mpi_test_internal( ${testname} "cpp" ${testpath} ${mpi_num_procs} ${autorun_test} )
endfunction()

# Wrapper: c tests need to call this version
function( add_mpi_c_test testname testpath mpi_num_procs autorun_test )
    add_mpi_test_internal( ${testname} "c" ${testpath} ${mpi_num_procs} ${autorun_test} )
endfunction()



# Options
option( BUILD_SHARED_LIBS           "Build Faodel as shared libs"  OFF )
option( BUILD_TESTS                 "Faodel testing gtest and ctest" OFF )
option( BUILD_EXAMPLES              "Build examples" OFF)
option( BUILD_DOCS                  "Generate documentation using Doxygen" ON )

option( Faodel_ENABLE_ARROW         "Enable support for Apache Arrow" OFF )
option( Faodel_ENABLE_MPI_SUPPORT   "Enable use of MPI communication" ON )
option( Faodel_ENABLE_TCMALLOC      "Use tcmalloc from gperftools in Lunasa, potentially other places" ON )
option( Faodel_ENABLE_IOM_HDF5      "Build the HDF5-based IOM in Kelpie" OFF )
option( Faodel_ENABLE_IOM_LEVELDB   "Build the LevelDB-based IOM in Kelpie" OFF )
option( Faodel_ENABLE_IOM_CASSANDRA "Build the Cassandra-based IOM in Kelpie" OFF )
option( Faodel_ENABLE_DEBUG_TIMERS  "Enable timers for debug purposed" OFF )

mark_as_advanced(Faodel_ENABLE_DEBUG_TIMERS)

set( Faodel_NETWORK_LIBRARY   "nnti" CACHE STRING "RDMA Network library to use for low-level communication" )
set_property(CACHE Faodel_NETWORK_LIBRARY PROPERTY STRINGS nnti libfabric)

set( Faodel_NNTI_SERIALIZATION_METHOD "XDR" CACHE STRING "Serialization library for use in NNTI <XDR|CEREAL>" )
set_property(CACHE Faodel_NNTI_SERIALIZATION_METHOD PROPERTY STRINGS XDR CEREAL )

# Configure the world

# Paths
set( BINARY_INSTALL_DIR  bin     CACHE PATH "Installation directory for executables"  )
set( LIBRARY_INSTALL_DIR lib     CACHE PATH "Installation directory for libraries"    )
set( INCLUDE_INSTALL_DIR include CACHE PATH "Installation directory for header files" )

# This clause cleans up the install paths and does:
#   (1) handle a cygwin name problem
#   (2) set the install path
#   (3) convert INSTALL_*_PATH vars to be absolute

unset(DEF_CMAKE_INSTALL_DIR CACHE)
if(WIN32 AND NOT CYGWIN)
    set(DEF_CMAKE_INSTALL_DIR cmake)
else()
    set(DEF_CMAKE_INSTALL_DIR lib/cmake)
endif()

set(CMAKE_INSTALL_DIR ${DEF_CMAKE_INSTALL_DIR}/Faodel
    CACHE PATH "Installation directory for CMake files")


# Convert each install path to an absolute path
foreach(p LIBRARY BINARY INCLUDE CMAKE)
  set(var ${p}_INSTALL_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

if( NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif()


#
# add some options to whatever build is in progress
#
option( Faodel_NO_ISYSTEM_FLAG "Use -I instead of -isystem when compiling" OFF )
mark_as_advanced(Faodel_NO_ISYSTEM_FLAG)

#
# Require C++11 from whatever compiler is in use
#
set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED YES )
set( CMAKE_CXX_EXTENSIONS OFF )

#
# Set up build configurations. This specifies some common compiler flags for Debug builds.
# Use the CMake configurations instead of setting a *_FLAGS variable directly for a debug build,
#  for example, unless there's some specific need.
#
# These flags get used depending on the value of CMAKE_BUILD_TYPE.
set( CMAKE_CXX_FLAGS_DEBUG "-O0 -g " )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g" )
set( CMAKE_CXX_FLAGS_RELEASE "-O2" )
set( CMAKE_CXX_FLAGS_PEDANTIC "-O0 -g -Wall -Wextra -Wpedantic" )
#set( CMAKE_CONFIGURATION_TYPES Debug Pedantic Release RelWithDebInfo CACHE TYPE INTERNAL FORCE )

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Pedantic Release RelWithDebInfo )


# Set the build type in case a project doesn't. This can be overridden by providing e.g. -DCMAKE_BUILD_TYPE=Release at
#  cmake time.
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Debug CACHE STRING "Specify a build configuration" FORCE )
endif()


#if( ${CMAKE_SYSTEM_NAME} MATCHES "CrayLinuxEnvironment" )
#  if( BUILD_SHARED_LIBS )
#    message( FATAL_ERROR
#    "Faodel does not support shared libraies on Cray compute nodes.  Please set BUILD_SHARED_LIBS=OFF."
#    )
#  endif()
#  set( Boost_USE_STATIC_LIBS ON )
#endif()

macro( disable_isystem_flag )
  # Disable use of the -isystem compiler flag by telling CMake to use -I instead
  set( CMAKE_INCLUDE_SYSTEM_FLAG_C   "-I" )
  set( CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-I" )
  set( Faodel_NO_ISYSTEM_FLAG ON CACHE BOOL "Use -I instead of -isystem when compiling" FORCE ) # you must FORCE a change an option value
  set( NO_ISYSTEM_FLAG ON )
endmacro()


if( Faodel_NO_ISYSTEM_FLAG )
  message( STATUS "This build will not use -isystem for compiling (NO_SYSTEM_IFLAG is ON)." )
  disable_isystem_flag()
endif()

#
# Compiler-specific customizations
#
if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
  add_compile_options(
    "-Wno-unused-parameter"
    "-Wno-unused-local-typedefs"
    )

  if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6 AND NOT Faodel_NO_ISYSTEM_FLAG )
    # disable -isystem
    message( STATUS "This build will not use -isystem for compiling (compiler is GCC >= 6.0)." )
    disable_isystem_flag()
  endif()
    
endif()

####
#
# Handle asserts in debugging
#
####
set( Faodel_ASSERT_METHOD     "cassert" CACHE STRING "Mechanism for dealing with asserts in Faodel")
set_property(CACHE Faodel_ASSERT_METHOD PROPERTY STRINGS cassert debugExit debugHalt debugWarn none)
mark_as_advanced(Faodel_ASSERT_METHOD)
if(     Faodel_ASSERT_METHOD             MATCHES "cassert")
   set( Faodel_ASSERT_METHOD_CASSERT     TRUE)
elseif( Faodel_ASSERT_METHOD             MATCHES "debugExit")
   set( Faodel_ASSERT_METHOD_DEBUG_EXIT  TRUE)
elseif( Faodel_ASSERT_METHOD             MATCHES "debugHalt")
   set( Faodel_ASSERT_METHOD_DEBUG_HALT  TRUE)
elseif( Faodel_ASSERT_METHOD             MATCHES "debugWarn")
   set( Faodel_ASSERT_METHOD_DEBUG_WARN  TRUE)
elseif( Faodel_ASSERT_METHOD             MATCHES "none")
   set( Faodel_ASSERT_METHOD_NONE        TRUE)
endif()




####
#
# Handle threads
#
####

set(Faodel_THREADING_MODEL "pthreads" CACHE STRING "Threading library that Faodel should use for Mutexes. pthreads, openmp")
set_property(CACHE Faodel_THREADING_MODEL PROPERTY STRINGS pthreads openmp)
mark_as_advanced(Faodel_THREADING_MODEL)

if( Faodel_THREADING_MODEL MATCHES "pthreads" )
    #Faodel uses a define to figure out whether it should try loading headers
    set( Faodel_THREADING_MODEL_PTHREADS  TRUE )
    set( Faodel_THREADING_MODEL_OPENMP    FALSE )
    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "CrayLinuxEnvironment")
      message(STATUS "pthreads on Cray CLE requires --whole-archive")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
    else()
      find_package(Threads REQUIRED)
    endif()
elseif( Faodel_THREADING_MODEL MATCHES "openmp" )
    set( Faodel_THREADING_MODEL_PTHREADS  FALSE )
    set( Faodel_THREADING_MODEL_OPENMP    TRUE )
    find_package( OpenMP REQUIRED)
    if(OPENMP_FOUND)
      add_compile_options( "$<$<COMPILE_LANGUAGE:C>:${OpenMP_C_FLAGS}>" )
      add_compile_options( "$<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>" )
    endif()
else()
    message( FATAL_ERROR
          "Faodel's threading model needs to be either pthreads or openmp."
          )
endif()

## Handle crc32()
## Some tests use crc32() to validate network transfers.
## Assume that crc32() is in zlib and make sure we can find it.
find_package(ZLIB)
if (ZLIB_FOUND)
  set (CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARIES})
  check_function_exists(crc32 Faodel_HAVE_CRC32)
  set (CMAKE_REQUIRED_LIBRARIES "")
endif(ZLIB_FOUND)
if (NOT Faodel_HAVE_CRC32)
  message(STATUS "crc32() not found - some tests will not be built")
endif (NOT Faodel_HAVE_CRC32)

## Handle logging
set( Faodel_LOGGING_METHOD "stdout" CACHE STRING "Select where logging interface output is routed" )
set_property(CACHE Faodel_LOGGING_METHOD PROPERTY STRINGS stdout sbl disabled)

if (Faodel_LOGGING_METHOD MATCHES "stdout")
  set(Faodel_LOGGINGINTERFACE_DISABLED FALSE)
  set(Faodel_LOGGINGINTERFACE_USE_SBL  FALSE)

elseif( Faodel_LOGGING_METHOD MATCHES "sbl")
  set(Faodel_LOGGINGINTERFACE_DISABLED FALSE)
  set(Faodel_LOGGINGINTERFACE_USE_SBL  TRUE)

else()
  # Disabled - optimize away
  set(Faodel_LOGGINGINTERFACE_DISABLED TRUE)
  set(Faodel_LOGGINGINTERFACE_USE_SBL  FALSE)
endif()

##############################
#
# Stanza 2 : Locate required TPLs
#
##############################

# Don't treat any of our imports here as system libraries
set( savevar ${CMAKE_NO_SYSTEM_FROM_IMPORTED} )
set( CMAKE_NO_SYSTEM_FROM_IMPORTED TRUE )

include( ${CMAKE_CURRENT_LIST_DIR}/cmake/FaodelTPLs.cmake )

include( ${CMAKE_CURRENT_LIST_DIR}/cmake/FaodelRpathHelpers.cmake )
set_faodel_install_rpath()
set_cmake_install_rpath()
# The CMake Cray system module does not define the RPATH flags (probably
# because Cray has historically only supported static linking).  Now that
# dynamic linking is supported, we define them ourselves.
if( ${CMAKE_SYSTEM_NAME} MATCHES "CrayLinuxEnvironment" )
  if( BUILD_SHARED_LIBS )
    set_cmake_rpath_flags()
  endif()
endif()

##############################
#
# Stanza 3 : Locate optional TPLs and services
#
##############################


#########################
##  Libhio
#########################
# previous versions used to have hooks to libhio. We no longer need
#pkg_search_module( Libhio_pc libhio IMPORTED_TARGET )
#
#if( Libhio_pc_FOUND )
#  set( LIBHIO_FOUND TRUE )
#  LIST( APPEND Libhio_TARGETS PkgConfig::Libhio_pc )
#  message( STATUS "Found Libhio, target PkgConfig::Libhio_pc appended to Libhio_targets" )
#endif()

#########################
##  Doxygen
#########################
find_package( Doxygen QUIET )


# Leave this flag how we found it, in case someone else is using it
set( CMAKE_NO_SYSTEM_FROM_IMPORTED ${savevar} )


################################
##
## Stanza 4 : Define Faodel build and install targets
##
################################
if( Faodel_ENABLE_TCMALLOC )

  # Bail out if we're on an architecture that doesn't support tcmalloc
  if( CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le" ) 
     message( FATAL_ERROR "The TCMALLOC library included with Faodel does not currently have "
                          "  support for the ${CMAKE_SYSTEM_PROCESSOR} CPU (it has optimizations"
                          "  for x86_64). You must set Faodel_ENABLE_TCMALLOC to OFF in order to"
                          "  build Faodel on this platform.")
  endif()

  add_subdirectory( tpl/gperftools )
  message( STATUS "Faodel_ENABLE_TCMALLOC is set, gperftools TPL will be built and tcmalloc will be used" )
  set( PKGCONFIG_TCMALLOC "-ltcmalloc -lspinlock" )
endif()

add_subdirectory( tpl/cereal )

add_subdirectory( src/sbl )
add_subdirectory( src/faodel-common)
add_subdirectory(src/whookie)
add_subdirectory( src/faodel-services)
if( BUILD_NNTI )
  add_subdirectory( src/nnti )
endif()
add_subdirectory( src/lunasa )
add_subdirectory( src/opbox )
add_subdirectory( src/dirman )
add_subdirectory( src/kelpie )
if( Faodel_ENABLE_ARROW)
    add_subdirectory(src/faodel-arrow)
endif()


# Set some useful properties on our targets, now that they're defined
foreach( COMP sbl common whookie services lunasa opbox kelpie )
  
  target_include_directories( ${COMP}
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include/faodel>
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    )
  set_target_properties( ${COMP} PROPERTIES LINKER_LANGUAGE CXX )

endforeach()

if( BUILD_NNTI )
  target_include_directories( nnti INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include/faodel>
    )
  set_target_properties( nnti PROPERTIES LINKER_LANGUAGE CXX )
endif()

if (Faodel_NETWORK_LIBRARY STREQUAL "nnti")
  set( USE_NNTI TRUE )
endif()
if (Faodel_NETWORK_LIBRARY STREQUAL "libfabric")
  set( USE_LIBFABRIC TRUE )
endif()

if( Faodel_ENABLE_ARROW )
    target_include_directories( arrow
            INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
            $<INSTALL_INTERFACE:include/faodel>
            PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
            )
    set_target_properties( arrow PROPERTIES LINKER_LANGUAGE CXX )
endif()

# Build tools
add_subdirectory( tools/faodel-cli )
add_subdirectory( tools/faodel-stress )
add_subdirectory( tools/kelpie-server )


# Do the tests
if( BUILD_TESTS )

  enable_testing()
  
  add_subdirectory( tests/sbl )
  add_subdirectory( tests/common )
  add_subdirectory( tests/whookie )
  add_subdirectory( tests/services )
  if( BUILD_NNTI )
    add_subdirectory( tests/nnti )
  endif()
  add_subdirectory( tests/lunasa )
  if(Faodel_ENABLE_ARROW)
      add_subdirectory(tests/faodel-arrow )
  endif()
  add_subdirectory( tests/opbox )
  add_subdirectory( tests/dirman )
  add_subdirectory( tests/kelpie )
  
endif()

if( BUILD_EXAMPLES )
  # Optional: add the examples. Examples are scoped to only use libs from a certain
  # point down. Currently missing the nnti examples.
  foreach(ex_lib common dirman kelpie lunasa opbox services)
      set(EXAMPLE_LIBS ${ex_lib})
      add_subdirectory( examples/${ex_lib} )
  endforeach(ex_lib)

  set(EXAMPLE_LIBS whookie common)
  add_subdirectory( examples/whookie )
endif()

export( EXPORT FaodelTargets FILE FaodelTargets.cmake )

install( EXPORT FaodelTargets
  FILE FaodelTargets.cmake
  NAMESPACE Faodel::
  DESTINATION ${CMAKE_INSTALL_DIR}
  COMPONENT dev )

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/FaodelCtestConfig.cmake.in
  "${PROJECT_BINARY_DIR}/FaodelCtestConfig.cmake"
  @ONLY ) 

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/FaodelConfig.cmake.in
  ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FaodelConfig.cmake
  INSTALL_DESTINATION lib/cmake/Faodel
  PATH_VARS INCLUDE_INSTALL_DIR LIBRARY_INSTALL_DIR BINARY_INSTALL_DIR CMAKE_INSTALL_DIR
  )

write_basic_package_version_file( 
  ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FaodelConfigVersion.cmake
  COMPATIBILITY AnyNewerVersion )


install( FILES
  ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FaodelConfig.cmake
  ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FaodelConfigVersion.cmake
  ${PROJECT_SOURCE_DIR}/cmake/FaodelTPLs.cmake
  DESTINATION lib/cmake/Faodel
  )

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/faodel.pc.in
  ${PROJECT_BINARY_DIR}/faodel.pc
  @ONLY
  )

install( FILES
  ${PROJECT_BINARY_DIR}/faodel.pc
  DESTINATION lib/pkgconfig
  )

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/faodelConfig.h.in
    "${PROJECT_BINARY_DIR}/src/faodelConfig.h"
    @ONLY
    )

# ... puts faodelConfig.h in include/common
install(FILES 
    "${PROJECT_BINARY_DIR}/src/faodelConfig.h"
    DESTINATION include/faodel
    )


if( BUILD_DOCS )

  if( DOXYGEN_FOUND )
    
    add_custom_target(doc
      COMMAND ${DOXYGEN_EXECUTABLE} docs/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating API documentation with Doxygen" VERBATIM
      )
    add_custom_target(docs)
    add_dependencies( docs doc)
    
    add_custom_target( install-doc
      COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME} && cp -R docs/ ${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Installing API documentation" VERBATIM
      )
    
    add_dependencies( install-doc doc )
  else()
    
    add_custom_target( doc
      COMMAND echo "Doxygen isn't available so I can't generate documentation, sorry."
      VERBATIM
      )
    
    add_custom_target( install-doc
      COMMAND echo "Doxygen isn't available so I can't generate documentation, sorry."
      VERBATIM
      )
  endif( DOXYGEN_FOUND )
endif( BUILD_DOCS )



################################
##
## Stanza 5 : Any status reporting or cleanup items
##
################################
include( ${CMAKE_CURRENT_LIST_DIR}/cmake/FaodelSummary.cmake )
print_faodel_config_summary()

##
## Next we check for disabled NNTI transports that seem like 
## they could be errors or unintended misconfigurations.
##
include( ${CMAKE_CURRENT_LIST_DIR}/cmake/FaodelCheckTransports.cmake )
check_preferred_transports()
