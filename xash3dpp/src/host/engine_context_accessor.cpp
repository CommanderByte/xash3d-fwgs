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

#include <atomic>

namespace xash::abi {

namespace {
    std::atomic<EngineContext *> g_engine_ctx { nullptr };  // Q-2 exception (OQ-10)
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
