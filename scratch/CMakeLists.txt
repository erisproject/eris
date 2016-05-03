
file(GLOB scratch_progs_src RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cpp)

string(REGEX REPLACE ".cpp(;|$)" "\\1" scratch_progs "${scratch_progs_src}")

find_package(GSL)
if (GSL_FOUND)
    include_directories(${GSL_INCLUDE_DIRS})
endif()

foreach(prog ${scratch_progs})
    add_executable(${prog} EXCLUDE_FROM_ALL "${prog}.cpp")
    if (prog STREQUAL "draw-perf")
        # draw-perf also depends on gsl
        target_link_libraries(${prog} eris ${GSL_LIBRARIES})
    else()
        target_link_libraries(${prog} eris)
    endif()
endforeach()

execute_process(COMMAND dpkg-architecture -qDEB_TARGET_ARCH
    RESULT_VARIABLE dpkg_arch_result
    OUTPUT_VARIABLE dpkg_arch
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if (dpkg_arch_result STREQUAL 0)
    message(STATUS "Debian system detected; enabling eris-symlinks-deb target")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eris-symlinks DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target(eris-symlinks-deb
        dpkg-buildpackage -uc -us -b
        COMMAND mv ../eris-symlinks_0_${dpkg_arch}.deb ../..
        DEPENDS eris api_doc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/eris-symlinks
        )
else()
    message(STATUS "Not a debian system (or dpkg-dev not installed); not enabling eris-symlinks-deb target")
endif()
