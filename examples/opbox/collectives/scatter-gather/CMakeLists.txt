set(PROJECT_NAME ex_opbox_scatter-gather)

set(HEADERS
   OpScatter.hh
   OpGather.hh
)

set(SOURCES
   OpScatter.cpp
   OpGather.cpp
   scatter_gather.cpp
)


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLE_LIBS})

install(TARGETS ${PROJECT_NAME}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)
