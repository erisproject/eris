# We probably need to reconfigure the liberis.pc file: the DEB and RPM targets
# install to /usr rather than ${CMAKE_INSTALL_PREFIX}
if(NOT CMAKE_INSTALL_PREFIX STREQUAL /usr)
    if(CPACK_GENERATOR STREQUAL DEB OR CPACK_GENERATOR STREQUAL RPM)
        set(CPACK_INSTALL_SCRIPT ${CPACK_PACKAGE_DIRECTORY}/RegenPkgConfig.cmake)
    endif()
endif()
