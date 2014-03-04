
project(Eris)

cmake_minimum_required(VERSION 2.8)

# Eris package version
set(ERIS_VERSION_MAJOR "0")
set(ERIS_VERSION_MINOR "0")
set(ERIS_VERSION_PATCH "1")

# liberis library version (CURRENT.AGE.REVISION), which is totally separate
# from the above.
# CURRENT gets incremented for an incompatible interface change
# AGE gets incremented for compatible interface changes
# REVISION gets incremented for code changes without interface changes
set(LIBERIS_CURRENT  "0")
set(LIBERIS_AGE      "0")
set(LIBERIS_REVISION "0")


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

include_directories(include)
file(GLOB_RECURSE eris_src src/*.cpp)
file(GLOB_RECURSE eris_include include/*.h include/*.hpp)

set(CMAKE_CXX_FLAGS "-std=c++11 -march=native -O3 ${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DERIS_DEBUG")

find_package(Threads)

add_library(eris SHARED ${eris_src})

set_target_properties(eris PROPERTIES
    VERSION "${LIBERIS_CURRENT}.${LIBERIS_AGE}.${LIBERIS_REVISION}"
    SOVERSION "${LIBERIS_CURRENT}"
)

target_link_libraries(eris ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(scratch)

add_subdirectory(doc EXCLUDE_FROM_ALL)

enable_testing()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_subdirectory(tests EXCLUDE_FROM_ALL)

