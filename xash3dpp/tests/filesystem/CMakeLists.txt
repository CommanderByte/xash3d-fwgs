cmake_minimum_required(VERSION 3.16)
find_package(Threads REQUIRED)

add_executable(test_filesystem test_filesystem.cpp)
target_link_libraries(test_filesystem PRIVATE xash3dpp_filesystem Threads::Threads)
add_test(NAME test_filesystem COMMAND test_filesystem)

add_executable(test_file test_file.cpp)
target_link_libraries(test_file PRIVATE xash3dpp_filesystem)
add_test(NAME test_file COMMAND test_file)

add_executable(test_ci_directory test_ci_directory.cpp)
target_link_libraries(test_ci_directory PRIVATE xash3dpp_filesystem)
add_test(NAME test_ci_directory COMMAND test_ci_directory)

add_executable(test_backends test_backends.cpp)
target_link_libraries(test_backends PRIVATE xash3dpp_filesystem xash3dpp_miniz xash3dpp_memory)
add_test(NAME test_backends COMMAND test_backends)
