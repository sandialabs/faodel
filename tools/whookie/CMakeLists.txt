
set(
  SOURCES
        whookie.cpp
)

add_executable( whookie ${SOURCES} )
set_target_properties( whookie PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries( whookie webhook common) #Faodel::kelpie )

install(TARGETS whookie
        EXPORT faodelTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  )