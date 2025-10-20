# Doxygen documentation generation
# This file is intentionally minimal since doxygen is handled by libm17/doc/

# Create a simple target that does nothing
# The actual doxygen generation is handled by libm17/doc/CMakeLists.txt
add_custom_target(doxygen_docs_placeholder
    COMMENT "Doxygen documentation placeholder"
)

# Install the existing doxygen documentation
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/html)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/html
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/doxygen
    )
endif()
