

# Feature Tests
include( sblProbeGettid.cmake )

## What to build ##############################################################


set(HEADERS
        sbl_boost_headers.hh
        sbl_types.hh
        sbl_source.hh
        sbl_stream.hh
        sbl_logger.hh
)
set(HEADERS_PUBLIC
        sbl_boost_headers.hh
        sbl_types.hh
        sbl_source.hh
        sbl_stream.hh
        sbl_logger.hh
)

set(SOURCES
  sbl_source.cpp
  sbl_stream.cpp
  sbl_logger.cpp
  )

add_library( sbl ${HEADERS} ${SOURCES})
target_link_libraries( sbl Boost::log )

install(TARGETS sbl
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  PATTERN "scripts" EXCLUDE
  PATTERN "tests" EXCLUDE
  )

