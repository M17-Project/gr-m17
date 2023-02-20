find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_M17 gnuradio-m17)

FIND_PATH(
    GR_M17_INCLUDE_DIRS
    NAMES gnuradio/m17/api.h
    HINTS $ENV{M17_DIR}/include
        ${PC_M17_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_M17_LIBRARIES
    NAMES gnuradio-m17
    HINTS $ENV{M17_DIR}/lib
        ${PC_M17_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-m17Target.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_M17 DEFAULT_MSG GR_M17_LIBRARIES GR_M17_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_M17_LIBRARIES GR_M17_INCLUDE_DIRS)
