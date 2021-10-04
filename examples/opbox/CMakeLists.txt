
if( NOT TARGET MPI::MPI_CXX )
    message( STATUS "OpBox Examples will not be built because MPI Support was not enabled")

else()

    if(NOT DEFINED EXAMPLE_LIBS)
        # A standalone project can pull in everything via Faodel::opbox
        set(EXAMPLE_LIBS Faodel::opbox)
    endif()

    # We use a few helpers here to simplify the examples code
    add_subdirectory(opbox-example-support)


    include_directories(
       opbox-example-support
    )

    list(APPEND EXAMPLE_LIBS
            opbox_example_support
            Boost::program_options )

    add_subdirectory( basic/my-simple-ping )
    add_subdirectory( basic/rdma-ping )
    add_subdirectory( basic/message-packing )

    add_subdirectory( collectives/rapidfire )
    add_subdirectory( collectives/ringer )
    add_subdirectory( collectives/scatter-gather )

    add_subdirectory( advanced/lingering-data )
    add_subdirectory( advanced/job2job )

    add_subdirectory( benchmarks )

endif()