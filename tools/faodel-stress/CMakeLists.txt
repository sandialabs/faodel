set(
    SOURCES
    Worker.cpp
    Job.cpp
    JobKeys.cpp
    JobLocalPool.cpp
    JobMemoryAlloc.cpp
    JobSerdes.cpp
    JobWebClient.cpp
    serdes/SerdesParticleBundleObject.cpp
    serdes/SerdesStringObject.cpp
    faodel-stress.cpp
)

add_executable(
    faodel-stress
    ${SOURCES}
)

set_target_properties(
    faodel-stress
    PROPERTIES LINKER_LANGUAGE CXX    
)

target_link_libraries(
    faodel-stress
    kelpie
    Boost::serialization
)

install(
    TARGETS faodel-stress
    EXPORT faodelTargets
    RUNTIME DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
)


