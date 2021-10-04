

add_subdirectory( backburner )

if( (NOT TARGET MPI::MPI_CXX) OR (NOT Faodel_ENABLE_MPI_SUPPORT)  )
   message( STATUS "Some Service Examples will not be built because MPI Support was not enabled")

else()
   add_subdirectory( mpi-sync-start )

endif()


