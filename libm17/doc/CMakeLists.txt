find_package(LATEX)
set(enable_dot_graphs ${DOXYGEN_DOT_FOUND})
set(enable_html_docs YES)
set(enable_latex_docs ${LATEX_FOUND})
set(enable_xml_docs NO)
set(Doxygen_output_dirs
  ${CMAKE_CURRENT_BINARY_DIR}/html)
file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR} top_srcdir)
file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR} top_builddir)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    @ONLY)

add_custom_command(
    COMMENT "Doxygen run"
    DEPENDS ${GENERATED_DEPS}
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT ${Doxygen_output_dirs})

add_custom_target(doxygen_docs ALL DEPENDS ${Doxygen_output_dirs})
if(enable_html_docs)
  if(INSTALL_DOCS)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
      DESTINATION ${CMAKE_INSTALL_DOCDIR})
  endif()
endif()
if(enable_latex_docs)
  add_custom_command(
    COMMENT "Generate PDF"
    DEPENDS doxygen_docs ${GENERATED_DEPS}
    COMMAND ${LATEX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.tex
    COMMAND ${LATEX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.tex
    COMMAND ${DVIPDF_CONVERTER} ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.dvi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/latex
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf)
  add_custom_target(doxygen_pdf ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf)
  if(INSTALL_DOCS)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/latex/refman.pdf
      DESTINATION ${CMAKE_INSTALL_DOCDIR})
  endif()
endif()
if(enable_xml_docs)
  if(INSTALL_DOCS)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xml
      DESTINATION ${CMAKE_INSTALL_DOCDIR})
  endif()
endif()
