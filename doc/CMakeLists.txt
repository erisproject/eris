
add_custom_target(doc)

find_package(Doxygen REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

# Use a custom command to output doxygen's generated index file.  Doing it this way ensures
# that 'make doc' only runs doxygen when something in src or include has changed.
add_custom_command(OUTPUT "api/index.xhtml"
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    DEPENDS "Doxyfile.in" ${eris_src} ${eris_include}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation (using doxygen)"
    )

add_custom_target(api_doc DEPENDS "api/index.xhtml")

set_property(DIRECTORY APPEND PROPERTY
    ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/api")
#set_property(DIRECTORY APPEND PROPERTY
#    ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/api-latex")


add_dependencies(doc api_doc)
