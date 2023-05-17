

#
# These directories need the functions defined above


set( HEADERS_PUBLIC
     Kelpie.hh
     Key.hh
     common/KelpieInternal.hh
     common/ObjectCapacities.hh
     common/Types.hh
     ioms/IomBase.hh
     ioms/IomRegistry.hh
     localkv/LocalKV.hh
     localkv/LocalKVTypes.hh
     localkv/LocalKVCell.hh
     localkv/LocalKVRow.hh
     pools/Pool.hh
)

set( HEADERS
     common/ComputeRegistry.hh
     common/OpArgsObjectAvailable.hh
     core/KelpieCoreBase.hh
     core/KelpieCoreNoNet.hh
     core/KelpieCoreStandard.hh
     core/KelpieCoreUnconfigured.hh
     core/Singleton.hh
     ops/direct/msg_direct.hh
     ops/direct/OpKelpieCompute.hh
     ops/direct/OpKelpieDrop.hh
     ops/direct/OpKelpieGetBounded.hh
     ops/direct/OpKelpieGetUnbounded.hh
     ops/direct/OpKelpieList.hh
     ops/direct/OpKelpieMeta.hh
     ops/direct/OpKelpiePublish.hh
     ioms/IomPosixIndividualObjects.hh
     pools/PoolBase.hh
     pools/LocalPool/LocalPool.hh
     pools/DHTPool/DHTPool.hh
     pools/NullPool/NullPool.hh
     pools/TFTPool/TFTPool.hh
     pools/TracePool/TracePool.hh
     pools/PoolRegistry.hh
     pools/ResultCollector.cpp
     pools/UnconfiguredPool/UnconfiguredPool.hh

)

set( SOURCES
     Kelpie.cpp
     Key.cpp
     common/ComputeRegistry.cpp
     common/KelpieInternal.cpp
     common/ObjectCapacities.cpp
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
     ops/direct/OpKelpieCompute.cpp
     ops/direct/OpKelpieDrop.cpp
     ops/direct/OpKelpieGetBounded.cpp
     ops/direct/OpKelpieGetUnbounded.cpp
     ops/direct/OpKelpieList.cpp
     ops/direct/OpKelpieMeta.cpp
     ops/direct/OpKelpiePublish.cpp
     pools/DHTPool/DHTPool.cpp
     pools/LocalPool/LocalPool.cpp
     pools/NullPool/NullPool.cpp
     pools/TFTPool/TFTPool.cpp
     pools/TracePool/TracePool.cpp
     pools/Pool.cpp
     pools/PoolBase.cpp
     pools/PoolRegistry.cpp
     pools/ResultCollector.cpp
     pools/UnconfiguredPool/UnconfiguredPool.cpp
)

if( Faodel_ENABLE_MPI_SUPPORT )
  LIST( APPEND HEADERS pools/RFTPool/RFTPool.hh)
  LIST( APPEND SOURCES pools/RFTPool/RFTPool.cpp)
  LIST( APPEND HEADERS services/PoolServerDriver.hh)
  LIST( APPEND SOURCES services/PoolServerDriver.cpp)
endif()


LIST( APPEND Kelpie_imports
  dirman
  opbox ${NETLIB_TARGETS}
  whookie common sbl
  Boost::program_options
  )

# IomLevelDB needs LevelDB
if( FAODEL_HAVE_LEVELDB AND Faodel_ENABLE_IOM_LEVELDB )
  LIST( APPEND Kelpie_imports leveldb::leveldb )
  LIST( APPEND Kelpie_extra_compile_defs "FAODEL_HAVE_LEVELDB" )

  LIST( APPEND HEADERS ioms/IomLevelDB.hh )
  LIST( APPEND SOURCES ioms/IomLevelDB.cpp )
endif()

# IomHDF5 needs HDF5
if( FAODEL_HAVE_HDF5 AND Faodel_ENABLE_IOM_HDF5 )
  LIST( APPEND Kelpie_imports Faodel::HDF5 )
  LIST( APPEND Kelpie_extra_compile_defs "FAODEL_HAVE_HDF5" )
  LIST( APPEND HEADERS ioms/IomHDF5.hh )
  LIST( APPEND SOURCES ioms/IomHDF5.cpp )
endif()

if( FAODEL_HAVE_CASSANDRA AND Faodel_ENABLE_IOM_CASSANDRA )
  LIST( APPEND Kelpie_imports Faodel::Cassandra )
  LIST( APPEND Kelpie_extra_compile_defs "FAODEL_HAVE_CASSANDRA" )
  LIST( APPEND HEADERS ioms/IomCassandra.hh )
  LIST( APPEND SOURCES ioms/IomCassandra.cpp )
endif()

# All things go in kelpie lib now
add_library( kelpie ${HEADERS} ${SOURCES})
if( Kelpie_extra_compile_defs )
  target_compile_definitions( kelpie PUBLIC ${Kelpie_extra_compile_defs} )
endif()
target_link_libraries( kelpie PUBLIC ${Kelpie_imports} )

install(TARGETS kelpie
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( FILES Kelpie.hh Key.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie )
install( FILES common/Types.hh common/KelpieInternal.hh common/ObjectCapacities.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/common )
install( FILES ioms/IomBase.hh ioms/IomRegistry.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/ioms )
install( FILES localkv/LocalKV.hh localkv/LocalKVCell.hh localkv/LocalKVRow.hh localkv/LocalKVTypes.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/localkv )
install( FILES pools/Pool.hh pools/ResultCollector.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/pools )
install( FILES services/PoolServerDriver.hh
  DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/kelpie/services )

  
#-----------------------------------------
