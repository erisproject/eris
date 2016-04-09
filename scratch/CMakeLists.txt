
file(GLOB scratch_progs_src RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cpp)

string(REGEX REPLACE ".cpp(;|$)" "\\1" scratch_progs "${scratch_progs_src}")

find_package(GSL)
if (GSL_FOUND)
    include_directories(${GSL_INCLUDE_DIRS})
endif()

foreach(prog ${scratch_progs})
    add_executable(${prog} EXCLUDE_FROM_ALL "${prog}.cpp")
    # draw-perf is special: it depends on gsl, and doesn't depend on eris
    if (prog STREQUAL "draw-perf")
        target_link_libraries(${prog} ${GSL_LIBRARIES})
    else()
        target_link_libraries(${prog} eris)
    endif()
endforeach()

add_custom_target(scratch DEPENDS ${scratch_progs})
