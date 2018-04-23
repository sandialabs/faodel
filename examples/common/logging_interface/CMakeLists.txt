set(PROJECT_NAME logging_interface)


set(HEADERS
   ClassA.hh
   ClassB.hh
)

set(SOURCES
   ClassA.cpp
   ClassB.cpp
   example1_selectiveA.cpp
   example2_disableB.cpp
   logging_interface.cpp
)


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLES_LIBS})
