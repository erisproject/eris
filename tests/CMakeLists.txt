
project(eris_test)

remove_definitions(-Wall -Werror)

add_subdirectory(gtest-1.6.0)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(eris_tests
    bundle-test
    incr-buyer-test
    mupd-test
    sim-setup-test
    lock-test
    )

foreach(t ${eris_tests})
    add_executable("${t}" "${t}.cpp")
    target_link_libraries("${t}" eris gtest gtest_main)
    add_test("${t}-run" "${t}" --gtest_color=yes)
    add_dependencies(check "${t}")
endforeach()

