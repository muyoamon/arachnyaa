cmake_minimum_required(VERSION 3.28)
project(arachnyaa C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(src/)
add_subdirectory(lib/)
