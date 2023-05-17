include_directories(../../support ${CMAKE_CURRENT_BINARY_DIR} )

set(HEADERS
        ArrowDataObject.cpp
)

set(HEADERS_PUBLIC
        ArrowDataObject.hh
)

set(SOURCES
        ArrowDataObject.cpp
)

LIST( APPEND Farrow_imports lunasa whookie common Parquet::parquet_shared ArrowDataset::arrow_dataset_shared )

add_library(arrow ${HEADERS} ${SOURCES})
set_target_properties(arrow PROPERTIES
  OUTPUT_NAME "faodel-arrow"
)
target_link_libraries( arrow PUBLIC lunasa ${Farrow_imports})

install(TARGETS arrow
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
