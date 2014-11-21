cmake_minimum_required(VERSION 2.8)

# The C here is misleading, but FindThreads needs it
project(eris CXX C)

include(GNUInstallDirs)

# Eris package version
set(ERIS_VERSION_MAJOR "0")
set(ERIS_VERSION_MINOR "2")
set(ERIS_VERSION_PATCH "3")
set(ERIS_VERSION "${ERIS_VERSION_MAJOR}.${ERIS_VERSION_MINOR}.${ERIS_VERSION_PATCH}")

set(eris_author "Jason Rhinelander <jason@imaginary.ca>")
set(eris_url "https://github.com/erisproject/eris")
set(eris_description "Agent-based economic modelling library")

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

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
file(GLOB_RECURSE eris_src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
file(GLOB_RECURSE eris_src_include RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/*.h include/*.hpp include/*.h.in include/*.hpp.in)

foreach(h ${eris_src_include})
    if(h MATCHES "\\.in$")
        string(REGEX REPLACE "\\.in$" "" h_processed "${h}")
        configure_file("${h}" "${h_processed}" @ONLY)
        list(APPEND eris_include "${CMAKE_CURRENT_BINARY_DIR}/${h_processed}")
    else()
        configure_file("${h}" "${h}" COPYONLY)
        list(APPEND eris_include "${CMAKE_CURRENT_BINARY_DIR}/${h}")
    endif()
endforeach()

set(CMAKE_CXX_FLAGS "-std=c++11 -march=native -O3 ${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DERIS_DEBUG")

find_package(Threads REQUIRED)

add_library(eris SHARED ${eris_src})

set_target_properties(eris PROPERTIES
    VERSION "${LIBERIS_CURRENT}.${LIBERIS_REVISION}.${LIBERIS_AGE}"
    SOVERSION "${LIBERIS_CURRENT}"
)

target_link_libraries(eris ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(scratch)

add_subdirectory(doc)

enable_testing()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_subdirectory(tests EXCLUDE_FROM_ALL)

set(arch ${CMAKE_HOST_SYSTEM_PROCESSOR})
if (arch STREQUAL x86_64)
    set(arch amd64)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackProjectConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake
    @ONLY)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake)

set(CPACK_DEBIAN_PACKAGE_SECTION libs)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${eris_author}")
#set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${eris_url}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${eris_description}")

set(CPACK_PACKAGE_VERSION_MAJOR ${ERIS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${ERIS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${ERIS_VERSION_PATCH})

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR DEB)
endif()

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)


include(CPack)

# Make liberis.pc for the 'make install' target
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/liberis.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/liberis.pc
    @ONLY)
# And make another, liberis.pc.in, for the cpack targets; this one doesn't
# replace CMAKE_INSTALL_PREFIX, but leaves it to be replaced during packaging
# (in cmake/RegenPkgConfig.cmake), since the packages install into /usr rather
# than CMAKE_INSTALL_PREFIX.
set(prefix_save ${CMAKE_INSTALL_PREFIX})
set(CMAKE_INSTALL_PREFIX "\@CMAKE_INSTALL_PREFIX\@")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/liberis.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/liberis.pc.in
    @ONLY
)
set(CMAKE_INSTALL_PREFIX ${prefix_save})
unset(prefix_save)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RegenPkgConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RegenPkgConfig.cmake
    @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liberis.pc
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT libs)
install(TARGETS eris
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT libs)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/eris/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/eris/"
    COMPONENT headers
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(FILES README.md LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT docs)
