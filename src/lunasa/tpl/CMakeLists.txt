

if( Faodel_ENABLE_TCMALLOC )
  add_subdirectory(gperftools)
endif( Faodel_ENABLE_TCMALLOC )

#list(APPEND TPL_INCLUDE_DIRS ${GPERFTOOLS_INCLUDE_DIRS} PARENT_SCOPE)
#set(TPL_INCLUDE_DIRS ${TPL_INCLUDE_DIRS} ${GPERFTOOLS_INCLUDE_DIRS} PARENT_SCOPE)

# Make sure the top-level CMake has PROJECT_LIBS in it's scope
# PROJECT_LIBS will be needed by targets in test/
set(  PROJECT_LIBS "${PROJECT_LIBS}" PARENT_SCOPE)


