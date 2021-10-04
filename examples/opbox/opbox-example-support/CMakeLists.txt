
set(TARGET opbox_example_support)

add_library(${TARGET}
        Globals.cpp
        SimpleDataStore.cpp
        )
set_target_properties( ${TARGET} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries( ${TARGET}
        MPI::MPI_CXX
        ${EXAMPLE_LIBS} )