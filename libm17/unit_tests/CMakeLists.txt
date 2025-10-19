find_package(unity)

if(unity_FOUND)
 add_executable(unit_tests unit_tests.c)
 target_link_libraries(unit_tests PRIVATE libm17 unity::framework)
 add_test(NAME unit_tests COMMAND unit_tests)
endif()
