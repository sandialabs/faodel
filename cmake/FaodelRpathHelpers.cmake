
function( add_library_paths_to_var lib_list add_to_var )
  set(target_var "${${add_to_var}}")
  foreach(lib IN LISTS lib_list)
    get_filename_component(lib_path ${lib} DIRECTORY )
    set(target_var "${target_var}:${lib_path}")
  endforeach()
  set(${add_to_var} "${target_var}" PARENT_SCOPE)
endfunction()

function( set_faodel_build_rpath )
    set(FAODEL_BUILD_RPATH "" CACHE STRING "list of paths to use for RPATH in the build tree")
    ## add the Faodel library build paths
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tpl/gperftools")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/sbl")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/faodel-common")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/whookie")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/faodel-services")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/nnti")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/lunasa")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/opbox")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/dirman")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/src/kelpie")
    ## add the Faodel test library build paths
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tests/nnti/benchmarks")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tests/nnti/c-api")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tests/nnti/cpp-api")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tests/opbox")
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${CMAKE_CURRENT_BINARY_DIR}/tests/kelpie")
    ## add the Faodel TPL library build paths
    if (Boost_FOUND)
      set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}:${Boost_LIBRARY_DIRS}")
    endif()
    if (GTest_FOUND)
      add_library_paths_to_var(${GTEST_BOTH_LIBRARIES} FAODEL_BUILD_RPATH)
    endif()

    ## put into the parent scope
    set(FAODEL_BUILD_RPATH "${FAODEL_BUILD_RPATH}" PARENT_SCOPE)

#    message( STATUS "FAODEL_BUILD_RPATH=${FAODEL_BUILD_RPATH}" )
endfunction()

function( set_faodel_install_rpath )
    set(FAODEL_INSTALL_RPATH "" CACHE STRING "list of paths to use for RPATH in the install tree")
    set(FAODEL_INSTALL_RPATH "${FAODEL_INSTALL_RPATH}:${CMAKE_INSTALL_PREFIX}/lib")
    if (Boost_FOUND)
      set(FAODEL_INSTALL_RPATH "${FAODEL_INSTALL_RPATH}:${Boost_LIBRARY_DIRS}")
    endif()
    if (GTest_FOUND)
      add_library_paths_to_var("${GTEST_BOTH_LIBRARIES}" FAODEL_INSTALL_RPATH)
    endif()

    ## put into the parent scope
    set(FAODEL_INSTALL_RPATH "${FAODEL_INSTALL_RPATH}" PARENT_SCOPE)

#    message( STATUS "FAODEL_INSTALL_RPATH=${FAODEL_INSTALL_RPATH}" )
endfunction()

function( set_cmake_rpath_flags )
    set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG     "-Wl,-rpath," PARENT_SCOPE)
    set(CMAKE_EXECUTABLE_RUNTIME_CXX_FLAG_SEP ":"           PARENT_SCOPE)
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG       "-Wl,-rpath," PARENT_SCOPE)
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG_SEP   ":"           PARENT_SCOPE)

    set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG     "-Wl,-rpath," PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG_SEP ":"           PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG       "-Wl,-rpath," PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP   ":"           PARENT_SCOPE)

    set(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-Wl,-soname," PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG   "-Wl,-soname," PARENT_SCOPE)

    set(CMAKE_EXECUTABLE_RPATH_LINK_CXX_FLAG     "-Wl,-rpath-link," PARENT_SCOPE)
    set(CMAKE_EXECUTABLE_RPATH_LINK_C_FLAG       "-Wl,-rpath-link," PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX_FLAG "-Wl,-rpath-link," PARENT_SCOPE)
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_C_FLAG   "-Wl,-rpath-link," PARENT_SCOPE)

    set(CMAKE_SKIP_INSTALL_RPATH NO PARENT_SCOPE)
    set(CMAKE_SKIP_RPATH         NO PARENT_SCOPE)
endfunction()

function( set_cmake_linker_rpath_flags )
    set (CMAKE_EXE_LINKER_FLAGS    "-Wl,-rpath,${FAODEL_BUILD_RPATH}" PARENT_SCOPE)
    set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-rpath,${FAODEL_BUILD_RPATH}" PARENT_SCOPE)
endfunction()

function( set_cmake_install_rpath )
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:${FAODEL_INSTALL_RPATH}")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}" PARENT_SCOPE)
#    message( STATUS "CMAKE_INSTALL_RPATH=${CMAKE_INSTALL_RPATH}" )
endfunction()
