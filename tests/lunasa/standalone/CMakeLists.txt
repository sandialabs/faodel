cmake_minimum_required(VERSION 2.8)


find_package(lunasa REQUIRED HINTS $ENV{LUNASA})
message( STATUS "\nFound Lunasa! Here are the details: ")
message( STATUS "   Lunasa_INCLUDE_DIRS       = ${Lunasa_INCLUDE_DIRS}")
message( STATUS "   Lunasa_LIBRARIES          = ${Lunasa_LIBRARIES}")
message( STATUS "   Lunasa_TPL_LIBRARIES      = ${Lunasa_TPL_LIBRARIES}")


project(lunasa_standalone CXX C)

# C++11 Check: lunasa doesn't really need it, but other things do
# Taken from:   http://stackoverflow.com/questions/10984442
#
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (NOT (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7))
        message(FATAL_ERROR "${PROJECT_NAME} requires g++ 4.7 or greater.")
    endif ()
    
         
    set(CMAKE_CXX_FLAGS    "-std=c++11 ${CMAKE_CXX_FLAGS}") 
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
    message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()



include_directories( ${Lunasa_INCLUDE_DIRS} )
add_executable( tb_staticBootstrap tb_staticBootstrap.cpp)
target_link_libraries( tb_staticBootstrap
       ${Lunasa_LIBRARIES}
       ${Lunasa_TPL_LIBRARIES}
)
