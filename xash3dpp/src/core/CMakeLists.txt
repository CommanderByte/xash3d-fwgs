cmake_minimum_required(VERSION 3.16)

# xash3dpp_core — engine-tier primitives (clock, error codes).
#
# Provides:
#   • <xash3dpp/core/clock.hpp>        — frame clock + timing cvars
#   • <xash3dpp/core/error.hpp>        — ErrorCode enum + helpers
#
# The DIAGNOSTICS TIER (log.cpp, thread_role.cpp — the implementations behind
# <xash3dpp/core/log.hpp>, <xash3dpp/core/assert.hpp>,
# <xash3dpp/core/thread_role.hpp>, <xash3dpp/private/core/assert_main.hpp>)
# is COMPILED INTO xash3dpp_platform so the base layer is self-contained
# (D-1 dependency hardening, 2026-07-06): platform's console/crash/sockets
# use the assert/thread-role helpers, and the default log sink is platform
# console — hosting those TUs there removes the historical core ⇄ platform
# link cycle.  The headers keep their xash3dpp/core/ paths and the xash::core
# namespace — only target membership moved.  See docs/design/layer-model.md.
#
# Layering: core sits ABOVE platform (one-way: core → platform).

add_library(xash3dpp_core STATIC
    error.cpp
    clock.cpp
)

target_include_directories(xash3dpp_core
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_core PUBLIC cxx_std_23)

# clock/error use the diagnostics tier, which is hosted in platform.
target_link_libraries(xash3dpp_core PRIVATE xash3dpp_platform)
