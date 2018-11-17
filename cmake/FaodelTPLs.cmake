#####################################
# Get the Kokkos target
#####################################

# This does NOT call out to a FindKokkos.cmake module. CMake CONFIG mode.
find_package(Kokkos
  HINTS ${KOKKOS_ROOT} ${KOKKOS} $ENV{KOKKOS_ROOT} $ENV{KOKKOS}
  NO_MODULE
  QUIET )

if( Kokkos_FOUND )
  # This indicates we found a config file, possibly from a Trilinos installation
  
  # Use the same compilers as Trilinos
  # WHen would we ever want to allow the compiler to be changed underneath us?
  SET(CMAKE_CXX_COMPILER     ${Kokkos_CXX_COMPILER}     )
  SET(CMAKE_C_COMPILER       ${Kokkos_C_COMPILER}       )
  SET(CMAKE_Fortran_COMPILER ${Kokkos_Fortran_COMPILER} )
  
  SET(CMAKE_CXX_FLAGS     "${Kokkos_CXX_COMPILER_FLAGS}     ${CMAKE_CXX_FLAGS}"     )
  SET(CMAKE_C_FLAGS       "${Kokkos_C_COMPILER_FLAGS}       ${CMAKE_C_FLAGS}"       )
  SET(CMAKE_Fortran_FLAGS "${Kokkos_Fortran_COMPILER_FLAGS} ${CMAKE_Fortran_FLAGS}" )
  
else()
  
  # No config file found, so we go looking for a standalone Kokkos install
  # (likely from a checkout of the public Kokkos github repo).

  # Try pkg_config first  
  pkg_check_modules( Kokkos QUIET Kokkos )

  if( NOT Kokkos_FOUND )

    # Try to find it ourselves
    find_path( Kokkos_INCLUDE_DIRS
      NAMES KokkosCore_config.h
      PATHS ${KOKKOS_ROOT}/include $ENV{KOKKOS}/include $ENV{Kokkos}/include
      )
    
    find_library( Kokkos_LIBRARIES
      NAMES kokkos
      PATHS ${KOKKOS_ROOT}/lib $ENV{KOKKOS}/lib $ENV{Kokkos}/lib
      )
  endif()

  # At this point, we won't try anything else to find it, so decide y/n
  find_package_handle_standard_args( Kokkos
    FOUND_VAR Kokkos_FOUND
    REQUIRED_VARS Kokkos_LIBRARIES Kokkos_INCLUDE_DIRS
    )

endif()

if( Kokkos_FOUND )
    
  if( NOT TARGET Kokkos::Kokkos )
    add_library( Kokkos::Kokkos UNKNOWN IMPORTED )
    set_target_properties( Kokkos::Kokkos PROPERTIES
      IMPORTED_LOCATION "${Kokkos_LIBRARIES}"
      IMPORTED_LINK_INTERFACE "CXX"
      INTERFACE_INCLUDE_DIRECTORIES "${Kokkos_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${Kokkos_DEFINITIONS}"
      )
  endif()
  
  mark_as_advanced( Kokkos_INCLUDE_DIRS Kokkos_LIBRARIES )

  message( STATUS "\nFound Kokkos!  Here are the details: ")
  message( STATUS "   Kokkos_LIBRARIES        = ${Kokkos_LIBRARIES}" )
  message( STATUS "   Kokkos_INCLUDE_DIRS     = ${Kokkos_INCLUDE_DIRS}" )
  
endif()
    
##########################
## Get the Boost targets
##########################
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
    "Imported targets for Boost were not created and FAODEL subprojects cannot be built. 
This can happen when the located Boost installation is newer than your version of CMake,
and your CMake's FindBoost.cmake doesn't have dependency information for that Boost version. If this is the case, you should
either switch to a newer CMake or an older Boost."
    )
endif()

if( NOT Boost_USE_STATIC_LIBS )
  set_target_properties( Boost::log PROPERTIES INTERFACE_COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK" )
  set_target_properties( Boost::log_setup PROPERTIES INTERFACE_COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK" )
endif()

#########################
## Google Test
#########################
find_package(GTest REQUIRED)

########################
## MPI
########################

#
# Provide the user a choice of building with MPI or not. Synthesize import targets.
#
# This will need to change once we move to a default CMake whose FindMPI.cmake defines its own
# import targets.
if( USE_MPI )
 
  find_package(MPI REQUIRED)
  message(STATUS "MPI_C_FOUND = ${MPI_C_FOUND}")
  if (MPI_C_FOUND)
    add_library( MPI_C INTERFACE IMPORTED )
    set_target_properties( MPI_C PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MPI_C_INCLUDE_PATH}" )
    set_target_properties( MPI_C PROPERTIES INTERFACE_LINK_LIBRARIES "${MPI_C_LIBRARIES}" )
    set(${PROJECT_NAME}_HAVE_MPI 1)
  endif()
  message(STATUS "MPI_CXX_FOUND = ${MPI_CXX_FOUND}")
  if (MPI_CXX_FOUND)
    add_library( MPI_CXX INTERFACE IMPORTED )
    set_target_properties( MPI_CXX PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MPI_CXX_INCLUDE_PATH}" )
    set_target_properties( MPI_CXX PROPERTIES INTERFACE_LINK_LIBRARIES "${MPI_CXX_LIBRARIES}" )
    set(${PROJECT_NAME}_HAVE_MPI 1)
  endif()

endif()

##############################
#
# Stanza 3 : Determine Faodel's network library 
#
##############################
if( "${NETWORK_LIBRARY}" STREQUAL "" )
  MESSAGE( STATUS "NETWORK_LIBRARY is not set, using nnti" )
  set( NETWORK_LIBRARY "nnti" )
endif()

if (NETWORK_LIBRARY STREQUAL "libfabric")

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
      # Unlikely that we have any STATIC info, use the regularr variabes
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
    set( PKGCONFIG_REQUIRES "libfabric" )
    message( STATUS "Found Libfabric, target appended to FaodelNetlib_TARGETS" )
  endif()

elseif( NETWORK_LIBRARY STREQUAL "nnti" )

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
   
elseif(NETWORK_LIBRARY STREQUAL "localmem")
   message(WARNING "NETWORK_LIBRARY is set to localmem. Build will NOT try to use nnti or libfabric")
else()
   message(FATAL_ERROR "NETWORK_LIBRARY should be defined as one of (nnti | libfabric | localmem ). Defaulting to localmem." )
endif()
