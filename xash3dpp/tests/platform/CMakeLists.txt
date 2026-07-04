xash3dpp_add_test(test_platform LIBS xash3dpp_platform)

# ---------------------------------------------------------------------------
# Per-platform tests — each file contains platform-specific invariants.
# All three compile and run on the current host platform only; they are
# selected here by the CMake platform variables rather than by preprocessor
# guards inside a single source file.
# ---------------------------------------------------------------------------

xash3dpp_add_test(test_os_io LIBS xash3dpp_platform)
xash3dpp_add_test(test_console LIBS xash3dpp_platform)
xash3dpp_add_test(test_crash LIBS xash3dpp_platform)
xash3dpp_add_test(test_os_socket LIBS xash3dpp_platform)
