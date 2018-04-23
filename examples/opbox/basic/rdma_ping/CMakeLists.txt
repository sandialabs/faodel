set(PROJECT_NAME rdma_ping)

set(HEADERS
   OpRdmaPing.hh
)

set(SOURCES
   OpRdmaPing.cpp
   rdma_ping.cpp
)


add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )

target_link_libraries(${PROJECT_NAME} ${EXAMPLES_LIBS})
