set( 
  SOURCES 
  faodel_info.cpp 
  cmake_info.cpp
)

if( NNTI_BUILD_IBVERBS )
set(
  SOURCES
  ${SOURCES}
  ib_info.cpp
)
endif()

if(Faodel_ENABLE_MPI_SUPPORT)
  add_executable(
    faodel-info
    ${SOURCES}
    )
  set_target_properties(
    faodel-info
    PROPERTIES LINKER_LANGUAGE CXX )
  target_link_libraries(
    faodel-info
    kelpie
    ${FaodelNetlib_TARGETS}
    MPI::MPI_CXX )

  install(TARGETS faodel-info
          EXPORT faodelTargets
          RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  )
endif( Faodel_ENABLE_MPI_SUPPORT)