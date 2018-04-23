
option(Kokkos_CONFIG_INCLUDED "Build Lunasa with Kokkos support"     OFF)

set(HEADERS_PUBLIC
    DataObject.hh
    Lunasa.hh
    common/Types.hh
)


set(HEADERS
    Lunasa.hh
    DataObject.hh
    allocators/Allocators.hh
    allocators/AllocatorBase.hh
    allocators/AllocatorMalloc.hh
    allocators/AllocatorUnconfigured.hh
    common/Allocation.hh
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
    core/LunasaCoreBase.cpp
    core/LunasaCoreSplit.cpp
    core/LunasaCoreUnconfigured.cpp
    core/Singleton.cpp
) 

if( ENABLE_TCMALLOC )
  list(APPEND SOURCES allocators/AllocatorTcmalloc.cpp)
  list(APPEND HEADERS allocators/AllocatorTcmalloc.hh)
  add_definitions(-DENABLE_TCMALLOC)
endif()

# This list has to explicitly contain webhook, common, and sbl
# This is because NETLIB_TARGETS might end up being libfabric and friends,
# and in that case we don't get webhook et al. through the nnti target.
LIST( APPEND Lunasa_IMPORTS webhook common sbl ${FaodelNetlib_TARGETS}  )
if( ENABLE_TCMALLOC )
  LIST( APPEND Lunasa_IMPORTS tcmalloc spinlock )
endif()

add_library(lunasa STATIC ${HEADERS} ${SOURCES})
target_link_libraries( lunasa ${Lunasa_IMPORTS} )


install(TARGETS lunasa
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )

install( FILES DataObject.hh Lunasa.hh DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/lunasa )
install( FILES common/Types.hh DESTINATION ${INCLUDE_INSTALL_DIR}/faodel/lunasa/common )


#-----------------------------------------
