
project(eris_test)

remove_definitions(-Wall -Werror)

add_subdirectory(gtest-1.6.0)
enable_testing()

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(bundle-test bundle-test.cpp)
target_link_libraries(bundle-test gtest gtest_main)
add_test(bundle-test bundle-test)

