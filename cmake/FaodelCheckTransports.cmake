
function( check_preferred_transports )
  #
  # First we check for disabled transports that probably should be enabled
  #
  if( NOT "${NNTI_BUILD_UGNI}" )
    if( "${CMAKE_SYSTEM_NAME}" MATCHES "CrayLinuxEnvironment" )
      message( STATUS "======================================================================" )
      message( STATUS "                                 WARNING                              " )
      message( STATUS "======================================================================" )
      message( STATUS "   This is a Cray System, but the UGNI transport is disabled.         " )
      found_status( "libugni"     UGNI_FOUND "v" "${UGNI_PC_VERSION}" )
      found_status( "Cray DRC"    DRC_FOUND )
      message( STATUS "======================================================================" )
      message( STATUS "" )
    endif()
  endif()

  if( NOT "${NNTI_BUILD_IBVERBS}" )
    find_program(IBV_DEVINFO ibv_devinfo )
    if( IBV_DEVINFO )
      execute_process( COMMAND ${IBV_DEVINFO} RESULT_VARIABLE DEVINFO_RESULT OUTPUT_VARIABLE DEVINFO_STDOUT ERROR_VARIABLE DEVINFO_STDERR )
      if( ${DEVINFO_RESULT} EQUAL 0 )
        message( STATUS "======================================================================" )
        message( STATUS "                                 WARNING                              " )
        message( STATUS "======================================================================" )
        message( STATUS "This system has an IB device, but the ibverbs transport is disabled.  " )
        found_status( "ibverbs"     IBVerbs_FOUND )
        if( ${NNTI_DISABLE_IBVERBS_TRANSPORT} )
          message( STATUS "   IBVerbs Transport explicitly disabled" )
        else()
          message( STATUS "   Not building the IBVerbs Transport" )
        endif()
        message( STATUS "======================================================================" )
        message( STATUS "" )
      endif()
    endif()
  endif()

  #
  # Now we check for enabled transports that probably shouldn't be
  #
  if( "${NNTI_BUILD_IBVERBS}" )
    find_program(IBV_DEVINFO ibv_devinfo )
    if( IBV_DEVINFO )
      execute_process( COMMAND ${IBV_DEVINFO} RESULT_VARIABLE DEVINFO_RESULT OUTPUT_VARIABLE DEVINFO_STDOUT ERROR_VARIABLE DEVINFO_STDERR )
      if( NOT ${DEVINFO_RESULT} EQUAL 0 )
        message( STATUS "======================================================================" )
        message( STATUS "                                 WARNING                              " )
        message( STATUS "======================================================================" )
        message( STATUS "The ibverbs transport is enabled, but ibv_devinfo didn't report any   " )
        message( STATUS "device information.  This can happen if the ibverbs headers, libraries" )
        message( STATUS "and tools are installed, but the system doesn't have any IB hardware. " )
        message( STATUS "To disable the ibverbs transport, add this to your cmake command:     " )
        message( STATUS "  -DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE                          " )
        message( STATUS "======================================================================" )
        message( STATUS "" )
      endif()
    else()
        message( STATUS "======================================================================" )
        message( STATUS "                                 WARNING                              " )
        message( STATUS "======================================================================" )
        message( STATUS "The ibverbs transport is enabled, but ibv_devinfo couldn't be found.  " )
        message( STATUS "This can happen if the ibverbs headers and libraries are installed,   " )
        message( STATUS "but the runtime tools are not installed.  The tools are not required  " )
        message( STATUS "to build and use the ibverbs transport, but this could indicate a     " )
        message( STATUS "partial installation or missing IB hardware.                          " )
        message( STATUS "To disable the ibverbs transport, add this to your cmake command:     " )
        message( STATUS "  -DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE                          " )
        message( STATUS "======================================================================" )
        message( STATUS "" )
    endif()
  endif()
endfunction()
