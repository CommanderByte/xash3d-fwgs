add_executable(test_platform test_platform.cpp)
target_link_libraries(test_platform PRIVATE xash3dpp_platform)
add_test(NAME test_platform COMMAND test_platform)

add_executable(test_log test_log.cpp)
target_link_libraries(test_log PRIVATE xash3dpp_platform)
add_test(NAME test_log COMMAND test_log)

# ---------------------------------------------------------------------------
# Per-platform tests — each file contains platform-specific invariants.
# All three compile and run on the current host platform only; they are
# selected here by the CMake platform variables rather than by preprocessor
# guards inside a single source file.
# ---------------------------------------------------------------------------

add_executable(test_os_io test_os_io.cpp)
target_link_libraries(test_os_io PRIVATE xash3dpp_platform)
add_test(NAME test_os_io COMMAND test_os_io)

add_executable(test_console test_console.cpp)
target_link_libraries(test_console PRIVATE xash3dpp_platform)
add_test(NAME test_console COMMAND test_console)

add_executable(test_crash test_crash.cpp)
target_link_libraries(test_crash PRIVATE xash3dpp_platform)
add_test(NAME test_crash COMMAND test_crash)
