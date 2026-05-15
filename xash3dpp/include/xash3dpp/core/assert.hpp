#pragma once
// xash3dpp — assertion macros (ASSERTIONS / QH, decisions-style.md)
//
// Library: xash3dpp_core (cross-cutting invariant checks usable from any
//          subsystem).  XASH_FATAL expands to a call into xash::core::logf —
//          translation units that use XASH_FATAL must link xash3dpp_core.
//
// Two-tier assertion policy:
//
//   XASH_ASSERT(expr)
//     Debug-only invariant check.  No-op in NDEBUG / release builds.
//     Use for cheap invariants that are true by construction in correct code
//     (e.g. pointer not null, array index in range, subsystem initialised).
//     Does NOT call core::log — it fires immediately via the platform
//     debugger break / abort path.
//
//   XASH_FATAL(expr, msg)
//     Always-on invariant check.  Fires in both debug and release builds.
//     Use for invariants whose violation indicates data corruption or a porting
//     bug that would cause silent, hard-to-diagnose misbehaviour downstream.
//     Logs via core::logf(LogLevel::Fatal, ...) before aborting — producing
//     a human-readable message in the crash log / stderr.
//
// Neither macro throws.  Both abort the process when the condition is false.
// They are distinct from Q-5 error returns: assertions are for invariants
// (programmer bugs), not for recoverable runtime failures.
//
// Do NOT use assert() from <cassert>:
//   • On MSVC /EHs-c-, assert() uses _invoke_watson which is inconsistent.
//   • assert() provides no diagnostic log call.
//   • XASH_ASSERT replaces it completely.

#include <xash3dpp/core/log.hpp>    // core::log, LogLevel

// ---------------------------------------------------------------------------
// Portability helpers (debugger break)
// ---------------------------------------------------------------------------

#if defined( _MSC_VER )
#  include <intrin.h>
#  define XASH_DEBUG_BREAK() __debugbreak()
#elif defined( __has_builtin ) && __has_builtin( __builtin_debugtrap )
#  define XASH_DEBUG_BREAK() __builtin_debugtrap()
#elif defined( __GNUC__ ) || defined( __clang__ )
#  include <csignal>
#  define XASH_DEBUG_BREAK() __builtin_trap()
#else
#  include <cstdlib>
#  define XASH_DEBUG_BREAK() std::abort()
#endif

// ---------------------------------------------------------------------------
// XASH_ASSERT — debug-only
// ---------------------------------------------------------------------------

#ifndef NDEBUG
#  define XASH_ASSERT( expr ) \
    do { \
        if( !( expr ) ) [[unlikely]] { \
            XASH_DEBUG_BREAK(); \
        } \
    } while( 0 )
#else
#  define XASH_ASSERT( expr ) do { (void)sizeof( expr ); } while( 0 )
#endif

// ---------------------------------------------------------------------------
// XASH_FATAL — always-on
// ---------------------------------------------------------------------------
// Uses __FILE__ / __LINE__ for a compact location string.  Logging before
// aborting ensures the reason appears in stderr / logfiles even in release.

#include <cstdlib>    // std::abort

#define XASH_FATAL( expr, msg ) \
    do { \
        if( !( expr ) ) [[unlikely]] { \
            ::xash::core::logf( \
                ::xash::core::LogLevel::Fatal, \
                "assert", \
                "FATAL: %s  [" __FILE__ ":%d]  %s", \
                #expr, __LINE__, ( msg ) ); \
            XASH_DEBUG_BREAK(); \
            std::abort(); \
        } \
    } while( 0 )
