
project(eris_test)

include(AddGTest)

add_definitions(-DERIS_TESTS)

set(eris_tests
    bundle-test
    incr-buyer-test
    mupd-test
    sim-setup-test
    lock-test
    pos-agent-test
    )

foreach(t ${eris_tests})
    add_executable("${t}" "${t}.cpp")
    target_link_libraries("${t}" eris gtest gtest_main)
    add_test("${t}-run" "${t}" --gtest_color=yes)
    add_dependencies(check "${t}")
endforeach()
