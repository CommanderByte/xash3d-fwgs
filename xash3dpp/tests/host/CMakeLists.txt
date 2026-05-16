find_package(Threads REQUIRED)

add_executable(test_host test_host.cpp)
target_link_libraries(test_host PRIVATE
    xash3dpp_host
    xash3dpp_core
    xash3dpp_memory
    Threads::Threads
)
add_test(NAME test_host COMMAND test_host)
