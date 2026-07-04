find_package(Threads REQUIRED)

xash3dpp_add_test(test_memory LIBS xash3dpp_memory Threads::Threads)
