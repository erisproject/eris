# Finds a source installation of google test and includes its CMakeLists.txt
# into ${CMAKE_CURRENT_BINARY_DIR}/gtest.  The FindGTest distributed with cmake
# (at least as of 2.8.12.1) isn't suitable for the way gtest is actually
# distributed: it is no longer meant to be precompiled.
#
# gtest's CMakeLists.txt is looked for in the following locations:
#
# - (GTEST_ROOT cmake variable)/CMakeLists.txt
# - (GTEST_ROOT environment variable)/CMakeLists.txt
# - ${CMAKE_PREFIX_PATH}/src/gtest/CMakeLists.txt
# - /usr/src/gtest/CMakeLists.txt
#
# If found, GTEST_SRC is set to the path and GTEST_LIB is set to the location of the to-be-compiled
# libgest.  Otherwise both are left unset.

if (NOT GTEST_SRC)
    find_path(GTEST_CMakeLists
        NAMES CMakeLists.txt
        PATHS ${GTEST_ROOT}
            $ENV{GTEST_ROOT}
            "${CMAKE_PREFIX_PATH}/src/gtest"
            "${CMAKE_INSTALL_PREFIX}/src/gtest"
            "/usr/src/gtest"
        DOC "Root path of GTest source installation"
        NO_CMAKE_PATH
        NO_CMAKE_ENVIRONMENT_PATH
        NO_SYSTEM_ENVIRONMENT_PATH
        NO_CMAKE_SYSTEM_PATH)

    if (GTEST_CMakeLists STREQUAL "GTEST_CMakeLists-NOTFOUND")
        message(STATUS "gtest NOT FOUND (try GTEST_ROOT=/path/to/gtest/src)")
    else()
        message(STATUS "Found gtest: ${GTEST_CMakeLists}")
        add_subdirectory(${GTEST_CMakeLists} ${CMAKE_CURRENT_BINARY_DIR}/gtest)
        set(GTEST_SRC "${GTEST_CMakeLists}" CACHE PATH "path to gtest src directory")
        set(GTEST_LIB "${CMAKE_CURRENT_BINARY_DIR}/gtest" CACHE PATH "path to gtest lib directory")
    endif()
endif()
