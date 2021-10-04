set(FOO ex_kelpie_compute1_pick ex_kelpie_compute2_udf)
# When doing a standalone build, use the Faodel::kelpie imported target
if( NOT DEFINED EXAMPLE_LIBS)
    set(EXAMPLE_LIBS Faodel::kelpie)
endif()

# We also use boost and mpi
list(APPEND EXAMPLE_LIBS
        MPI::MPI_CXX
        )

set(TARGET ex_kelpie_compute1_pick)
add_executable( ${TARGET} ex_compute1_pick.cpp )
target_link_libraries( ${TARGET} ${EXAMPLE_LIBS} )

set(TARGET ex_kelpie_compute2_udf)
add_executable( ${TARGET} ex_compute2_udf.cpp )
target_link_libraries( ${TARGET} ${EXAMPLE_LIBS} )


install(TARGETS ${FOO}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
        )
