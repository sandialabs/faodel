include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

set(HEADERS
  Bootstrap.hh
  BootstrapInterface.hh
  BootstrapImplementation.hh
  Bucket.hh
  Configuration.hh
  Debug.hh
  DirectoryInfo.hh
  FaodelTypes.hh
  Common.hh
  InfoInterface.hh
  LoggingInterface.hh
  LoggingInterfaceMacros.hh
  MutexWrapper.hh
  NodeID.hh
  QuickHTML.hh
  ReplyStream.hh
  ResourceURL.hh
  SerializationHelpers.hh
  StringHelpers.hh
)

# todo: slim this down to the real list
set(HEADERS_PUBLIC
    ${HEADERS})

set(SOURCES
  Bootstrap.cpp
  BootstrapImplementation.cpp
  Bucket.cpp
  Configuration.cpp
  DirectoryInfo.cpp
  Debug.cpp
  LoggingInterface.cpp
  MutexWrapper.cpp
  NodeID.cpp
  QuickHTML.cpp
  ReplyStream.cpp
  ResourceURL.cpp
  StringHelpers.cpp
)

add_library( common ${HEADERS} ${SOURCES} )
set_target_properties( common PROPERTIES
  OUTPUT_NAME "faodel-common"
  )
target_link_libraries( common sbl )




install(TARGETS common
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  )

