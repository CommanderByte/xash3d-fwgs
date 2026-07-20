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
xash3dpp_add_test(test_thread LIBS xash3dpp_platform)

# ---------------------------------------------------------------------------
# dynlib reverse lookup (SAV-OQ-3) — needs a REAL loaded module to exercise
# the export-table walk. Built as its own tiny MODULE fixture right here
# (rather than reusing tests/server/abi's fake_game_dll) because
# tests/server is add_subdirectory()'d AFTER tests/platform in the
# top-level tests/CMakeLists.txt, so that target does not exist yet when
# this file is processed.
# ---------------------------------------------------------------------------
add_library(platform_dynlib_fixture MODULE dynlib_fixture.cpp)
target_compile_features(platform_dynlib_fixture PRIVATE cxx_std_23)

# Named test_dynlib_lookup (not test_dynlib) — tests/utilities already
# registers a test_dynlib target for the unrelated ExportEntry table
# helpers (xash3dpp_utilities' dllfunc_t-style export-slot filler); CMake
# target names are global, so this one needs a distinct name.
xash3dpp_add_test(test_dynlib_lookup SOURCE test_dynlib.cpp LIBS xash3dpp_platform)
target_compile_definitions(test_dynlib_lookup PRIVATE
    DYNLIB_FIXTURE_PATH="$<TARGET_FILE:platform_dynlib_fixture>")
add_dependencies(test_dynlib_lookup platform_dynlib_fixture)
