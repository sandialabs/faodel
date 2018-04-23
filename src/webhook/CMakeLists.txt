
set(HEADERS
  WebHook.hh
  Server.hh

  client/Client.hh
  common/QuickHTML.hh
  common/ReplyStream.hh
  
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
  WebHook.hh
  Server.hh
  client/Client.hh
  common/QuickHTML.hh
  common/ReplyStream.hh  
)

set(SOURCES
  client/Client.cpp
  common/QuickHTML.cpp
  common/ReplyStream.cpp

  server/boost/connection.cpp
  server/boost/connection_manager.cpp
  server/boost/mime_types.cpp
  server/boost/reply.cpp
  server/boost/request_handler.cpp
  server/boost/request_parser.cpp
  server/boost/server.cpp
  server/ServerBoost.cpp      
)

# Everything now goes in webhook
add_library( webhook STATIC ${HEADERS} ${SOURCES} )
target_link_libraries( webhook Boost::boost )


# Standalone tool for grabbing data
add_executable(wh_grabData tools/grabData.cpp client/Client.cpp)
target_link_libraries(wh_grabData PUBLIC common )
install(TARGETS wh_grabData
        EXPORT webhookTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)

install(TARGETS webhook
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR};${INCLUDE_INSTALL_DIR}/faodel"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  PATTERN "server" EXCLUDE
  PATTERN "tools" EXCLUDE
  )

