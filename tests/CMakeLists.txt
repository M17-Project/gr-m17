cmake_minimum_required(VERSION 3.8)

# Try CMake config packages first
find_package(GTest QUIET CONFIG)
find_package(GMock QUIET CONFIG)

# Fallback to pkg-config, but only create targets if they don't already exist
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  if(NOT TARGET GTest::gtest)
    pkg_check_modules(GTEST_PKG gtest QUIET)
    if(GTEST_PKG_FOUND)
      # Use the first libdir
      list(GET GTEST_PKG_LIBDIR 0 GTEST_LIBDIR0)
      add_library(GTest::gtest UNKNOWN IMPORTED)
      set_target_properties(GTest::gtest PROPERTIES
        IMPORTED_LOCATION "${GTEST_LIBDIR0}/libgtest.so"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_PKG_INCLUDEDIR}")
      add_library(GTest::gtest_main UNKNOWN IMPORTED)
      set_target_properties(GTest::gtest_main PROPERTIES
        IMPORTED_LOCATION "${GTEST_LIBDIR0}/libgtest_main.so"
        INTERFACE_INCLUDE_DIRECTORIES "${GTEST_PKG_INCLUDEDIR}")
    endif()
  endif()
endif()

# RapidCheck: header-only on Debian/Ubuntu librapidcheck-dev (rapidcheck.h at /usr/include)
find_path(RAPIDCHECK_INCLUDE_DIR NAMES rapidcheck.h rapidcheck/rapidcheck.h
  HINTS /usr/include /usr/local/include)

# Prefer linking to librapidcheck.a if available
find_library(RAPIDCHECK_LIBRARY NAMES rapidcheck
  HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)

if(RAPIDCHECK_LIBRARY)
  add_library(RapidCheck::rapidcheck STATIC IMPORTED)
  set_target_properties(RapidCheck::rapidcheck PROPERTIES
    IMPORTED_LOCATION "${RAPIDCHECK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RAPIDCHECK_INCLUDE_DIR}")
elseif(RAPIDCHECK_INCLUDE_DIR AND NOT TARGET RapidCheck::rapidcheck)
  add_library(RapidCheck::rapidcheck INTERFACE IMPORTED)
  set_target_properties(RapidCheck::rapidcheck PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${RAPIDCHECK_INCLUDE_DIR}")
endif()

include(CTest)

add_subdirectory(unit)

# Optional: valgrind memcheck for tests if available
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
  set(MEMCHECK_ARGS "--leak-check=full" "--error-exitcode=1" "--track-origins=yes")
  add_custom_target(memcheck
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -T memcheck --overwrite MemoryCheckCommand=${VALGRIND_EXECUTABLE} --force-new-ctest-process
    COMMENT "Running tests under valgrind"
  )
endif()


