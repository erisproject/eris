
project(eris_test)

include(AddGTest)

if (GTEST_SRC)
    add_subdirectory(${GTEST_SRC} "${CMAKE_CURRENT_BINARY_DIR}/gtest")

    add_definitions(-DERIS_TESTS)

    link_directories(${GTEST_LIB})

    set(eris_tests
        bundle-test
        incr-buyer-test
        mupd-test
        sim-setup-test
        lock-test
        pos-agent-test
        single-peak-search
        stage-priority
        )

    foreach(t ${eris_tests})
        add_executable("${t}" "${t}.cpp")
        target_link_libraries("${t}" eris gtest gtest_main)
        add_test("${t}-run" "${t}" --gtest_color=yes)
        add_dependencies(check "${t}")
    endforeach()

else()
    message("NOTE: gtest not found, testing is disabled.")
endif()

