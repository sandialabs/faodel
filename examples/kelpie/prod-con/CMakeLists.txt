set( Prod_Con_REQUIRED_HDF5_VERSION "1.10" )
set( HDF5_PREFER_PARALLEL TRUE )

set(TARGET ex_kelpie_prod-con)

# When doing a standalone build, use the Faodel::kelpie imported target
if( NOT DEFINED EXAMPLE_LIBS)
    set(EXAMPLE_LIBS Faodel::kelpie)
endif()

# We also use boost and mpi
list(APPEND EXAMPLE_LIBS
        Boost::program_options
        MPI::MPI_CXX
)

add_executable( ${TARGET} prod-con.cpp Globals.cpp )
target_link_libraries( ${TARGET} ${EXAMPLE_LIBS} )

find_package( HDF5 COMPONENTS CXX HL )
if( HDF5_FOUND )
  if( HDF5_VERSION VERSION_GREATER_EQUAL ${Prod_Con_REQUIRED_HDF5_VERSION} )
    target_link_libraries( ${TARGET} ${HDF5_LIBRARIES} )
    target_include_directories( ${TARGET} PUBLIC ${HDF5_INCLUDE_DIRS} )
    target_compile_definitions( ${TARGET} PUBLIC ${HDF5_DEFINITIONS} -DHAVE_HDF5 )
  endif()
endif()


install(TARGETS ${TARGET}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
        )


