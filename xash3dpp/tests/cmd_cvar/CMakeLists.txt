cmake_minimum_required(VERSION 3.16)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Shared helper: a null ITrustOracle and ICompatPolicy for use in all tests
# ---------------------------------------------------------------------------

add_library(cmd_cvar_test_stubs STATIC test_stubs.cpp)
target_link_libraries(cmd_cvar_test_stubs PUBLIC xash3dpp_cmd_cvar)

# ---------------------------------------------------------------------------
# Individual test executables
# ---------------------------------------------------------------------------

xash3dpp_add_test(test_cvar_registry LIBS cmd_cvar_test_stubs)
xash3dpp_add_test(test_cmd_buffer LIBS cmd_cvar_test_stubs)
xash3dpp_add_test(test_privilege LIBS cmd_cvar_test_stubs Threads::Threads)
xash3dpp_add_test(test_compat LIBS cmd_cvar_test_stubs)
xash3dpp_add_test(test_memory_accounting LIBS cmd_cvar_test_stubs)
