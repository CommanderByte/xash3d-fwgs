# std::jthread in test_thread_role / test_queue_family needs -pthread on the
# GCC/Clang targets; xash3dpp_core links xash3dpp_platform PRIVATE, so nothing
# propagates it (S9.7a adversarial review F8 — the omission is invisible on
# MSVC, where Win32 threads need no separate library).
find_package(Threads REQUIRED)

xash3dpp_add_test(test_log LIBS xash3dpp_core xash3dpp_platform)
xash3dpp_add_test(test_thread_role LIBS xash3dpp_core Threads::Threads)
xash3dpp_add_test(test_queue_family LIBS xash3dpp_core Threads::Threads)

add_executable(test_legacy_random
    test_legacy_random.cpp
    legacy_random_oracle.c
)
target_link_libraries(test_legacy_random PRIVATE xash3dpp_core)
if(NOT MSVC)
    # common.c relied on the legacy compiler's two's-complement overflow for
    # its 32-bit range expression. Keep the C oracle on that effective model.
    target_compile_options(test_legacy_random PRIVATE $<$<COMPILE_LANGUAGE:C>:-fwrapv>)
endif()
add_test(NAME test_legacy_random COMMAND test_legacy_random)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME verify_legacy_random_oracle
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_legacy_random_oracle.py
            ${PROJECT_SOURCE_DIR}/../engine/common/common.c
            ${CMAKE_CURRENT_SOURCE_DIR}/legacy_random_oracle.c
)
