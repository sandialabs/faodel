


set(SOURCES
   nodeid_example.cpp
   main.cpp
)


add_executable( data_types ${HEADERS} ${SOURCES})
set_target_properties( data_types PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries( data_types ${EXAMPLES_LIBS} )
