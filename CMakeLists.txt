cmake_minimum_required(VERSION 3.2)
project( Faodel
  LANGUAGES CXX C
  VERSION 1.1803.1
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

# CMake built-ins that we'll use
include( FindPkgConfig )
include( FindPackageHandleStandardArgs )
include( CMakePackageConfigHelpers )


# Macros

# This function helps us do all the build/install options for the tests. Rather
# than have the user pipe in the source and target names, it asks for
# a modified version of the path/name of the test. eg
#
#  project/tests/component/tb_mything.cpp --> component/tb_mything
#
# The other arg specifies whether it should add it to the project's test list.
#
function( add_serial_test testname testpath autorun_test )
  
    if( NOT SERIAL_TEST_LIBS )
      message( WARNING "add_serial_test: adding a test with SERIAL_TEST_LIBS not set" )
    endif()
    
    if( USE_MPI )
        set( MPI_TEST_LIBS ${SERIAL_TEST_LIBS} )
        add_mpi_test( ${testname} ${testpath} 1 ${autorun_test} )
        return()
    endif( USE_MPI )
    
    # Use testname to build the target and source names      
    set(target ${testname} )
    set(source ${testpath}/${testname})

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

# This function helps us do all the build/install options for the tests. Rather
# than have the user pipe in the source and target names, it asks for
# a modified version of the path/name of the test. eg
#
#  project/tests/component/tb_mything.cpp --> component/tb_mything
#
# The other arg specifies whether it should add it to the project's test list.
#
function( add_mpi_test testname testpath mpi_num_procs autorun_test )

    # Use testname to build the target and source names      
    set(target ${testname} )
    set(source ${testpath}/${testname})

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


# Options
option( BUILD_SHARED_LIBS   "Build Faodel as shared libs"  OFF )  
option( BUILD_TESTS       "Faodel testing gtest and ctest" ON )
option( BUILD_DOCS          "Generate documentation using Doxygen"  ON )
option( USE_MPI             "Enable use of MPI communication"       ON )
option( ENABLE_TCMALLOC        "Use tcmalloc from gperftools in Lunasa, potentially other places"  ON )
option( QUIET_BUILD         "Output configuration, build, test, install output to log files instead of stdout" OFF )

# Configure the world

# Paths
set( BINARY_INSTALL_DIR     bin     CACHE PATH "Installation directory for executables"  )
set( LIBRARY_INSTALL_DIR     lib     CACHE PATH "Installation directory for libraries"    )
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
option( NO_ISYSTEM_FLAG "Use -I instead of -isystem when compiling" OFF )

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
set( CMAKE_CONFIGURATION_TYPES Debug Pedantic Release RelWithDebInfo CACHE TYPE INTERNAL FORCE )

# Set the build type in case a project doesn't. This can be overridden by providing e.g. -DCMAKE_BUILD_TYPE=Release at
#  cmake time.
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Debug )
endif()


if( NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
  set( LOAD_WHOLE_ARCHIVE_PREFIX "-Wl,--whole-archive" )
  set( LOAD_WHOLE_ARCHIVE_SUFFIX "-Wl,--no-whole-archive" )
else()
  set( LOAD_WHOLE_ARCHIVE_PREFIX "-Wl,-force_load")
endif()

if( ${CMAKE_SYSTEM_NAME} MATCHES "CrayLinuxEnvironment" )
  set( Boost_USE_STATIC_LIBS ON )
endif()

macro( disable_isystem_flag )
  # Disable use of the -isystem compiler flag by telling CMake to use -I instead
  set( CMAKE_INCLUDE_SYSTEM_FLAG_C   "-I" )
  set( CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-I" )
  set( NO_ISYSTEM_FLAG ON )
endmacro()


if( NO_ISYSTEM_FLAG )
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

  if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6 AND NOT NO_ISYSTEM_FLAG )
    # disable -isystem
    message( STATUS "This build will not use -isystem for compiling (compiler is GCC >= 6.0)." )
    disable_isystem_flag()
  endif()
    
endif()

####
#
# Handle threads
#
####

set(THREADING_MODEL "PTHREADS" CACHE STRING "Threading library that Kelpie should use for Mutexes. PTHREADS, OPENMP, NONE")
set_property(CACHE THREADING_MODEL PROPERTY STRINGS PTHREADS OPENMP NONE)

if( THREADING_MODEL MATCHES "PTHREADS" )
    #Kelpie uses a define to figure out whether it should try loading headers
    add_definitions(-D_PTHREADS)
    set( _PTHREADS true )
    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "CrayLinuxEnvironment")
      message(STATUS "pthreads on Cray CLE requires --whole-archive")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
    else()
      find_package(Threads REQUIRED)
    endif()
elseif( THREADING_MODEL MATCHES "OPENMP" )
    add_definitions(-D_OPENMP)
    set( _OPENMP  true )
    find_package( OpenMP REQUIRED)
    if(OPENMP_FOUND)
      add_compile_options( "$<$<COMPILE_LANGUAGE:C>:${OpenMP_C_FLAGS}>" )
      add_compile_options( "$<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>" )
    endif()
endif()

##############################
#
# Stanza 2 : Locate required TPLs
#
##############################

# Don't treat any of our imports here as system libraries
set( savevar ${CMAKE_NO_SYSTEM_FROM_IMPORTED} )
set( CMAKE_NO_SYSTEM_FROM_IMPORTED TRUE )

include( ${CMAKE_CURRENT_LIST_DIR}/config/FaodelTPLs.cmake )

##############################
#
# Stanza 4 : Locate optional TPLs and services
#
##############################


#########################
##  Libhio
#########################

pkg_search_module( Libhio_pc libhio IMPORTED_TARGET )

if( Libhio_pc_FOUND )
  set( LIBHIO_FOUND TRUE )
  LIST( APPEND Libhio_TARGETS PkgConfig::Libhio_pc )
  message( STATUS "Found Libhio, target PkgConfig::Libhio_pc appended to Libhio_targets" )
endif()

#########################
##  Doxygen
#########################
find_package( Doxygen QUIET )


# Leave this flag how we found it, in case someone else is using it
set( CMAKE_NO_SYSTEM_FROM_IMPORTED ${savevar} )


################################
##
## Stanza 5 : Define Faodel build and install targets
##
################################
if( ENABLE_TCMALLOC )
  add_subdirectory( tpl/gperftools )
  message( STATUS "ENABLE_TCMALLOC is set, gperftools TPL will be built and tcmalloc will be used" )
endif()
    
add_subdirectory( src/sbl )
add_subdirectory( src/common )
add_subdirectory( src/webhook )
if( BUILD_NNTI )
  add_subdirectory( src/nnti )
endif()
add_subdirectory( src/lunasa )
add_subdirectory( src/opbox )
add_subdirectory( src/kelpie )

# Set some useful properties on our targets, now that they're defined
foreach( COMP sbl common webhook lunasa opbox kelpie )
  
  target_include_directories( ${COMP}
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include/faodel>
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    )
  set_target_properties( ${COMP} PROPERTIES LINKER_LANGUAGE CXX )

endforeach()

if( BUILD_NNTI )
  target_include_directories( nnti INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include/faodel>
    )
  set_target_properties( nnti PROPERTIES LINKER_LANGUAGE CXX )
endif()


# Do the tests
if( BUILD_TESTS )

  enable_testing()
  
  add_subdirectory( tests/sbl )
  add_subdirectory( tests/common )
  add_subdirectory( tests/webhook )
  if( BUILD_NNTI )
    add_subdirectory( tests/nnti )
  endif()
  add_subdirectory( tests/lunasa )
  add_subdirectory( tests/opbox )
  add_subdirectory( tests/kelpie )
  
endif()

export( EXPORT FaodelTargets FILE FaodelTargets.cmake )

install( EXPORT FaodelTargets
  FILE FaodelTargets.cmake
  NAMESPACE Faodel::
  DESTINATION ${CMAKE_INSTALL_DIR}
  COMPONENT dev )

configure_file(
  ${PROJECT_SOURCE_DIR}/config/FaodelCtestConfig.cmake.in
  "${PROJECT_BINARY_DIR}/FaodelCtestConfig.cmake"
  @ONLY ) 

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/config/FaodelConfig.cmake.in
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
  ${PROJECT_SOURCE_DIR}/config/FaodelTPLs.cmake
  DESTINATION lib/cmake/Faodel
  )

configure_file(
  ${PROJECT_SOURCE_DIR}/config/faodel.pc.in
  ${PROJECT_BINARY_DIR}/faodel.pc
  @ONLY
  )

install( FILES
  ${PROJECT_BINARY_DIR}/faodel.pc
  DESTINATION lib/pkgconfig
  )


if( BUILD_DOCS )

  if( DOXYGEN_FOUND )
    
    add_custom_target(doc
      COMMAND ${DOXYGEN_EXECUTABLE} docs/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating API documentation with Doxygen" VERBATIM
      )
    
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
message( STATUS "Configured to build Faodel with ${CMAKE_MAKE_PROGRAM}" )

