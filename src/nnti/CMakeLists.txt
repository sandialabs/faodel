
if ( NOT DEFINED NNTI_BUILD_TYPE )
    ## try defaulting to the CMake build type
    string( TOLOWER "${CMAKE_BUILD_TYPE}" lower_type )
    IF ( lower_type STREQUAL "debug" )
        set(NNTI_BUILD_TYPE "DEBUG")
    ELSEIF ( lower_type STREQUAL "release" )
        set(NNTI_BUILD_TYPE "RELEASE")
    ELSE()
        ## fallback is RELEASE
        set(NNTI_BUILD_TYPE "RELEASE")
    ENDIF()
endif()

string( TOLOWER "${NNTI_BUILD_TYPE}" lower_type )
if( lower_type STREQUAL "debug" )
   message(STATUS "This is an NNTI debug build.")
   message(STATUS "NNTI is building with debug logging.")
   message(STATUS "NNTI is building with all stats.")
   set(NNTI_ENABLE_DEBUG_LOGGING TRUE)
elseif( lower_type STREQUAL "release" )
   message(STATUS "This is an NNTI release build.")
   message(STATUS "NNTI is building without debug logging.")
   message(STATUS "NNTI is building with fast stats only.")
   set(NNTI_FAST_STATS_ONLY 1)
   set(NNTI_ENABLE_DEBUG_LOGGING FALSE)
elseif( lower_type STREQUAL "performance" )
   message(STATUS "This is an NNTI performance build.")
   message(STATUS "NNTI is building without debug logging.")
   message(STATUS "NNTI is building with no stats.")
   set(NNTI_NO_STATS 1)
   set(NNTI_ENABLE_DEBUG_LOGGING FALSE)
else()
  message(FATAL_ERROR "NNTI_BUILD_TYPE must be defined as one of DEBUG or RELEASE or PERFORMANCE")
endif()

if (NNTI_NO_STATS)
    unset(NNTI_ENABLE_STATS)
    unset(NNTI_ENABLE_FAST_STATS)
    unset(NNTI_ENABLE_SLOW_STATS)
elseif (NNTI_FAST_STATS_ONLY)
    set(NNTI_ENABLE_STATS 1)
    set(NNTI_ENABLE_FAST_STATS 1)
    unset(NNTI_ENABLE_SLOW_STATS)
else ()
    set(NNTI_ENABLE_STATS 1)
    set(NNTI_ENABLE_FAST_STATS 1)
    set(NNTI_ENABLE_SLOW_STATS 1)
endif ()


if (NNTI_DISABLE_IBVERBS_TRANSPORT)
    message(STATUS "IBVerbs transport explicitly disabled")
    set(NNTI_BUILD_IBVERBS 0)
else ()
    message(STATUS "IBVerbs_FOUND = ${IBVerbs_FOUND}")
    if (IBVerbs_FOUND)
      set(NNTI_BUILD_IBVERBS 1)
      LIST( APPEND NNTI_TRANSPORT_TARGETS IBVerbs )
    endif()
endif()

if (NNTI_DISABLE_UGNI_TRANSPORT)
    message(STATUS "UGNI transport explicitly disabled")
    set(NNTI_BUILD_UGNI 0)
else ()
    message(STATUS "UGNI_FOUND = ${UGNI_FOUND}")
    if (UGNI_FOUND)
	LIST( APPEND NNTI_TRANSPORT_TARGETS ${UGNI_TARGETS} )
    endif()
    message(STATUS "DRC_FOUND = ${DRC_FOUND}")
    if (DRC_FOUND)
	LIST( APPEND NNTI_TRANSPORT_TARGETS ${DRC_TARGETS} )
    endif()
    if( UGNI_FOUND AND DRC_FOUND )
        set( NNTI_BUILD_UGNI 1)
    endif()
endif()

if (NNTI_DISABLE_MPI_TRANSPORT)
    message(STATUS "MPI transport explicitly disabled")
    set(NNTI_BUILD_MPI 0)
else ()
    message(STATUS "MPI_C_FOUND = ${MPI_C_FOUND}")
    message(STATUS "MPI_CXX_FOUND = ${MPI_CXX_FOUND}")
    if( MPI_C_FOUND AND MPI_CXX_FOUND )
        set( NNTI_BUILD_MPI 1)
    endif()
endif()

# Put these in the parent scope so they can be used in the config summary
set(NNTI_BUILD_IBVERBS ${NNTI_BUILD_IBVERBS} PARENT_SCOPE)
set(NNTI_BUILD_UGNI    ${NNTI_BUILD_UGNI}    PARENT_SCOPE)
set(NNTI_BUILD_MPI     ${NNTI_BUILD_MPI}     PARENT_SCOPE)


# Feature Tests
include(nntiProbeGettid.cmake)
include(nntiProbeNetwork.cmake)
include(nntiProbeThreads.cmake)
include(nntiProbeTimers.cmake)
include(nntiProbeXDR.cmake)



## What to build ##############################################################

include(nntiProcessXDR.cmake)



set(HEADERS
  nnti.h
  nnti_transport.hpp
  transport_factory.hpp
  nnti_datatype.hpp
  nnti_buffer.hpp
  nnti_callback.hpp
  nnti_connection.hpp
  nnti_eq.hpp
  nnti_logger.hpp
  nnti_peer.hpp
  nnti_threads.h
  nnti_threads_types.h
  nnti_op.hpp
  nnti_packable.h
  nnti_pid.hpp
  nnti_types.h
  nnti_url.hpp
  nnti_util.hpp
  nnti_wid.hpp
  nnti_wr.hpp
  nnti_xdr.h
  transports/base/base_transport.hpp
  transports/null/null_transport.hpp
)
if( NNTI_BUILD_IBVERBS )
set(HEADERS
  ${HEADERS}
  transports/ibverbs/ibverbs_atomic_op.hpp
  transports/ibverbs/ibverbs_buffer.hpp
  transports/ibverbs/ibverbs_cmd_buffer.hpp
  transports/ibverbs/ibverbs_cmd_msg.hpp
  transports/ibverbs/ibverbs_cmd_op.hpp
  transports/ibverbs/ibverbs_connection.hpp
  transports/ibverbs/ibverbs_peer.hpp
  transports/ibverbs/ibverbs_rdma_op.hpp
  transports/ibverbs/ibverbs_transport.hpp
)
endif()
if( NNTI_BUILD_UGNI )
set(HEADERS
  ${HEADERS}
  transports/ugni/ugni_atomic_op.hpp
  transports/ugni/ugni_buffer.hpp
  transports/ugni/ugni_cmd_msg.hpp
  transports/ugni/ugni_cmd_op.hpp
  transports/ugni/ugni_connection.hpp
  transports/ugni/ugni_mailbox.hpp
  transports/ugni/ugni_peer.hpp
  transports/ugni/ugni_rdma_op.hpp
  transports/ugni/ugni_transport.hpp
)
endif()
if( NNTI_BUILD_MPI )
set(HEADERS
  ${HEADERS}
  transports/mpi/mpi_buffer.hpp
  transports/mpi/mpi_cmd_buffer.hpp
  transports/mpi/mpi_cmd_msg.hpp
  transports/mpi/mpi_cmd_op.hpp
  transports/mpi/mpi_connection.hpp
  transports/mpi/mpi_peer.hpp
  transports/mpi/mpi_transport.hpp
)
endif()

set(HEADERS_PUBLIC
  nnti.h
  nnti_buffer.hpp
  nnti_transport.hpp
  nnti_logger.h
  nnti_logger.hpp
  nnti_packable.h
  nnti_types.h
  nnti_datatype.hpp
  nnti_callback.hpp
  nnti_connection.hpp
  nnti_peer.hpp
  nnti_pid.hpp
  nnti_threads.h
  nnti_threads_types.h
  nnti_url.hpp
  nnti_util.hpp
  nnti_vector.hpp
  nnti_wid.hpp
  nnti_wr.hpp
  nnti_xdr.h
  transport_factory.hpp
)

set(SOURCES
  nnti.cpp
  transport_factory.cpp
  nnti_buffer.cpp
  nnti_connection.cpp
  nnti_logger.cpp
  nnti_op.cpp
  nnti_packable.c
  nnti_pid.cpp
  nnti_threads.cpp
  nnti_url.cpp
  nnti_wid.cpp
  transports/base/base_transport.cpp
)
if( NNTI_BUILD_IBVERBS )
set(SOURCES
  ${SOURCES}
  transports/ibverbs/ibverbs_buffer.cpp
  transports/ibverbs/ibverbs_cmd_buffer.cpp
  transports/ibverbs/ibverbs_cmd_msg.cpp
  transports/ibverbs/ibverbs_transport.cpp
)
endif()
if( NNTI_BUILD_UGNI )
set(SOURCES
  ${SOURCES}
  transports/ugni/ugni_buffer.cpp
  transports/ugni/ugni_cmd_msg.cpp
  transports/ugni/ugni_cmd_op.cpp
  transports/ugni/ugni_connection.cpp
  transports/ugni/ugni_mailbox.cpp
  transports/ugni/ugni_transport.cpp
)
endif()
if( NNTI_BUILD_MPI )
set(SOURCES
  ${SOURCES}
  transports/mpi/mpi_buffer.cpp
  transports/mpi/mpi_cmd_buffer.cpp
  transports/mpi/mpi_cmd_msg.cpp
  transports/mpi/mpi_transport.cpp
)
endif()

#
# If no transport was selected, this build of NNTI should succeed but will not
# be usable for communication. Warn the user.
#
if( NOT NNTI_BUILD_UGNI AND
    NOT NNTI_BUILD_IBVERBS AND
    NOT NNTI_BUILD_MPI )
  message( WARNING
    "NNTI will be built without a network transport. While this may be useful to test the build process, NNTI won't be usable for communication. Faodel components which depend on NNTI may fail to build, as they typically assume NNTI has been built with at least one transport. You should examine your build configuration to make sure this is your intended result." )
endif()


LIST( APPEND NNTI_imports webhook common sbl Boost::atomic Boost::log Boost::log_setup )
if( Faodel_ENABLE_MPI_SUPPORT )
  LIST( APPEND NNTI_imports MPI::MPI_CXX )
endif()
LIST( APPEND NNTI_imports ${NNTI_TRANSPORT_TARGETS} )

nntiProcessXDR(${CMAKE_CURRENT_SOURCE_DIR}/nnti_packable.x ${CMAKE_CURRENT_SOURCE_DIR})

set_source_files_properties(transport_factory.cpp PROPERTIES COMPILE_FLAGS "-O0 -g")

add_library(nnti ${HEADERS} ${SOURCES})
target_link_libraries( nnti ${NNTI_imports} )
add_dependencies(nnti generate-nnti_packable)


install(TARGETS nnti
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( FILES ${HEADERS_PUBLIC}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/nnti )

#
# Create header with flags that can be used by NessieShim
#
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nntiConfig.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/nntiConfig.h"
    @ONLY
    )

# ... puts nntiConfig.h in include/${PROJECT_NAME}
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nntiConfig.h"
    DESTINATION include/faodel/nnti
    )
#-----------------------------------------
