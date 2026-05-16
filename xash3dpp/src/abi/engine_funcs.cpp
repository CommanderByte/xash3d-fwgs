// xash3dpp — C ABI direct-export shims for game DLL imports
// Legacy reference: engine/common/host.c (GAME_EXPORT Host_Error)
//                   engine/common/system.c
//
// Decision ref: docs/boundaries/host-boundary.md  Resolved-decision OQ-10
//
// This translation unit houses every `extern "C"` symbol that legacy game /
// client / menu DLLs link against by name (not via the enginefuncs_t vtable).
// Each shim formats its varargs, routes through core::log, and forwards to
// the live EngineContext via xash::abi::current_engine_context().
//
// Existing subsystems used:
//   xash3dpp_core       — core::log_va for fatal logging
//   xash3dpp_host       — Host::signal_frame_abort

#include <xash3dpp/abi/engine_context_accessor.hpp>
#include <xash3dpp/core/error.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/host/engine_context.hpp>
#include <xash3dpp/host/host.hpp>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Singleton accessor (the documented exception to "no global accessor")
// ---------------------------------------------------------------------------

namespace xash::abi {

namespace {
    std::atomic<EngineContext *> g_engine_ctx { nullptr };
} // anonymous namespace

void set_current_engine_context( EngineContext *ctx ) noexcept
{
    g_engine_ctx.store( ctx, std::memory_order_release );
}

EngineContext *current_engine_context() noexcept
{
    return g_engine_ctx.load( std::memory_order_acquire );
}

} // namespace xash::abi

// ---------------------------------------------------------------------------
// GAME_EXPORT direct-export symbols
// ---------------------------------------------------------------------------
//
// Win32:   exported via __declspec(dllexport)
// POSIX:   exported via default visibility (engine binary itself is linked
//          with -fvisibility=hidden in release; these symbols use the
//          attribute to override).

#if defined(_WIN32)
#  define XASH_ABI_EXPORT extern "C" __declspec(dllexport)
#else
#  define XASH_ABI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ---------------------------------------------------------------------------
// Host_Error — game-DLL fatal error path (Quirk Q-14, OQ-10)
// ---------------------------------------------------------------------------
//
// Legacy signature: void Host_Error( const char *error, ... )
// Routes to Host::signal_frame_abort after formatting + Fatal-level logging.

XASH_ABI_EXPORT void Host_Error( const char *fmt, ... )
{
    // Note Q-2 exception: this function may be called from any game-DLL
    // thread, but in practice the legacy engine treats it as main-thread-only
    // (Quirk Q-4 recursion guard relies on host.framecount).  We do not
    // assert_thread_role here because the shim's job is to be unconditionally
    // callable; the recursion guard inside Host::signal_frame_abort enforces
    // the policy.

    std::va_list ap;
    va_start( ap, fmt );
    xash::core::log_va( xash::core::LogLevel::Fatal, "host", fmt, ap );
    va_end( ap );

    if (auto *ctx = xash::abi::current_engine_context()) {
        // Detail is intentionally a fixed string — the body was already
        // logged by log_va above.  The Host's frame-abort fields record only
        // the error code; the human-readable text lives in the log.
        ctx->host.signal_frame_abort( xash::core::ErrorCode::HostFatal,
                                      "game DLL invoked Host_Error" );
        return;
    }

    // No context is wired yet (called before init or after shutdown):
    // there is nothing to abort, so fall through to a hard exit so the
    // game DLL contract is preserved.
    std::fputs( "xash3dpp: Host_Error invoked with no live EngineContext\n",
                stderr );
    std::abort();
}
