set(PROJECT_NAME ex_service_mpi-sync-start)

# When doing a standalone build, use the Faodel::services imported target
if(NOT DEFINED EXAMPLE_LIBS)
    set( EXAMPLE_LIBS Faodel::services )
endif()


set( TARGET ex_service_mpi-sync-start-basic )
add_executable(${TARGET} mpi_sync_start_basic.cpp)
set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(${TARGET} ${EXAMPLE_LIBS})

set( TARGET ex_service_mpi-sync-start-advanced )
add_executable(${TARGET} mpi_sync_start_advanced.cpp)
set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(${TARGET} ${EXAMPLE_LIBS})

install(TARGETS ex_service_mpi-sync-start-basic ex_service_mpi-sync-start-advanced
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
        )
