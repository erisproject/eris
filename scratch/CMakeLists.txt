
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

add_custom_target(scratch DEPENDS ${scratch_progs})
