
include(AddGTest)

if (GTEST_SRC)
    add_definitions(-DERIS_TESTS)

    link_directories(${GTEST_LIB})

    set(eris_tests
        bundle-test
        sim-setup-test
        lock-test
        pos-agent-test
        single-peak-search
        stage-priority
        )
# Not working (need to investigate why; for now just disable):
#        mupd-test
#        incr-buyer-test

    foreach(t ${eris_tests})
        add_executable("${t}" "${t}.cpp")
        target_link_libraries("${t}" eris gtest gtest_main)
        # The following hacks around ctest's serious test target deficiency,
        # http://public.kitware.com/Bug/view.php?id=8774
        #add_test("compile-test-${t}" "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target "${t}")
        add_test(NAME "test-${t}" COMMAND ${t})
        #set_tests_properties("test-${t}" PROPERTIES DEPENDS "compile-test-${t}")
        add_dependencies(check "${t}")
    endforeach()

else()
    message("NOTE: gtest not found, testing is disabled.")
endif()

