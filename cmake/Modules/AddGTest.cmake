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

find_path(GTEST_CMakeLists CMakeLists.txt
    PATHS ${GTEST_ROOT}
        $ENV{GTEST_ROOT}
        "${CMAKE_PREFIX_PATH}/src/gtest"
        "/usr/src/gtest"
    DOC "Root path of GTest source installation"
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH)

add_subdirectory(${GTEST_CMakeLists} ${CMAKE_CURRENT_BINARY_DIR}/gtest)
