
if( NOT NETWORK_LIBRARY )
    message( STATUS "Network Library not selected.  Defaulting to nnti." )
    set(NETWORK_LIBRARY "nnti" CACHE STRING "network library (nnti | libfabric)")
endif()
message( STATUS "Network Library    ${NETWORK_LIBRARY}"    )


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
    services/dirman/common/DirectoryCache.hh
    services/dirman/common/DirectoryOwnerCache.hh
    services/dirman/core/DirManCoreBase.hh
    services/dirman/core/DirManCoreCentralized.hh
    services/dirman/core/DirManCoreDistributed.hh
    services/dirman/core/DirManCoreUnconfigured.hh
    services/dirman/core/Singleton.hh
    services/dirman/DirectoryInfo.hh
    services/dirman/DirectoryManager.hh
    services/dirman/ops/msg_dirman.hh
    services/dirman/ops/OpDirManCentralized.hh
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
    services/dirman/DirectoryInfo.cpp
    services/dirman/DirectoryManager.cpp
    services/dirman/common/DirectoryCache.cpp
    services/dirman/common/DirectoryOwnerCache.cpp
    services/dirman/core/DirManCoreBase.cpp
    services/dirman/core/DirManCoreCentralized.cpp
    services/dirman/core/DirManCoreDistributed.cpp
    services/dirman/core/DirManCoreUnconfigured.cpp
    services/dirman/core/Singleton.cpp
    services/dirman/ops/msg_dirman.cpp
    services/dirman/ops/OpDirManCentralized.cpp
    services/dirman/ops/OpDirManCentralized_Origin.cpp
    services/dirman/ops/OpDirManCentralized_Target.cpp
)

add_subdirectory( net )

LIST( APPEND Opbox_imports lunasa ${FaodelNetlib_TARGETS} webhook common sbl Boost::serialization )

add_library( opbox STATIC ${HEADERS} ${SOURCES} )
target_link_libraries( opbox PUBLIC ${Opbox_imports} )

install(TARGETS opbox
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel
  FILES_MATCHING PATTERN "*.hh"
  )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/opboxConfig.h.in
    "${CMAKE_CURRENT_SOURCE_DIR}/opboxConfig.h"
    @ONLY
    )

# ... puts opboxConfig.h in include/${PROJECT_NAME}
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/opboxConfig.h"
    DESTINATION include/faodel/opbox
    )
#-----------------------------------------
