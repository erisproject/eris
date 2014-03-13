cmake_minimum_required(VERSION 2.8)

# The C here is misleading, but FindThreads needs it
project(Eris CXX C)

include(GNUInstallDirs)

# Eris package version
set(ERIS_VERSION_MAJOR "0")
set(ERIS_VERSION_MINOR "1")
set(ERIS_VERSION_PATCH "0")

# eris library version (CURRENT.REVISION.AGE), which is totally separate
# from the above.  See http://www.sourceware.org/autobook/autobook/autobook_91.html
# Basic rules:
# - CURRENT gets incremented for an API change.
# - REVISION changes whenever CURRENT changes:
#   - if the API change was purely an addition, REVISION gets incremented
#   - otherwise, REVISION gets reset to 0
# - AGE changes for every source code change:
#   - if CURRENT changes, AGE reset to 0.
#   - Otherwise AGE gets incremented.
# (So something like 3.7.1 indicates the 8th revision of the liberis-3
# interface, and that this code that would link against liberis-2.* can be
# linked against this version, but *not* liberis-1.* linked code).
set(LIBERIS_CURRENT  "0")
set(LIBERIS_REVISION "0")
set(LIBERIS_AGE      "0")

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
    VERSION "${LIBERIS_CURRENT}.${LIBERIS_REVISION}.${LIBERIS_AGE}"
    SOVERSION "${LIBERIS_CURRENT}"
)

target_link_libraries(eris ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(scratch)

add_subdirectory(doc EXCLUDE_FROM_ALL)

enable_testing()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_subdirectory(tests EXCLUDE_FROM_ALL)

install(TARGETS eris
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT libs)
install(DIRECTORY include/eris/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    COMPONENT headers
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
