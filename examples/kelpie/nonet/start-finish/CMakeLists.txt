set(PROJECT_NAME ex_kelpie_start-finish)

set(HEADERS
)

set(SOURCES
    start-finish.cpp
)

# When doing a standalone build, use the Faodel::kelpie imported target
if( NOT DEFINED EXAMPLE_LIBS)
    set(EXAMPLE_LIBS Faodel::kelpie)
endif()

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLE_LIBS} )

install(TARGETS ${PROJECT_NAME}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)
