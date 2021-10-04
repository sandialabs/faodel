set(PROJECT_NAME ex_common_data-types)


set(SOURCES
   nodeid_example.cpp
   resourceurl_example.cpp
   main.cpp
)

# When doing a standalone build, use the Faodel::common imported target
if(NOT DEFINED EXAMPLE_LIBS)
    set( EXAMPLE_LIBS Faodel::common )
endif()

add_executable( ${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties( ${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries( ${PROJECT_NAME} ${EXAMPLE_LIBS} )

install(TARGETS ${PROJECT_NAME}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)