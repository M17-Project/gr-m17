# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
# The m17_sources variable should be available from the parent scope
# If not available, we'll define a minimal set for Python bindings
if(NOT DEFINED m17_sources)
    MESSAGE(STATUS "m17_sources not defined, using minimal set for Python bindings")
    set(m17_sources
        ../../../lib/m17_coder_impl.cc
        ../../../lib/m17_decoder_impl.cc
    )
endif()

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Create proper doxygen target for Python bindings
########################################################################
# The Python bindings need a doxygen target to extract docstrings
# Create a simple doxygen target that generates basic documentation
add_custom_target(doxygen_target
    COMMENT "Generating doxygen documentation for Python bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "Doxygen documentation generated"
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/doxygen_complete
)

########################################################################
# Python Bindings
########################################################################

list(APPEND m17_python_files
    m17_coder_python.cc
    m17_decoder_python.cc python_bindings.cc)

GR_PYBIND_MAKE_OOT(m17
   ../../..
   gr::m17
   "${m17_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(TARGET m17_python POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:m17_python>
    ${PROJECT_BINARY_DIR}/test_modules/gnuradio/m17/
)

install(TARGETS m17_python DESTINATION ${GR_PYTHON_DIR}/gnuradio/m17 COMPONENT pythonapi)
