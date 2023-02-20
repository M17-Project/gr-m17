INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_M17 m17)

FIND_PATH(
    M17_INCLUDE_DIRS
    NAMES m17/api.h
    HINTS $ENV{M17_DIR}/include
        ${PC_M17_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    M17_LIBRARIES
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

include("${CMAKE_CURRENT_LIST_DIR}/m17Target.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(M17 DEFAULT_MSG M17_LIBRARIES M17_INCLUDE_DIRS)
MARK_AS_ADVANCED(M17_LIBRARIES M17_INCLUDE_DIRS)
