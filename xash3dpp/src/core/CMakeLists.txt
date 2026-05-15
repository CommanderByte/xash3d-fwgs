cmake_minimum_required(VERSION 3.16)

# xash3dpp_core — cross-cutting diagnostic and threading primitives.
#
# Provides:
#   • <xash3dpp/core/log.hpp>          — LogLevel, log/logf, callback hook
#   • <xash3dpp/core/assert.hpp>       — XASH_ASSERT, XASH_FATAL macros
#   • <xash3dpp/core/thread_role.hpp>  — ThreadRole registry + assertion
#   • <xash3dpp/private/core/assert_main.hpp> — legacy main-thread wrapper
#
# Layering: sits above xash3dpp_platform (uses platform::console::write as the
#           default log sink).  Every subsystem above core may link this lib;
#           it is the recommended first-touch dependency for all C++ code.

add_library(xash3dpp_core STATIC
    log.cpp
    thread_role.cpp
)

target_include_directories(xash3dpp_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_core PUBLIC cxx_std_20)

# log.cpp calls platform::console::write — PRIVATE because the public API
# (core::log) does not expose any platform types.
target_link_libraries(xash3dpp_core PRIVATE xash3dpp_platform)
