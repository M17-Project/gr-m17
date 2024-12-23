/*
 * Copyright 2024 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually
 * edited  */
/* The following lines can be configured to regenerate this file during cmake */
/* If manual edits are made, the following tags should be modified accordingly.
 */
/* BINDTOOL_GEN_AUTOMATIC(0) */
/* BINDTOOL_USE_PYGCCXML(0) */
/* BINDTOOL_HEADER_FILE(m17_coder.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(d3d92e833946721163c71aa788430568) */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/m17/m17_coder.h>
// pydoc.h is automatically generated in the build directory
#include <m17_coder_pydoc.h>

void bind_m17_coder(py::module &m) {

  using m17_coder = ::gr::m17::m17_coder;

  py::class_<m17_coder, gr::block, gr::basic_block, std::shared_ptr<m17_coder>>(
      m, "m17_coder", D(m17_coder))

      .def(py::init(&m17_coder::make), py::arg("src_id"), py::arg("dst_id"),
           py::arg("mode"), py::arg("data"), py::arg("encr_type"),
           py::arg("encr_subtype"), py::arg("can"), py::arg("meta"),
           py::arg("key"), py::arg("priv_key"), py::arg("debug"),
           py::arg("signed_str"), D(m17_coder, make))

      .def("set_key", &m17_coder::set_key, py::arg("meta"),
           D(m17_coder, set_key))

      .def("set_priv_key", &m17_coder::set_priv_key, py::arg("meta"),
           D(m17_coder, set_priv_key))

      .def("set_meta", &m17_coder::set_meta, py::arg("meta"),
           D(m17_coder, set_meta))

      .def("set_src_id", &m17_coder::set_src_id, py::arg("src_id"),
           D(m17_coder, set_src_id))

      .def("set_dst_id", &m17_coder::set_dst_id, py::arg("dst_id"),
           D(m17_coder, set_dst_id))

      .def("set_debug", &m17_coder::set_debug, py::arg("debug"),
           D(m17_coder, set_debug))

      .def("set_signed", &m17_coder::set_signed, py::arg("signed_str"),
           D(m17_coder, set_signed))

      .def("set_type", &m17_coder::set_type, py::arg("mode"), py::arg("data"),
           py::arg("encr_type"), py::arg("encr_subtype"), py::arg("can"),
           D(m17_coder, set_type))

      .def("set_mode", &m17_coder::set_mode, py::arg("mode"),
           D(m17_coder, set_mode))

      .def("set_data", &m17_coder::set_data, py::arg("data"),
           D(m17_coder, set_data))

      .def("set_encr_type", &m17_coder::set_encr_type, py::arg("encr_type"),
           D(m17_coder, set_encr_type))

      .def("set_encr_subtype", &m17_coder::set_encr_subtype,
           py::arg("encr_subtype"), D(m17_coder, set_encr_subtype))

      .def("set_can", &m17_coder::set_can, py::arg("can"),
           D(m17_coder, set_can))

      ;
}
