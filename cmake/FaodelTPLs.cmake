  
##########################
## Get the Boost targets
##########################

# Try not to accidentally pick up the wrong install of a TPL when part of a larger compilation.
# In general, for a TPL that has a FindXXX.cmake that we can access through find_package(),
# require that the associated XXX_ROOT variable is set (e.g., BOOST_ROOT for Boost). If we
# are included in someone else's compilation, this will allow the top level to let us know
# which TPL install that they've used to build other components. 

if( NOT DEFINED BOOST_ROOT AND 
    NOT DEFINED BOOSTROOT AND 
    NOT DEFINED ENV{BOOST_ROOT} )
  message( FATAL_ERROR
    "Neither of the variables BOOST_ROOT or BOOSTROOT is set. Make sure one of these variables contains the location of a usable Boost installation for this build, then re-run CMake."
    )
endif()

set( Boost_NO_BOOST_CMAKE ON )
if( FAODEL_FORCE_BOOST_STATIC_LINKING OR REQUIRE_STATIC_LINKING )
  set( Boost_USE_STATIC_LIBS ON )
endif()

find_package( Boost 1.60
  COMPONENTS system log log_setup thread serialization program_options
  REQUIRED )

if( NOT TARGET Boost::system )
  # Boost libraries were found but imported targets weren't created.
  # This is most likely due to the present CMake's FindBoost.cmake module not knowing
  # about the found Boost version because that version is newer than FindBoost.cmake.
  # FindBoost.cmake will have dropped some error output but will continue the CMake configuration.
  # However, since we can't continue until the user either uses a "new enough" CMake or "old enough"
  # Boost, we will just pull the plug here.
  message( FATAL_ERROR
    "Imported targets for Boost were not created and FAODEL subprojects cannot be built. This can happen when the located Boost installation is newer than your version of CMake, and your CMake's FindBoost.cmake doesn't have dependency information for that Boost version. If this is the case, you should either switch to a newer CMake or an older Boost."
    )
endif()

if( NOT Boost_USE_STATIC_LIBS )
  set_target_properties( Boost::log PROPERTIES INTERFACE_COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK" )
  set_target_properties( Boost::log_setup PROPERTIES INTERFACE_COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK" )
endif()

#########################
## Google Test
#########################
  
if(BUILD_TESTS)  # We only care about GTEST_ROOT if tests are being built
  
  if( NOT DEFINED GTEST_ROOT AND
      NOT DEFINED GOOGLETEST_ROOT AND
      NOT DEFINED ENV{GTEST_ROOT} AND
      NOT DEFINED ENV{GOOGLETEST_ROOT}
   )
    message( FATAL_ERROR
      "Neither of the variables GTEST_ROOT or GOOGLETEST_ROOT is set. Make sure one of these variables contains the location of a usable GTest installation for this build, then re-run CMake."
      )
  endif()
  
  find_package(GTest REQUIRED)
  
endif() 

########################
## HDF5 IOM
########################
if( Faodel_ENABLE_IOM_HDF5 )
  find_package( HDF5 COMPONENTS C MODULE )
  if( HDF5_FOUND AND NOT TARGET Faodel::HDF5 )
    add_library( Faodel::HDF5 INTERFACE IMPORTED )
    target_compile_definitions( Faodel::HDF5 INTERFACE ${HDF5_DEFINITIONS} )
    target_link_libraries( Faodel::HDF5 INTERFACE ${HDF5_LIBRARIES} )
    target_include_directories( Faodel::HDF5 INTERFACE ${HDF5_INCLUDE_DIRS} )
    set( FAODEL_HAVE_HDF5 TRUE )
    message( STATUS "Will build HDF5 IOM, Faodel_ENABLE_IOM_HDF5 set and HDF5 found" )
  else()
    message( STATUS "Cannot build HDF5 IOM as requested, HDF5 not found. Set HDF5_ROOT" )
  endif()
endif()


########################
## LevelDB IOM
########################
if( Faodel_ENABLE_IOM_LEVELDB )
  find_package( leveldb NO_MODULE )
  if( TARGET leveldb::leveldb )
    set( FAODEL_HAVE_LEVELDB TRUE )
    message( STATUS "Will build LevelDB IOM, Faodel_ENABLE_IOM_LEVELDB set and LevelDB found" )
  else()
    message( STATUS "Cannot build LevelDB IOM as requested, LevelDB not found. Set CMAKE_PREFIX_PATH or leveldb_DIR" )
  endif()
endif()
    


#######################
## Cassandra IOM
#######################

if( Faodel_ENABLE_IOM_CASSANDRA )
  
  # The DataStax Cassandra C/C++ driver generates a pkgconfig module, so let's try to find that.
  set( PKG_CONFIG_USE_CMAKE_PREFIX_PATH 1 )
  
  pkg_search_module( cassandra_pc cassandra REQUIRED )
  
  if( cassandra_pc_FOUND )
    set( CASSANDRA_FOUND TRUE )
    set( FAODEL_HAVE_CASSANDRA TRUE )
    
    add_library( Faodel::Cassandra INTERFACE IMPORTED )
    target_include_directories( Faodel::Cassandra INTERFACE ${cassandra_pc_INCLUDE_DIRS} )
    target_link_libraries( Faodel::Cassandra INTERFACE ${cassandra_pc_LDFLAGS} )
    target_compile_definitions( Faodel::Cassandra INTERFACE ${cassandra_pc_CFLAGS_OTHER} )
    
    message( STATUS "Will build Cassandra IOM, Faodel_ENABLE_IOM_CASSANDRA set and Cassandra driver found" )
  else()
    message( STATUS "Cannot build Cassandra IOM as requested, Cassandra driver not found. Set CMAKE_PREFIX_PATH" )
  endif()

endif()
  
########################
## MPI
########################

#
# Provide the user a choice of building with MPI or not. Synthesize import targets if they
# don't get defined by FindMPI.cmake.
#
# Since building with MPI is usually pretty complicated and handled by a compiler wrapper,
# we assume here that any higher-level compilation driver has defined CMAKE_CXX_COMPILER
# appropriately and we don't check for anything else.
#
if( Faodel_ENABLE_MPI_SUPPORT )
  
  find_package(MPI REQUIRED)

  message(STATUS "MPI_C_FOUND = ${MPI_C_FOUND}")
  if( MPI_C_FOUND AND NOT TARGET MPI::MPI_C )
    message( STATUS "Defining MPI::MPI_C import target since FindMPI.cmake did not define it" )
    add_library( MPI::MPI_C INTERFACE IMPORTED )
    set_target_properties( MPI::MPI_C PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MPI_C_INCLUDE_PATH}"
      INTERFACE_LINK_LIBRARIES "${MPI_C_LIBRARIES}"
      INTERFACE_COMPILE_OPTIONS "${MPI_C_COMPILE_OPTIONS}"
      INTERFACE_COMPILE_DEFINITIONS "${MPI_C_COMPILE_DEFINITIONS}"
      )
  endif()
  
  message(STATUS "MPI_CXX_FOUND = ${MPI_CXX_FOUND}")
  if( MPI_CXX_FOUND AND NOT TARGET MPI::MPI_CXX )
    message( STATUS "Defining MPI::MPI_CXX import target since FindMPI.cmake did not define it" )
    add_library( MPI::MPI_CXX INTERFACE IMPORTED )
    set_target_properties( MPI::MPI_CXX PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MPI_CXX_INCLUDE_PATH}"
      INTERFACE_LINK_LIBRARIES "${MPI_CXX_LIBRARIES}"
      INTERFACE_COMPILE_OPTIONS "${MPI_CXX_COMPILE_OPTIONS}"
      INTERFACE_COMPILE_DEFINITIONS "${MPI_CXX_COMPILE_DEFINITIONS}"
      )
  endif()
  
endif()


##############################
#
# Stanza 3 : Determine Faodel's network library 
#
##############################
# When we're outside of the Faodel project (ie, this file has been installed and
# the user is importing Faodel to use in their project), we can pull in some of
# our settings from the installed lib when the user hasn't defined them.
if( NOT "${PROJECT_NAME}" STREQUAL "Faodel" )

if( "${Faodel_NETWORK_LIBRARY}" STREQUAL "" )
    set(Faodel_NETWORK_LIBRARY ${Faodel_BUILT_NETWORK_LIBRARY})
  endif()
  
endif()

if (Faodel_NETWORK_LIBRARY STREQUAL "libfabric")

  set( PKG_CONFIG_USE_CMAKE_PREFIX_PATH 1 )

  pkg_search_module( Libfabric_pc libfabric REQUIRED )
  
  if( Libfabric_pc_FOUND )
    set( LIBFABRIC_FOUND TRUE )
    
    # This pretty closely mimics what happens in FindPkgConfig.cmake from the CMake distro.
    # The only significant difference (should be) is that we get the STATIC versions of
    # information from the .pc file, since we always want to link network libs statically.
    # pkg_search_module( ... IMPORTED_TARGET ) sets up a target that only works with shared libraries.
    # It's possible on some platforms that the STATIC variables won't be created
    # (because pkg-config --static returns nothing, which might happen on platforms where no dynamic
    # linking is ever done), so check for empty vars and fall back to the non-static variables if necessary.

    add_library( Libfabric INTERFACE IMPORTED )
    if( Libfabric_pc_STATIC_LDFLAGS )
      # Assume we have STATIC information 
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Libfabric_pc_STATIC_INCLUDE_DIRS}
        )
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_LINK_LIBRARIES ${Libfabric_pc_STATIC_LDFLAGS}
        )
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_COMPILE_OPTIONS ${Libfabric_pc_STATIC_CFLAGS_OTHER}
        )
    else()	
      # Unlikely that we have any STATIC info, use the regular variabes
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Libfabric_pc_INCLUDE_DIRS}
        )
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_LINK_LIBRARIES ${Libfabric_pc_LDFLAGS}
        )
      set_property( TARGET Libfabric
        PROPERTY INTERFACE_COMPILE_OPTIONS ${Libfabric_pc_CFLAGS_OTHER}
        )
    endif()
    
    LIST( APPEND FaodelNetlib_TARGETS Libfabric )
    set( PKGCONFIG_REQUIRES "${PKGCONFIG_REQUIRES} libfabric" )
    message( STATUS "Found Libfabric, target appended to FaodelNetlib_TARGETS" )
  endif()

elseif( Faodel_NETWORK_LIBRARY STREQUAL "nnti" )

  set( NET_USE_NNTI 1 )
  set( BUILD_NNTI TRUE )
  LIST( APPEND FaodelNetlib_TARGETS nnti )
  set( PKGCONFIG_NNTI "-lnnti" )

  # For NNTI we need to locate UGNI, IBVerbs, and CrayDRC

  pkg_check_modules(UGNI_PC cray-ugni)

  SET(UGNI_FOUND ${UGNI_PC_FOUND})
  
  if(UGNI_PC_FOUND)
    foreach(ugnilib ${UGNI_PC_LIBRARIES})
      find_library(${ugnilib}_LIBRARY NAMES ${ugnilib} HINTS ${UGNI_PC_LIBRARY_DIRS})
      if (${ugnilib}_LIBRARY)
	LIST(APPEND UGNI_LIBRARIES ${${ugnilib}_LIBRARY})
	add_library( ${ugnilib} IMPORTED UNKNOWN )
	set_property( TARGET ${ugnilib} PROPERTY IMPORTED_LOCATION ${${ugnilib}_LIBRARY} )
	set_property( TARGET ${ugnilib} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${UGNI_PC_INCLUDE_DIRS} )
	LIST( APPEND UGNI_TARGETS ${ugnilib} )
      endif (${ugnilib}_LIBRARY)
    endforeach(ugnilib)
    SET(UGNI_INCLUDE_DIRS ${UGNI_PC_INCLUDE_DIRS})
    LIST( APPEND FaodelNetlib_TARGETS ${UGNI_TARGETS} )
    SET( NNTI_HAVE_UGNI 1 )
    SET( FAODEL_HAVE_UGNI 1 )
    set( PKGCONFIG_REQUIRES "${PKGCONFIG_REQUIRES} cray-ugni" )
  endif(UGNI_PC_FOUND)

  # IBVERBS

  # - Try to find IBVerbs
  # Once done this will define
  #  IBVerbs_FOUND - System has IBVerbs
  #  IBVerbs_INCLUDE_DIRS - The IBVerbs include directories
  #  IBVerbs_LIBRARIES - The libraries needed to use IBVerbs
  #  IBVerbs_DEFINITIONS - Compiler switches required for using IBVerbs
  
  find_path(IBVerbs_INCLUDE_DIR infiniband/verbs.h )
  
  find_library(IBVerbs_LIBRARY NAMES ibverbs )
  
  set(IBVerbs_LIBRARIES ${IBVerbs_LIBRARY} )
  set(IBVerbs_INCLUDE_DIRS ${IBVerbs_INCLUDE_DIR} )
  
  # set IBVerbs_FOUND to TRUE
  # if all listed variables are TRUE
  find_package_handle_standard_args(
    IBVerbs
    FOUND_VAR     IBVerbs_FOUND
    REQUIRED_VARS IBVerbs_LIBRARY IBVerbs_INCLUDE_DIR)
  
  if( IBVerbs_FOUND )
    add_library( IBVerbs IMPORTED UNKNOWN )
    set_property( TARGET IBVerbs PROPERTY IMPORTED_LOCATION ${IBVerbs_LIBRARY} )
    set_property( TARGET IBVerbs PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${IBVerbs_INCLUDE_DIR} )
    LIST( APPEND FaodelNetlib_TARGETS IBVerbs )
    SET( NNTI_HAVE_IBVERBS 1 )
    SET( FAODEL_HAVE_IBVERBS 1 )
    set( PKGCONFIG_LIBS_PRIVATE "${PKGCONFIG_LIBS_PRIVATE} ${IBVerbs_LIBRARY}" )
    set( PKGCONFIG_CFLAGS "${PKGCONFIG_CFLAGS} -I${IBVerbs_INCLUDE_DIR}" )
  endif()
  
  mark_as_advanced(IBVerbs_INCLUDE_DIR IBVerbs_LIBRARY )

  #
  # CrayDRC
  #

  pkg_check_modules(DRC_PC cray-drc)
  SET(DRC_FOUND ${DRC_PC_FOUND})
  if(DRC_PC_FOUND)
    foreach(drclib ${DRC_PC_LIBRARIES})
      find_library(${drclib}_LIBRARY NAMES ${drclib} HINTS ${DRC_PC_LIBRARY_DIRS})
      if (${drclib}_LIBRARY)
	add_library( ${drclib} IMPORTED UNKNOWN )
	set_property( TARGET ${drclib} PROPERTY IMPORTED_LOCATION ${${drclib}_LIBRARY} )
	set_property( TARGET ${drclib} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${DRC_PC_INCLUDE_DIRS}" )
	LIST( APPEND DRC_TARGETS ${drclib} )
	LIST( APPEND DRC_LIBRARIES ${${drclib}_LIBRARY})
      endif (${drclib}_LIBRARY)
    endforeach(drclib)
    LIST(APPEND DRC_INCLUDE_DIRS ${DRC_PC_INCLUDE_DIRS})
    LIST( APPEND FaodelNetlib_TARGETS ${DRC_TARGETS} )
    set( PKGCONFIG_REQUIRES "${PKGCONFIG_REQUIRES} cray-drc" )
  endif(DRC_PC_FOUND)

else()
   message(FATAL_ERROR "Faodel_NETWORK_LIBRARY must be defined as one of (nnti | libfabric).  The build cannot continue." )
endif()
