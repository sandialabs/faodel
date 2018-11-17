set(PROJECT_NAME dirman-example)


add_executable(       dirman1-preload-configuration dirman1-preload-configuration.cpp)
set_target_properties(dirman1-preload-configuration PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(dirman1-preload-configuration Faodel::dirman)

add_executable(       dirman2-runtime-define dirman2-runtime-define.cpp)
set_target_properties(dirman2-runtime-define PROPERTIES LINKER_LANGUAGE CXX )
target_link_libraries(dirman2-runtime-define Faodel::dirman)

install(TARGETS dirman1-preload-configuration dirman2-runtime-define
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)
