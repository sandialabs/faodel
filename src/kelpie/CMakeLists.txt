

#
# These directories need the functions defined above


set( HEADERS_PUBLIC
     Kelpie.hh
     Key.hh
     common/KelpieInternal.hh
     common/Types.hh
     ioms/IomBase.hh
     ioms/IomRegistry.hh
     localkv/LocalKV.hh
     localkv/LocalKVCell.hh
     localkv/LocalKVRow.hh
     pools/Pool.hh
)

set( HEADERS
     common/OpArgsObjectAvailable.hh
     core/KelpieCoreBase.hh
     core/KelpieCoreNoNet.hh
     core/KelpieCoreStandard.hh
     core/KelpieCoreUnconfigured.hh
     core/Singleton.hh
     ops/direct/msg_direct.hh
     ops/direct/OpKelpieGetBounded.hh
     ops/direct/OpKelpieGetUnbounded.hh
     ops/direct/OpKelpieMeta.hh
     ops/direct/OpKelpiePublish.hh
     ioms/IomPosixIndividualObjects.hh
     pools/PoolBase.hh
     pools/LocalPool/LocalPool.hh
     pools/DHTPool/DHTPool.hh
     pools/PoolRegistry.hh
     pools/UnconfiguredPool/UnconfiguredPool.hh
)

set( SOURCES
     Kelpie.cpp
     Key.cpp
     common/KelpieInternal.cpp
     common/OpArgsObjectAvailable.cpp
     common/Types.cpp
     core/KelpieCoreBase.cpp
     core/KelpieCoreNoNet.cpp
     core/KelpieCoreStandard.cpp
     core/KelpieCoreUnconfigured.cpp
     core/Singleton.cpp
     ioms/IomBase.cpp
     ioms/IomRegistry.cpp
     ioms/IomPosixIndividualObjects.cpp
     localkv/LocalKV.cpp
     localkv/LocalKVCell.cpp
     localkv/LocalKVRow.cpp
     ops/direct/msg_direct.cpp
     ops/direct/OpKelpieGetBounded.cpp
     ops/direct/OpKelpieGetUnbounded.cpp
     ops/direct/OpKelpieMeta.cpp
     ops/direct/OpKelpiePublish.cpp
     pools/LocalPool/LocalPool.cpp
     pools/DHTPool/DHTPool.cpp
     pools/Pool.cpp
     pools/PoolBase.cpp
     pools/PoolRegistry.cpp
     pools/UnconfiguredPool/UnconfiguredPool.cpp
)

LIST( APPEND Kelpie_imports
  opbox ${NETLIB_TARGETS}
  webhook common sbl
  Boost::program_options
  )


# All things go in kelpie lib now
add_library( kelpie STATIC ${HEADERS} ${SOURCES})
target_link_libraries( kelpie PUBLIC ${Kelpie_imports} )

install(TARGETS kelpie
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( FILES Kelpie.hh Key.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie )
install( FILES common/Types.hh common/KelpieInternal.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/common )
install( FILES ioms/IomBase.hh ioms/IomRegistry.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/ioms )
install( FILES localkv/LocalKV.hh localkv/LocalKVCell.hh localkv/LocalKVRow.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/localkv )
install( FILES pools/Pool.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/pools )

  
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kelpieConfig.h.in
    "${CMAKE_CURRENT_SOURCE_DIR}/kelpieConfig.h"
    @ONLY
    )

# ... puts kelpieConfig.h in include/${PROJECT_NAME}
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/kelpieConfig.h"
    DESTINATION include/faodel/kelpie
    )
#-----------------------------------------
