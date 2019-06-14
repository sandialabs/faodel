include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

set(HEADERS
  BackBurner.hh
  MPISyncStart.hh
)

set(HEADERS_PUBLIC
  ${HEADERS}
)

set(SOURCES
  BackBurner.cpp
  MPISyncStart.cpp
)

LIST( APPEND Services_imports whookie common )
if( Faodel_ENABLE_MPI_SUPPORT )
  LIST( APPEND Services_imports MPI::MPI_CXX )
endif()

add_library( services ${HEADERS} ${SOURCES} )
set_target_properties( services PROPERTIES
  OUTPUT_NAME "faodel-services"
)
target_link_libraries( services PUBLIC ${Services_imports} )


install(TARGETS services
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

