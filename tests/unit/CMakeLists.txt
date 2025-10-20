cmake_minimum_required(VERSION 3.8)

set(TEST_SOURCES
  test_basic.cpp
)

add_executable(unit_gtest ${TEST_SOURCES})
target_compile_features(unit_gtest PRIVATE cxx_std_17)

target_include_directories(unit_gtest PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

if(TARGET GTest::gtest)
  target_link_libraries(unit_gtest PRIVATE GTest::gtest GTest::gtest_main)
elseif(GTest_FOUND)
  # Already created imported targets above
  target_link_libraries(unit_gtest PRIVATE GTest::gtest GTest::gtest_main)
else()
  message(WARNING "GTest not found; unit_gtest will build without tests")
endif()

if(TARGET GMock::gmock)
  target_link_libraries(unit_gtest PRIVATE GMock::gmock GMock::gmock_main)
endif()

add_test(NAME unit_gtest COMMAND unit_gtest)

if(TARGET RapidCheck::rapidcheck)
  add_executable(unit_rc test_rc.cpp)
  target_compile_features(unit_rc PRIVATE cxx_std_17)
  target_link_libraries(unit_rc PRIVATE RapidCheck::rapidcheck)
  add_test(NAME unit_rc COMMAND unit_rc)
else()
  message(WARNING "RapidCheck headers not found; skipping RC tests")
endif()


