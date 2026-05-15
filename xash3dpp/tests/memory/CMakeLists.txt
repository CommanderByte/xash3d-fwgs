find_package(Threads REQUIRED)

add_executable(test_memory test_memory.cpp)
target_link_libraries(test_memory PRIVATE xash3dpp_memory Threads::Threads)
add_test(NAME test_memory COMMAND test_memory)
