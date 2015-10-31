cmake_minimum_required(VERSION 2.8)

# The C here is misleading, but FindThreads needs it
project(eris CXX C)

include(GNUInstallDirs)

set(eris_author "Jason Rhinelander <jason@imaginary.ca>")
set(eris_url "https://git.imaginary.ca/eris/eris")
set(eris_description "Agent-based economic modelling library")

# Eris package version
set(ERIS_VERSION_MAJOR "0")
set(ERIS_VERSION_MINOR "3")
set(ERIS_VERSION_PATCH "6")
set(ERIS_VERSION "${ERIS_VERSION_MAJOR}.${ERIS_VERSION_MINOR}.${ERIS_VERSION_PATCH}")

# eris library version (CURRENT.REVISION.AGE), which is totally separate
# from the above.  See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Basic rules for updating these when releasing a new eris version:
# - If the new release has an API change:
#   - CURRENT gets updated
#   - REVISION gets set to 0
#   - If the API change was purely an addition, increment AGE
#   - Otherwise (i.e. an API change or removal), reset AGE to 0
# - Otherwise, if the release has no API change but has code changes:
#   - REVISION gets incremented
#   - (CURRENT and AGE stay the same)
# If there is no code change (e.g. the release is just a documentation update)
# then none of these change.
#
# (So something like 3.7.1 indicates the 8th revision of the liberis.so.3
# interface, and that code that links against liberis.so.2 can safely link
# against this version, but code that links against liberis.so.1 cannot.
set(LIBERIS_CURRENT  "4")
set(LIBERIS_REVISION "0")
set(LIBERIS_AGE      "0")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

# Look for include-what-you-use, if requested
option(IWYU "Enable include-what-you-use header debugging support" OFF)
if (IWYU)
    cmake_minimum_required(VERSION 3.3)
    find_program(IWYU_PATH NAMES "iwyu" "include-what-you-use")
    if (NOT IWYU_PATH)
        message(FATAL_ERROR "Could not find executable for include-what-you-use support")
    endif()
    set(IWYU_ARGS ${IWYU_PATH} "-Xiwyu" "--mapping_file=${CMAKE_SOURCE_DIR}/eris.imp")

    message(STATUS "include-what-you-use header debugging support enabled (-DIWYU=off to disable)")
else()
    message(STATUS "include-what-you-use header debugging support disabled (-DIWYU=on to enable)")
endif()

option(API_DOCS "Enable creativity API documentation generation (requires doxygen)" ON)
if (API_DOCS)
    message(STATUS "API documentation generation enabled (-DAPI_DOCS=off to disable)")
else()
    message(STATUS "API documentation generation disabled (-DAPI_DOCS=on to enable)")
endif()

file(GLOB_RECURSE eris_src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
file(GLOB_RECURSE eris_src_include RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.h include/*.hpp include/*.h.in include/*.hpp.in)

foreach(h ${eris_src_include})
    if(h MATCHES "\\.in$")
        string(REGEX REPLACE "\\.in$" "" h_processed "${h}")
        configure_file("${h}" "${h_processed}" @ONLY)
        list(APPEND eris_doc_include "${CMAKE_CURRENT_BINARY_DIR}/${h_processed}")
    else()
        list(APPEND eris_doc_include "${CMAKE_CURRENT_SOURCE_DIR}/${h}")
    endif()
endforeach()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

set(CMAKE_CXX_FLAGS "-std=c++11 -O2 -Wall -Wextra ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DERIS_DEBUG")

find_package(Threads REQUIRED)

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_library(eris SHARED ${eris_src})

set_target_properties(eris PROPERTIES
    VERSION "${LIBERIS_CURRENT}.${LIBERIS_REVISION}.${LIBERIS_AGE}"
    SOVERSION "${LIBERIS_CURRENT}"
)

if (IWYU)
    set_property(TARGET eris PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_ARGS})

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/iwyu.cpp "// Do-nothing file for IWYU testing of headers without associated .cpp files\n")
    foreach(h ${eris_src_include})
        if ((NOT h MATCHES "\\.in$")
                AND (NOT h STREQUAL "include/eris/debug.hpp") # The macro voodoo in here confuses IWYU
                )
            string(REGEX REPLACE "include/eris/(.*)\\.hpp$" "src/eris/\\1.cpp" src "${h}")
            if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
                string(REGEX REPLACE "^include/" "" h_noinc "${h}")
                file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/iwyu.cpp "#include <${h_noinc}> // IWYU pragma: keep\n")
                list(APPEND IWYU_ARGS_HPP_ONLY "-Xiwyu" "--check_also=${CMAKE_CURRENT_SOURCE_DIR}/${h}")
            endif()
        endif()
    endforeach()
    add_library(iwyu-helper-target OBJECT "${CMAKE_CURRENT_BINARY_DIR}/iwyu.cpp")
    set_property(TARGET iwyu-helper-target PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_ARGS} ${IWYU_ARGS_HPP_ONLY})
endif()

target_link_libraries(eris ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(scratch)

if (API_DOCS)
    add_subdirectory(doc)
endif()

enable_testing()
# Include a "check" target that works just like the "test" target, but in verbose mode.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)
add_subdirectory(tests EXCLUDE_FROM_ALL)

set(arch ${CMAKE_HOST_SYSTEM_PROCESSOR})
if (arch STREQUAL x86_64)
    set(arch amd64)
endif()

# Make liberis.pc for the 'make install' target
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/liberis.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/liberis.pc
    @ONLY)

install(TARGETS eris
    DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liberis.pc
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/eris/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/eris/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(FILES README.md LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
