cmake_minimum_required(VERSION 3.16.1 FATAL_ERROR) 
project(SimpleSDL2ImguiAppFramework CXX)

set(RepoDirectory ${CMAKE_CURRENT_LIST_DIR})
set(DependenciesDirectory ${RepoDirectory}/Dependencies)
set(AssetDirectory ${RepoDirectory}/Assets)

add_subdirectory(Dependencies)
add_subdirectory(Source)