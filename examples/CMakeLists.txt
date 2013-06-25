
project(eris_examples)

file(GLOB_RECURSE eris_examples *.cpp)

foreach(e_src ${eris_examples})
    get_filename_component(e "${e_src}" NAME_WE)
    add_executable("${e}" "${e_src}")
    target_link_libraries("${e}" eris)
    add_dependencies(examples "${e}")
endforeach()

