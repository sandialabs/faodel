add_subdirectory( bootstrap )
add_subdirectory( ldo_packing )


if( NOT TARGET MPI::MPI_CXX )
  message( STATUS "Some Lunasa Examples will not be built because MPI support was not enabled" )
else()

  # These examples require nnti for moving data between nodes
  if(Faodel_NETWORK_LIBRARY STREQUAL "nnti")
    add_subdirectory( simple_ldo_send )
    add_subdirectory( simple_user_ldo_put )
  endif()
endif()
