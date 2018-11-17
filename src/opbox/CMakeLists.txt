
if( NOT Faodel_NETWORK_LIBRARY )
    message( STATUS "Network Library not selected.  Defaulting to nnti." )
    set(Faodel_NETWORK_LIBRARY "nnti" CACHE STRING "network library (nnti | libfabric)")
endif()
message( STATUS "Network Library    ${Faodel_NETWORK_LIBRARY}"    )


set(HEADERS
    OpBox.hh
    common/Message.hh
    common/MessageHelpers.hh
    common/OpArgs.hh
    common/OpRegistry.hh
    common/Types.hh
    core/Singleton.hh
    core/OpBoxCoreBase.hh
    core/OpBoxCoreStandard.hh
    core/OpBoxCoreThreaded.hh
    core/OpBoxCoreUnconfigured.hh
    net/net.hh
    net/nbr.hh
    net/peer.hh
    ops/Op.hh
    ops/OpCount.hh
    ops/OpHelpers.hh
    ops/OpPing.hh
)

# todo: slim this down to the real list
set(HEADERS_PUBLIC
    ${HEADERS}
)

set(SOURCES
    OpBox.cpp
    common/Message.cpp
    common/MessageHelpers.cpp
    common/OpArgs.cpp
    common/OpRegistry.cpp
    common/Types.cpp
    core/Singleton.cpp
    core/OpBoxCoreBase.cpp
    core/OpBoxCoreStandard.cpp
    core/OpBoxCoreThreaded.cpp
    core/OpBoxCoreUnconfigured.cpp
    ops/Op.cpp
    ops/OpCount.cpp
    ops/OpPing.cpp
)

add_subdirectory( net )

LIST( APPEND Opbox_imports lunasa ${FaodelNetlib_TARGETS} services webhook common sbl Boost::serialization )

add_library( opbox ${HEADERS} ${SOURCES} )
target_link_libraries( opbox PUBLIC ${Opbox_imports} )

install(TARGETS opbox
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  )

#-----------------------------------------
