add_executable(pebble-test test_runner.cpp)
target_link_libraries(pebble-test PRIVATE pebble-core)
target_include_directories(pebble-test PRIVATE compiler/include)

file(GLOB_RECURSE TEST_FILES "cases/*.pebble")
foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_test(NAME ${TEST_NAME} COMMAND pebble-test ${TEST_FILE})
endforeach()
