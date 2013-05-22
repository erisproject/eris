
project(eris_test)

remove_definitions(-Wall -Werror)

add_subdirectory(gtest-1.6.0)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(bundle-test bundle-test.cpp)
target_link_libraries(bundle-test eris gtest gtest_main)
add_test(bundle-test-run bundle-test --gtest_color=yes)
add_dependencies(check bundle-test)

