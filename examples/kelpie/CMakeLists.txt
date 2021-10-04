

add_subdirectory( nonet/start-finish )
add_subdirectory( nonet/local-pool-basics )
add_subdirectory( nonet/using-whookie )
add_subdirectory( nonet/storing-in-iom )
add_subdirectory( nonet/dim-sum )



if( NOT TARGET MPI::MPI_CXX )
    message( STATUS "Some Kelpie Examples will not be built because MPI Support was not enabled")

else()
    add_subdirectory( compute )
    add_subdirectory( prod-con )
endif()