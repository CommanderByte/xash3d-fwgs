// xash3dpp — platform (Android) — socket I/O
// Android socket I/O is identical to the POSIX implementation.
//
// This file exists so the CMake platform selection machinery can always
// reference a per-platform android/ source file without special-casing.
// No Android-specific socket behaviour is needed — the standard POSIX socket
// API is fully available on Android (the NDK exposes it via libc).
//
// The build system compiles posix/os_socket.cpp for XASH_PLATFORM_SUBDIR=posix
// (which Android uses), so this file is informational only and is NOT added to
// the CMake source list.  It is retained as documentation of intent and for
// future Android-specific socket extensions (e.g. network capability checks
// via ConnectivityManager JNI if they become necessary).

// If you need to add Android-specific socket behaviour in the future, remove
// the #include below, compile this file directly, and implement the necessary
// overrides.  See android/os_io.cpp for the established pattern.

#if !defined(XASH_ANDROID)
#  error "This file is Android-only (XASH_ANDROID must be defined)"
#endif

// Android shares the POSIX socket implementation.
// NOLINTNEXTLINE(build/include)
#include "../posix/os_socket.cpp"
