file(GLOB_RECURSE PEBBLE_LIB_SRC "src/lib/*.cpp")
add_library(pebble-core STATIC ${PEBBLE_LIB_SRC})
target_include_directories(pebble-core PUBLIC include)

add_executable(pebble src/main.cpp)
target_include_directories(pebble PRIVATE include)
target_link_libraries(pebble PRIVATE pebble-core)
