cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Per-OS subdirectory selection
# ---------------------------------------------------------------------------
# To port to a new OS, create a new subdirectory (e.g. src/platform/widgetos/)
# and implement the mandatory files:
#   sys.cpp     — time, sleep, dynlib, paths, debugger, message_box, shell_execute
#   os_io.cpp   — file open/close/read/write/seek/stat, directory ops
#   console.cpp — system console I/O
#   crash.cpp   — crash handler and stack trace
# Then add a new elseif branch below.

if(WIN32)
    set(XASH_PLATFORM_SUBDIR win32)
    set(XASH_PLATFORM_IO_EXTRA "")
elseif(ANDROID)
    # Android uses POSIX sys.cpp + android-specific os_io/console/crash.
    set(XASH_PLATFORM_SUBDIR posix)
    set(XASH_PLATFORM_IO_EXTRA android/os_io.cpp)
else()
    set(XASH_PLATFORM_SUBDIR posix)
    set(XASH_PLATFORM_IO_EXTRA "")
endif()

# console and crash: Android has its own; all others use the subdir variant.
if(ANDROID)
    set(XASH_PLATFORM_CONSOLE_SRC android/console.cpp)
    set(XASH_PLATFORM_CRASH_SRC   android/crash.cpp)
else()
    set(XASH_PLATFORM_CONSOLE_SRC ${XASH_PLATFORM_SUBDIR}/console.cpp)
    set(XASH_PLATFORM_CRASH_SRC   ${XASH_PLATFORM_SUBDIR}/crash.cpp)
endif()

# Mandatory files present on every platform.
# os_socket.cpp: Android uses the POSIX implementation (XASH_PLATFORM_SUBDIR=posix);
# Win32 and POSIX each have their own.  Android-specific socket additions belong in
# android/os_socket.cpp (not compiled here; see that file's header comment).
set(XASH_PLATFORM_SRC
    ${XASH_PLATFORM_SUBDIR}/sys.cpp
    ${XASH_PLATFORM_SUBDIR}/os_io.cpp
    ${XASH_PLATFORM_SUBDIR}/os_socket.cpp
    ${XASH_PLATFORM_SUBDIR}/thread.cpp
    ${XASH_PLATFORM_IO_EXTRA}
    ${XASH_PLATFORM_CONSOLE_SRC}
    ${XASH_PLATFORM_CRASH_SRC}
)

# Diagnostics tier (D-1 dependency hardening, 2026-07-06): the log and
# thread-role implementations are HOSTED here so the base layer is
# self-contained — platform's console/crash/sockets assert thread roles, and
# the default log sink is platform console.  Files stay under src/core/ and
# keep the xash::core namespace + <xash3dpp/core/...> header paths; only
# target membership moved.  This removes the historical core ⇄ platform link
# cycle (see docs/design/layer-model.md).
set(XASH_DIAGNOSTICS_SRC
    ${PROJECT_SOURCE_DIR}/src/core/log.cpp
    ${PROJECT_SOURCE_DIR}/src/core/thread_role.cpp
)

add_library(xash3dpp_platform STATIC
    ${XASH_PLATFORM_SRC}
    ${XASH_DIAGNOSTICS_SRC}
)

target_include_directories(xash3dpp_platform
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

# C++23 is required for std::expected<T,E> (introduced with the sockets layer).
target_compile_features(xash3dpp_platform PUBLIC cxx_std_23)

# xash3dpp_utilities is used internally for path normalisation and the log
# formatter (PRIVATE — not leaked into xash3dpp_platform's public API).
# The diagnostics tier (log/assert/thread-role) is hosted IN this target, so
# platform does not link core — the layer graph is one-way (core → platform).
target_link_libraries(xash3dpp_platform PRIVATE xash3dpp_utilities)

# thread.cpp (Q-24 spawn_thread) constructs a real std::thread — on POSIX
# (glibc pre-2.34, most other libc implementations) that requires linking
# pthread explicitly at the final executable. PUBLIC so every consumer of
# xash3dpp_platform picks up the requirement transitively instead of each
# downstream target re-discovering it (see tests/{host,memory,filesystem,
# cmd_cvar}/CMakeLists.txt for the previous per-test workaround). No-op on
# Win32 (FindThreads resolves to an empty INTERFACE target there).
find_package(Threads REQUIRED)
target_link_libraries(xash3dpp_platform PUBLIC Threads::Threads)

# Win32: link Winsock2 for the sockets layer.
if(WIN32)
    target_link_libraries(xash3dpp_platform PRIVATE Ws2_32)
endif()

# POSIX: dlopen/dlsym/dlclose live in libdl on Linux; on macOS they are in libc.
if(UNIX AND NOT APPLE)
    target_link_libraries(xash3dpp_platform PRIVATE dl)
endif()

# Android: link NDK AAsset library and define XASH_ANDROID for the TUs.
if(ANDROID)
    target_link_libraries(xash3dpp_platform PRIVATE android log)
    target_compile_definitions(xash3dpp_platform PRIVATE XASH_ANDROID)
endif()
