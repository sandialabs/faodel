
if( NOT Faodel_NETWORK_LIBRARY )
  message( STATUS "Network Library not selected.  Defaulting to nnti." )
  set(Faodel_NETWORK_LIBRARY "nnti" CACHE STRING "network library (nnti | libfabric)")
endif()
message( STATUS "Network Library    ${Faodel_NETWORK_LIBRARY}"    )


set(HEADERS
  common/DirectoryCache.hh
  common/DirectoryOwnerCache.hh
  core/DirManCoreBase.hh
  core/DirManCoreCentralized.hh
  core/DirManCoreStatic.hh
  core/DirManCoreUnconfigured.hh
  core/Singleton.hh
  DirMan.hh
  ops/msg_dirman.hh
  ops/OpDirManCentralized.hh
  )

# todo: slim this down to the real list
set(HEADERS_PUBLIC
  ${HEADERS}
  )

set(SOURCES
  DirMan.cpp
  common/DirectoryCache.cpp
  common/DirectoryOwnerCache.cpp
  core/DirManCoreBase.cpp
  core/DirManCoreCentralized.cpp
  core/DirManCoreStatic.cpp
  core/DirManCoreUnconfigured.cpp
  core/Singleton.cpp
  ops/msg_dirman.cpp
  ops/OpDirManCentralized.cpp
  ops/OpDirManCentralized_Origin.cpp
  ops/OpDirManCentralized_Target.cpp
  )


LIST( APPEND Dirman_imports opbox lunasa ${FaodelNetlib_TARGETS} services whookie common sbl )

add_library( dirman ${HEADERS} ${SOURCES} )
target_link_libraries( dirman PUBLIC ${Dirman_imports} )

install(TARGETS       dirman
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
