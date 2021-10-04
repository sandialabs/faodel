set(PROJECT_NAME ex_common_singleton)

# When doing a standalone build, use the Faodel::common imported target
if(NOT DEFINED EXAMPLE_LIBS)
    set( EXAMPLE_LIBS Faodel::common )
endif()


#Note: globals can get stripped out of libraries and may require -Wl,--whole-archive wrapping
#      This problem usually shows up when you don't build an executable w/ libs

foreach(LIB_LETTER A B C)
    add_library(ex_common_libService${LIB_LETTER} Service${LIB_LETTER}.cpp)
    target_link_libraries( ex_common_libService${LIB_LETTER} ${EXAMPLE_LIBS} )
endforeach()

set(SOURCES singleton_example.cpp)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(${PROJECT_NAME} ${EXAMPLE_LIBS} ex_common_libServiceA ex_common_libServiceB ex_common_libServiceC)


install(TARGETS ${PROJECT_NAME}
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)