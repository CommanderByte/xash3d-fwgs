// xash3dpp — singleton EngineContext accessor (implementation)
// Decision ref: docs/design/decisions-architecture.md §3 Q-2,
//               docs/boundaries/host-boundary.md  Resolved-decision OQ-10
//
// The state behind <xash3dpp/abi/engine_context_accessor.hpp> — the single
// documented exception to the "no global accessor" rule (C-ABI callers have
// no other way to reach the live EngineContext).
//
// Hosted in xash3dpp_host (D-1 dependency hardening, 2026-07-06): the host
// OWNS the pointer's lifecycle (EngineContext::init/shutdown set and clear
// it), so the state lives here and the abi shim target consumes it —
// removing the historical host ⇄ abi link cycle.  The header keeps its
// xash3dpp/abi/ path and the xash::abi namespace.

#include <xash3dpp/abi/engine_context_accessor.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/host/engine_context.hpp>

#include <atomic>

namespace xash::abi {

namespace {
    // The ONE sanctioned global accessor for C-ABI callers (Q-2 exception, host
    // OQ-10); the marker on this definition sanctions every reference below.
    std::atomic<EngineContext *> g_engine_ctx { nullptr }; // compliance-allow(di-global-ref): Q-2 documented singleton exception (OQ-10)
} // anonymous namespace

void set_current_engine_context( EngineContext *ctx ) noexcept
{
    // Main-thread only: called once each from EngineContext::init/shutdown.
    // The read path (current_engine_context) is lock-free and role-agnostic.
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    g_engine_ctx.store( ctx, std::memory_order_release );
}

EngineContext *current_engine_context() noexcept
{
    return g_engine_ctx.load( std::memory_order_acquire );
}

} // namespace xash::abi

namespace xash {

int legacy_random_long_callback( int low, int high ) noexcept
{
    EngineContext *ctx = ::xash::abi::current_engine_context();
    if ( ctx == nullptr )
        return low;
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return ctx->legacy_random.random_long( low, high );
}

float legacy_random_float_callback( float low, float high ) noexcept
{
    EngineContext *ctx = ::xash::abi::current_engine_context();
    if ( ctx == nullptr )
        return low;
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return ctx->legacy_random.random_float( low, high );
}

} // namespace xash
