
file(GLOB scratch_progs_src RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cpp)

string(REGEX REPLACE ".cpp(;|$)" "\\1" scratch_progs "${scratch_progs_src}")

foreach(prog ${scratch_progs})
    add_executable(${prog} EXCLUDE_FROM_ALL "${prog}.cpp")
    target_link_libraries(${prog} eris)
endforeach()

add_custom_target(scratch DEPENDS ${scratch_progs})
