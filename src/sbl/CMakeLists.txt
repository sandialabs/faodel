

# Feature Tests
include( sblProbeGettid.cmake )

## What to build ##############################################################


set(HEADERS
  sbl_boost_headers.hpp
  sbl_types.hpp
  sbl_source.hpp
  sbl_stream.hpp
  sbl_logger.hpp
)
set(HEADERS_PUBLIC
  sbl_boost_headers.hpp
  sbl_types.hpp
  sbl_source.hpp
  sbl_stream.hpp
  sbl_logger.hpp
)

set(SOURCES
  sbl_source.cpp
  sbl_stream.cpp
  sbl_logger.cpp
  )

add_library( sbl STATIC ${HEADERS} ${SOURCES})
target_link_libraries( sbl Boost::log )

install(TARGETS sbl
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hpp"
  PATTERN "scripts" EXCLUDE
  PATTERN "tests" EXCLUDE
  )

#
# Create header with flags that can be used by NessieShim
#
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/sblConfig.h.in
  "${CMAKE_CURRENT_SOURCE_DIR}/sblConfig.h"
  @ONLY
  )

install( FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/sblConfig.h"
  DESTINATION include/faodel/sbl
  )
