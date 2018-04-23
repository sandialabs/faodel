set(PROJECT_NAME singleton_example)

#Note: globals can get stripped out of libraries and may require -Wl,--whole-archive wrapping
#      This problem usually shows up when you don't build an executable w/ libs

set(SERVICEA_HEADERS ServiceA.hh)
set(SERVICEA_SOURCES ServiceA.cpp)
add_library(libServiceA ${SERVICEA_SOURCES} )
target_link_libraries( libServiceA Faodel::common )


set(SERVICEB_HEADERS ServiceB.hh)
set(SERVICEB_SOURCES ServiceB.cpp)
add_library(libServiceB ${SERVICEB_SOURCES} )
target_link_libraries( libServiceB Faodel::common )

set(SERVICEC_HEADERS ServiceC.hh)
set(SERVICEC_SOURCES ServiceC.cpp)
add_library(libServiceC ${SERVICEC_SOURCES} )
target_link_libraries( libServiceC Faodel::common )


set(SOURCES singleton_example.cpp)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(${PROJECT_NAME} ${EXAMPLES_LIBS} libServiceA libServiceB libServiceC)


