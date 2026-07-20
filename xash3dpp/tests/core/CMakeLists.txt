# std::jthread in test_thread_role / test_queue_family needs -pthread on the
# GCC/Clang targets; xash3dpp_core links xash3dpp_platform PRIVATE, so nothing
# propagates it (S9.7a adversarial review F8 — the omission is invisible on
# MSVC, where Win32 threads need no separate library).
find_package(Threads REQUIRED)

xash3dpp_add_test(test_log LIBS xash3dpp_core xash3dpp_platform)
xash3dpp_add_test(test_thread_role LIBS xash3dpp_core Threads::Threads)
xash3dpp_add_test(test_queue_family LIBS xash3dpp_core Threads::Threads)
