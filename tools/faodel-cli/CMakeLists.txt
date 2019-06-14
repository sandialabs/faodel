set( 
  SOURCES
  build_info.cpp
  config_info.cpp
  dirman_server.cpp
  kelpie_client.cpp
  kelpie_server.cpp
  resource.cpp
  whookie_client.cpp
  faodel_cli.cpp
  #..insert others here
)

if( NNTI_BUILD_IBVERBS )
 set(
  SOURCES
  ${SOURCES}
  build_info_ib.cpp
)
endif()

add_executable(
    faodel
    ${SOURCES}
)
set_target_properties(
    faodel
    PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(
    faodel
    kelpie
    ${FaodelNetlib_TARGETS} )

install(
    TARGETS faodel
    EXPORT faodelTargets
    RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)