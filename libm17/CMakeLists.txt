# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.14)
project(libm17 LANGUAGES C VERSION 0.1
  HOMEPAGE_URL "https://m17project.org/"
  DESCRIPTION "Reference M17 C library.")
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED TRUE)
  set(CMAKE_VERBOSE_MAKEFILE ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)
include(CTest)
enable_testing()

option(BUILD_SHARED_LIB "Build shared library" ON)
option(BUILD_STATIC_LIB "Build static library" ON)
# Check that we're building at least one library
if( NOT ${BUILD_SHARED_LIB} AND NOT ${BUILD_STATIC_LIB} )
  message( FATAL_ERROR "Not building static or shared library - at least one must be built")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  message(STATUS "Build type not specified: defaulting to RelWithDebInfo.")
  # ...As well as some good development default options
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-Wformat -Wformat-security -Werror=format-security)
    add_compile_options (-DFORTIFY_SOURCE=2 -fstack-protector-strong -fPIC -pie)
    add_compile_options (-Wl,-z,relro -Wl,-z,now)
    # SIMD optimizations
    add_compile_options (-march=native -mtune=native)
    # x86_64 SIMD optimizations
    add_compile_options (-msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2)
    # ARM SIMD optimizations (only for ARM)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*|ARM.*|aarch64.*|AARCH64.*")
      add_compile_options (-mfpu=neon)
    endif()
  endif()
  if(MSVC)
    add_compile_options (/W4 /WX)
  else()
    add_compile_options(-Wall -Wextra -pedantic)
  endif()
endif()

set(libm17_SOURCES
encode/convol.c
encode/symbols.c
decode/viterbi.c
decode/symbols.c
phy/sync.c
phy/slice.c
phy/interleave.c
phy/randomize.c
math/rrc.c
math/math.c
math/golay.c
payload/lich.c
payload/call.c
payload/crc.c
crypto/ed25519.c
crypto/curve25519.c
crypto/hkdf.c
crypto/aes_gcm.c
crypto/chacha20_poly1305.c
crypto/secure_memory.c
crypto/validation.c
crypto/key_derivation.c
crypto/replay_protection.c
crypto/bounds_checking.c
    crypto/security_monitoring.c
    crypto/trustzone.c
    crypto/optee.c
    crypto/secure_boot.c
    crypto/constant_time.c
crypto/openpgp_integration.c
crypto/nitrokey_openpgp.c
m17.c
m17_safe.c
m17_simd.c
)

if ( BUILD_STATIC_LIB )
 add_library(m17-static STATIC ${libm17_SOURCES})
 target_include_directories(m17-static PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
 )
 set_target_properties(m17-static PROPERTIES OUTPUT_NAME m17)
 target_link_libraries(m17-static PUBLIC -lm -lssl -lcrypto)
 install(TARGETS m17-static)
endif()

if (BUILD_SHARED_LIB )
 add_library(libm17 SHARED ${libm17_SOURCES})
 target_include_directories(libm17 PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
 )
 set_target_properties(libm17 PROPERTIES
    MACOSX_RPATH TRUE
    OUTPUT_NAME m17
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
 target_link_libraries(libm17 PUBLIC -lm -lssl -lcrypto)
 install(TARGETS libm17 EXPORT libm17)
endif()

# Add test executable for improvements
add_executable(test_improvements test_improvements.c)
target_link_libraries(test_improvements m17-static -lssl -lcrypto)
target_include_directories(test_improvements PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for crypto functions
add_executable(test_crypto test_crypto.c)
target_link_libraries(test_crypto m17-static -lssl -lcrypto)
target_include_directories(test_crypto PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for security functions
add_executable(test_security test_security.c)
target_link_libraries(test_security m17-static -lssl -lcrypto)
target_include_directories(test_security PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for critical security functions
add_executable(test_critical_security test_critical_security.c)
target_link_libraries(test_critical_security m17-static -lssl -lcrypto)
target_include_directories(test_critical_security PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for TrustZone/TEE functions
add_executable(test_trustzone_tee test_trustzone_tee.c)
target_link_libraries(test_trustzone_tee m17-static -lssl -lcrypto)
target_include_directories(test_trustzone_tee PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for ChaCha20-Poly1305 functions
add_executable(test_chacha20_poly1305 test_chacha20_poly1305.c)
target_link_libraries(test_chacha20_poly1305 m17-static -lssl -lcrypto)
target_include_directories(test_chacha20_poly1305 PRIVATE ${PROJECT_SOURCE_DIR})

# Add test executable for OpenPGP integration functions
add_executable(test_openpgp_integration crypto/test_openpgp_integration.c)
target_link_libraries(test_openpgp_integration m17-static -lssl -lcrypto)
target_include_directories(test_openpgp_integration PRIVATE ${PROJECT_SOURCE_DIR})

# CMake interface files
include( CMakePackageConfigHelpers )

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17Config.cmake"
"include (\${CMAKE_CURRENT_LIST_DIR}/libm17Targets.cmake)\n")

write_basic_package_version_file(
 ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17ConfigVersion.cmake
  VERSION ${libm17_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(EXPORT libm17
  FILE
     libm17Targets.cmake
  NAMESPACE
     libm17::
  DESTINATION
     ${CMAKE_INSTALL_LIBDIR}/cmake/libm17
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17ConfigVersion.cmake
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/libm17
)

install(FILES m17.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
add_subdirectory(unit_tests)

# Add new TNC, RF, Bridge, and Controller modules
add_subdirectory(tnc)
add_subdirectory(bridge)
add_subdirectory(controller)

option (BUILD_DOCS "Build Doxygen docs" OFF)
if(BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    add_subdirectory(doc)
  endif()
endif()
