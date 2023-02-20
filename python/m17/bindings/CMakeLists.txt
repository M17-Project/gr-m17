# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT m17_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT m17_sources)

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
