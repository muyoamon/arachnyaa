cmake_minimum_required(VERSION 3.28)
project(arachnyaa C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(CEF REQUIRED)

add_subdirectory(src/)
add_subdirectory(lib/)

target_link_libraries(arachnyaac CEF::CEF)

target_include_directories(arachnyaac PUBLIC ${CEF_INCLUDE_DIR})



