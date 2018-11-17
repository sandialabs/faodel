include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

set(HEADERS
  BackBurner.hh
  Bootstrap.hh
  BootstrapInterface.hh
  BootstrapInternal.hh
  Bucket.hh
  Configuration.hh
  Debug.hh
  FaodelTypes.hh
  Common.hh
  InfoInterface.hh
  LoggingInterface.hh
  LoggingInterfaceMacros.hh
  MutexWrapper.hh
  NodeID.hh
  ResourceURL.hh
  SerializationHelpers.hh
  StringHelpers.hh
)

# todo: slim this down to the real list
set(HEADERS_PUBLIC
    ${HEADERS})

set(SOURCES
  BackBurner.cpp
  Bootstrap.cpp
  Bucket.cpp
  Configuration.cpp
  Debug.cpp
  LoggingInterface.cpp
  MutexWrapper.cpp
  NodeID.cpp
  ResourceURL.cpp
  StringHelpers.cpp
)

add_library( common STATIC ${HEADERS} ${SOURCES} )
set_target_properties( common PROPERTIES
  OUTPUT_NAME "faodel-common"
  )
target_link_libraries( common sbl )

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/commonConfig.h.in
  "${CMAKE_CURRENT_SOURCE_DIR}/commonConfig.h"
  @ONLY
  )
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/commonConfig.h"
  DESTINATION include/faodel/common
  )

install(TARGETS common
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  )

