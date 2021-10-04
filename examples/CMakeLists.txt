cmake_minimum_required(VERSION 3.8.0)

project(faodel_examples LANGUAGES CXX C)

# Policy assertions
if( POLICY CMP0074 )
    cmake_policy( SET CMP0074 NEW ) # find_package() uses PackageName_ROOT vars
endif()

# Options
option( ENABLE_MPI_EXAMPLES             "Enable examples that use MPI to setup" ON)

# By default, cmake looks in your ~/.cmake directory for information
# about packages. While that should be fine for most users, we disable
# it in these examples because developers often have different versions
# of libraries in different places and cmake always guesses wrong.
set( CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY TRUE )
set( BINARY_INSTALL_DIR  bin  CACHE PATH "Installation directory for executables"  )

# The CMake Cray system module does not define the RPATH flags (probably
# because Cray has historically only supported static linking).  Now that
# dynamic linking is supported, we define them ourselves.
if( ${CMAKE_SYSTEM_NAME} MATCHES "CrayLinuxEnvironment" )
    set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG     "-Wl,-rpath," )
    set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG_SEP ":"           )
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG       "-Wl,-rpath," )
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG_SEP   ":"           )

    set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG     "-Wl,-rpath," )
    set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG_SEP ":"           )
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG       "-Wl,-rpath," )
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP   ":"           )

    set(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-Wl,-soname," )
    set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG   "-Wl,-soname," )

    set(CMAKE_EXECUTABLE_RPATH_LINK_CXX_FLAG     "-Wl,-rpath-link," )
    set(CMAKE_EXECUTABLE_RPATH_LINK_C_FLAG       "-Wl,-rpath-link," )
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX_FLAG "-Wl,-rpath-link," )
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_C_FLAG   "-Wl,-rpath-link," )

    set(CMAKE_SKIP_INSTALL_RPATH NO )
    set(CMAKE_SKIP_RPATH         NO )
endif()

find_package( Faodel CONFIG REQUIRED )


if( ENABLE_MPI_EXAMPLES )
    find_package(MPI REQUIRED)
endif()



add_subdirectory( common )
add_subdirectory( whookie )
add_subdirectory( services )
add_subdirectory( lunasa )
add_subdirectory( opbox )
add_subdirectory( dirman )
add_subdirectory( kelpie )

if( Faodel_ENABLE_MPI_SUPPORT )
  # These examples have some MPI requirements, so don't build if missing mpi

  if( Faodel_BUILT_Faodel_NETWORK_LIBRARY STREQUAL "nnti" )
    add_subdirectory( nnti )
  endif()



endif()

