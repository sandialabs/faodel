

add_executable( webhook_bootstrap_example bootstrap_example.cpp )
target_link_libraries( webhook_bootstrap_example ${EXAMPLE_LIBS} )

add_executable( webhook_killit_example killit_example.cpp )
target_link_libraries( webhook_killit_example ${EXAMPLE_LIBS} )

install(TARGETS webhook_bootstrap_example webhook_killit_example
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)