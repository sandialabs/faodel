set(PROJECT_NAME ex_dirman)

if( NOT TARGET MPI::MPI_CXX )
    message( STATUS "Some Dirman Examples will not be built because MPI Support was not enabled")

else()


    # When doing a standalone build, use the Faodel::dirman imported target
    if(NOT DEFINED EXAMPLE_LIBS)
        set(EXAMPLE_LIBS Faodel::dirman)
    endif()

    # We also use mpi
    list(APPEND EXAMPLE_LIBS
            MPI::MPI_CXX
            )

    set(TARGET ex_dirman1-preload-configuration)
    add_executable(       ${TARGET} dirman1-preload-configuration.cpp)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX )
    target_link_libraries(${TARGET} ${EXAMPLE_LIBS} )

    set(TARGET ex_dirman2-runtime-define)
    add_executable(       ${TARGET} dirman2-runtime-define.cpp)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX )
    target_link_libraries(${TARGET} ${EXAMPLE_LIBS})

    install(TARGETS ex_dirman1-preload-configuration ex_dirman2-runtime-define
            EXPORT faodelExampleTargets
            RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
    )

endif()