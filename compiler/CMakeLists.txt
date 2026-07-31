cmake_minimum_required(VERSION 3.20)
project(pebble)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE PEBBLE_LIB_SRC "src/lib/*.cpp")
add_library(pebble-core STATIC ${PEBBLE_LIB_SRC})
target_include_directories(pebble-core PRIVATE include)

add_executable(pebble src/main.cpp)
target_include_directories(pebble PRIVATE include)
target_link_libraries(pebble PRIVATE pebble-core)
