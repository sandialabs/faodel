
set(HEADERS
        Whookie.hh
  Server.hh

  client/Client.hh
  server/boost/connection.hpp
  server/boost/connection_manager.hpp
  server/boost/header.hpp
  server/boost/mime_types.hpp
  server/boost/reply.hpp
  server/boost/request_handler.hpp
  server/boost/request.hpp
  server/boost/request_parser.hpp
  server/boost/server.hpp  
)
set(HEADERS_PUBLIC
        Whookie.hh
  Server.hh
  client/Client.hh
)

set(SOURCES
  client/Client.cpp
  server/boost/connection.cpp
  server/boost/connection_manager.cpp
  server/boost/mime_types.cpp
  server/boost/reply.cpp
  server/boost/request_handler.cpp
  server/boost/request_parser.cpp
  server/boost/server.cpp
  server/ServerBoost.cpp      
)

# Everything now goes in whookie
add_library( whookie ${HEADERS} ${SOURCES} )
target_link_libraries( whookie Boost::boost )


install(TARGETS whookie
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR};${INCLUDE_INSTALL_DIR}/faodel"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  PATTERN "server" EXCLUDE
  PATTERN "tools" EXCLUDE
  )

