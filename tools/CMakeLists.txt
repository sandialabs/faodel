cmake_minimum_required(VERSION 3.2)

project(faodel_tools LANGUAGES CXX C)


# By default, cmake looks in your ~/.cmake directory for information
# about packages. While that should be fine for most users, we disable
# it in these examples because developers often have different versions
# of libraries in different places and cmake always guesses wrong.
set( CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY TRUE )


#find_package( Faodel CONFIG REQUIRED )

add_subdirectory( faodel-info )
add_subdirectory( kelpie-server )
add_subdirectory( whookie )

