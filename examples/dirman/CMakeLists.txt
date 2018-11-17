set(PROJECT_NAME directory_manager)

set(HEADERS
)

set(SOURCES
   directory_manager_example.cpp
   example1.cpp
   example2.cpp
   example3.cpp
   example4.cpp
   example5.cpp
)


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLES_LIBS})
