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

add_executable(test_cvar_registry test_cvar_registry.cpp)
target_link_libraries(test_cvar_registry PRIVATE cmd_cvar_test_stubs)
add_test(NAME test_cvar_registry COMMAND test_cvar_registry)

add_executable(test_cmd_buffer test_cmd_buffer.cpp)
target_link_libraries(test_cmd_buffer PRIVATE cmd_cvar_test_stubs)
add_test(NAME test_cmd_buffer COMMAND test_cmd_buffer)

add_executable(test_privilege test_privilege.cpp)
target_link_libraries(test_privilege PRIVATE cmd_cvar_test_stubs Threads::Threads)
add_test(NAME test_privilege COMMAND test_privilege)

add_executable(test_compat test_compat.cpp)
target_link_libraries(test_compat PRIVATE cmd_cvar_test_stubs)
add_test(NAME test_compat COMMAND test_compat)

add_executable(test_memory_accounting test_memory_accounting.cpp)
target_link_libraries(test_memory_accounting PRIVATE cmd_cvar_test_stubs)
add_test(NAME test_memory_accounting COMMAND test_memory_accounting)
