find_package(Threads REQUIRED)

xash3dpp_add_test(test_host LIBS
    xash3dpp_host
    xash3dpp_core
    xash3dpp_memory
    Threads::Threads
)
xash3dpp_add_test(test_engine_context_networking LIBS
    xash3dpp_host
    xash3dpp_networking
    xash3dpp_platform
    xash3dpp_core
    xash3dpp_memory
    Threads::Threads
)
