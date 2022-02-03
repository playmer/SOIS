cmake_minimum_required(VERSION 3.16.1 FATAL_ERROR) 
project(SOIS CXX)

set(RepoDirectory ${CMAKE_CURRENT_LIST_DIR})
set(DependenciesDirectory ${RepoDirectory}/Dependencies)
set(AssetDirectory ${RepoDirectory}/Assets)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

add_subdirectory(Dependencies)
add_subdirectory(Source)