
if(Faodel_ENABLE_MPI_SUPPORT)

  set(
    SOURCES
          kelpie_server_main.cpp
  )

  add_executable( kelpie-server ${SOURCES} )
  set_target_properties( kelpie-server PROPERTIES LINKER_LANGUAGE CXX )

  target_link_libraries( kelpie-server kelpie) #Faodel::kelpie )

  install(
     TARGETS kelpie-server
     EXPORT faodelTargets
     RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  )


endif(Faodel_ENABLE_MPI_SUPPORT)
