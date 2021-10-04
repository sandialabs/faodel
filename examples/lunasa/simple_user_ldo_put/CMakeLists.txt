set(PROJECT_NAME ex_lunasa_ldo-put)

set(SOURCES simple_user_ldo_put_example.cpp)

# When doing a standalone build, use the Faodel::lunasa imported target
if(NOT DEFINED EXAMPLE_LIBS)
    set( EXAMPLE_LIBS Faodel::lunasa )
endif()

# We also use mpi
list(APPEND EXAMPLE_LIBS
        MPI::MPI_CXX
        )


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(${PROJECT_NAME} ${EXAMPLE_LIBS})

install(TARGETS ${PROJECT_NAME}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)