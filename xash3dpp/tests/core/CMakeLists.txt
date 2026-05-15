add_executable(test_log test_log.cpp)
target_link_libraries(test_log PRIVATE xash3dpp_core xash3dpp_platform)
add_test(NAME test_log COMMAND test_log)

add_executable(test_thread_role test_thread_role.cpp)
target_link_libraries(test_thread_role PRIVATE xash3dpp_core)
add_test(NAME test_thread_role COMMAND test_thread_role)
