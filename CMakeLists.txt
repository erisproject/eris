
project(Eris)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

include_directories(include)
file(GLOB_RECURSE eris_src src/*.cpp)

add_definitions(-std=c++11 -Wall -Werror)

add_library(eris SHARED ${eris_src})

add_subdirectory(scratch)

enable_testing()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_subdirectory(tests EXCLUDE_FROM_ALL)
