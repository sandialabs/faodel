
#TODO: Future versions will include Kokkos support
#option(Kokkos_CONFIG_INCLUDED "Build Lunasa with Kokkos support"     OFF)

set(HEADERS_PUBLIC
    DataObject.hh
    Lunasa.hh
    common/Types.hh
    common/DataObjectPacker.hh
    common/Helpers.hh
    common/GenericRandomDataBundle.hh
    common/GenericSequentialDataBundle.hh
)


set(HEADERS
    Lunasa.hh
    DataObject.hh
    allocators/Allocators.hh
    allocators/AllocatorBase.hh
    allocators/AllocatorMalloc.hh
    allocators/AllocatorUnconfigured.hh
    common/Allocation.hh
    common/DataObjectPacker.hh
    common/GenericRandomDataBundle.hh
    common/GenericSequentialDataBundle.hh
    common/DataObjectTypeRegistry.hh
    common/Types.hh
    core/LunasaCoreBase.hh
    core/LunasaCoreSplit.hh
    core/LunasaCoreUnconfigured.hh
    core/Singleton.hh
)

set(SOURCES
    Lunasa.cpp
    DataObject.cpp
    allocators/Allocators.cpp
    allocators/AllocatorBase.cpp
    allocators/AllocatorMalloc.cpp
    allocators/AllocatorUnconfigured.cpp
    common/DataObjectTypeRegistry.cpp
    common/DataObjectPacker.cpp
    common/Helpers.cpp
    core/LunasaCoreBase.cpp
    core/LunasaCoreSplit.cpp
    core/LunasaCoreUnconfigured.cpp
    core/Singleton.cpp)

if( Faodel_ENABLE_TCMALLOC )
  list(APPEND SOURCES allocators/AllocatorTcmalloc.cpp)
  list(APPEND HEADERS allocators/AllocatorTcmalloc.hh)
  #add_definitions(-DFaodel_ENABLE_TCMALLOC)
endif()

# This list has to explicitly contain whookie, common, and sbl
# This is because NETLIB_TARGETS might end up being libfabric and friends,
# and in that case we don't get whookie et al. through the nnti target.
LIST( APPEND Lunasa_IMPORTS whookie common sbl ${FaodelNetlib_TARGETS}  )
if( Faodel_ENABLE_TCMALLOC )
  LIST( APPEND Lunasa_IMPORTS tcmalloc spinlock )
endif()

add_library(lunasa ${HEADERS} ${SOURCES})
target_link_libraries( lunasa ${Lunasa_IMPORTS} )


install(TARGETS lunasa
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( FILES DataObject.hh Lunasa.hh DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/lunasa )
install( FILES common/Types.hh common/DataObjectPacker.hh common/Helpers.hh common/GenericRandomDataBundle.hh common/GenericSequentialDataBundle.hh DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/lunasa/common )


#-----------------------------------------
