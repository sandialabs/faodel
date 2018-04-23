set(PROJECT_NAME local-pool-basics)

set(HEADERS
)

set(SOURCES
   local-pool-basics.cpp
   ex1_get_pool_handle.cpp
   ex2_publish_need.cpp
)


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLES_LIBS} )
