cmake_minimum_required(VERSION 2.8)

# The C here is misleading, but FindThreads needs it
project(eris CXX C)

include(GNUInstallDirs)

set(eris_author "Jason Rhinelander <jason@imaginary.ca>")
set(eris_url "https://git.imaginary.ca/eris/eris")
set(eris_description "Agent-based economic modelling library")

# Eris package version
set(ERIS_VERSION_MAJOR "0")
set(ERIS_VERSION_MINOR "5")
set(ERIS_VERSION_PATCH "900")
set(ERIS_VERSION "${ERIS_VERSION_MAJOR}.${ERIS_VERSION_MINOR}.${ERIS_VERSION_PATCH}")

# eris library version (CURRENT.REVISION.AGE), which is totally separate
# from the above.  See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# Basic rules for updating these when releasing a new eris version:
# - If the new release has an API change:
#   - CURRENT gets updated
#   - REVISION gets set to 0
#   - If the API change was purely an addition (see below), increment AGE
#   - Otherwise (i.e. an API change or removal), reset AGE to 0
# - Otherwise, if the release has no API change but has logic changes:
#   - REVISION gets incremented
#   - (CURRENT and AGE stay the same)
# If there is no code change (e.g. the release is just a documentation update)
# then none of these change.
#
# As to what constitutes an "addition": the conservative rule is that any
# non-cosmetic change in a .hpp file (i.e. changing an existing class or inline
# definition) makes the ABI not backwards compatible and thus requires
# resetting AGE.  Adding new classes and new functions is safe, as is adding
# new *non-virtual* methods to existing classes (adding virtual methods is
# not).  Changing inlined code (including templated code) is also not safe.
# Any change to the members of the class--addition, removal, type, cv, or order
# changes--also changes the ABI and requires an AGE reset.
#
# Changing implementation--i.e. in .cpp files--is, however, generally safe.  A
# release with only logic changes, but no .hpp changes, generally doesn't need
# CURRENT to be updated at all, but rather just REVISION.
#
# (So something like 3.7.1 indicates the 8th revision of the liberis.so.3
# interface, and that code that links against liberis.so.2 can safely link
# against this version, but code that links against liberis.so.1 cannot.
set(LIBERIS_CURRENT  "7")
set(LIBERIS_REVISION "0")
set(LIBERIS_AGE      "0")

cmake_policy(SET CMP0042 NEW)

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

set(api_docs_default ON)
if (DEFINED API_DOCS)
else()
    find_package(Doxygen 1.8.2)
    if (NOT DOXYGEN_FOUND OR NOT DOXYGEN_DOT_FOUND)
        message(STATUS "doxygen (with dot) not found: setting default API_DOCS to OFF.  Use -DAPI_DOCS=(on|off) to override")
        set(api_docs_default OFF)
    endif()
endif()

option(API_DOCS "Enable creativity API documentation generation (requires doxygen)" ${api_docs_default})
if (API_DOCS)
    message(STATUS "API documentation generation enabled (-DAPI_DOCS=off to disable)")
else()
    message(STATUS "API documentation generation disabled (-DAPI_DOCS=on to enable)")
endif()

file(GLOB_RECURSE eris_src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} eris/*.cpp)
file(GLOB_RECURSE eris_src_include RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} eris/*.h eris/*.hpp eris/*.h.in eris/*.hpp.in)

foreach(h ${eris_src_include})
    if(h MATCHES "\\.in$")
        string(REGEX REPLACE "\\.in$" "" h_processed "${h}")
        configure_file("${h}" "${h_processed}" @ONLY)
        list(APPEND eris_doc_include "${CMAKE_CURRENT_BINARY_DIR}/${h_processed}")
    else()
        list(APPEND eris_doc_include "${CMAKE_CURRENT_SOURCE_DIR}/${h}")
    endif()
endforeach()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS "-std=c++14 -O2 -Wall -Wextra ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DERIS_DEBUG")

find_package(Threads REQUIRED)

# Move semantics were during 3.3 development, and backported in 3.2.7:
find_package(Eigen3 REQUIRED)
if (EIGEN3_VERSION VERSION_LESS "3.2.7")
    message(FATAL_ERROR "Eigen >= 3.2.7 required, but found v${EIGEN3_VERSION}")
else()
    message(STATUS "Eigen version: ${EIGEN3_VERSION}")
endif()
include_directories(${EIGEN3_INCLUDE_DIR})

find_package(Boost REQUIRED COMPONENTS filesystem thread system chrono date_time atomic)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(LZMA REQUIRED liblzma)
include_directories(${LZMA_INCLUDE_DIRS})
link_directories(${LZMA_LIBRARY_DIRS})
add_definitions(${LZMA_CFLAGS_OTHER})

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
                AND (NOT h STREQUAL "eris/debug.hpp") # The macro voodoo in here confuses IWYU
                )
            string(REGEX REPLACE "eris/(.*)\\.hpp$" "eris/\\1.cpp" src "${h}")
            if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
                file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/iwyu.cpp "#include <${h}> // IWYU pragma: keep\n")
                list(APPEND IWYU_ARGS_HPP_ONLY "-Xiwyu" "--check_also=${CMAKE_CURRENT_SOURCE_DIR}/${h}")
            endif()
        endif()
    endforeach()
    add_library(iwyu-helper-target OBJECT "${CMAKE_CURRENT_BINARY_DIR}/iwyu.cpp")
    set_property(TARGET iwyu-helper-target PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_ARGS} ${IWYU_ARGS_HPP_ONLY})
endif()

target_link_libraries(eris ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${LZMA_LIBRARIES})

option(SCRATCH "Enable build support for programs in scratch" ON)
if (SCRATCH)
    add_subdirectory(scratch)
endif()

if (API_DOCS)
    add_subdirectory(doc)
endif()

enable_testing()
# CMake's default "test" target doesn't support deps, so add a check target
# that we can add dependencies to to build the tests.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
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
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/eris/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/eris/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(FILES README.md LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
