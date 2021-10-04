
if( NOT DEFINED EXAMPLE_LIBS )
    # A standalone project can pull in everything via Faodel::whookie and Faodel::common
    set( EXAMPLE_LIBS Faodel::whookie Faodel::common )
endif()



set(TARGET ex_whookie_bootstrap)
add_executable( ${TARGET} bootstrap_example.cpp )
target_link_libraries( ${TARGET} ${EXAMPLE_LIBS} )


set(TARGET ex_whookie_killit)
add_executable( ${TARGET} killit_example.cpp )
target_link_libraries( ${TARGET} ${EXAMPLE_LIBS} )


install(TARGETS ex_whookie_bootstrap ex_whookie_killit
        EXPORT faodelExampleTargets
        RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)