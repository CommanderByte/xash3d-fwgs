#pragma once
// xash3dpp — singleton EngineContext accessor for C ABI shims
// Decision ref: docs/design/decisions-architecture.md §3 Q-2,
//               docs/boundaries/host-boundary.md  Resolved-decision OQ-10
//
// This is the **single documented exception** to the "no global accessor" rule:
// game DLLs and other C-ABI callers (`Host_Error`, engine direct-export
// symbols) cross an `extern "C"` boundary and have no other way to reach the
// live EngineContext.  Engine-internal C++ code MUST go through normal
// parameter-passing — never through this accessor.
//
// @thread-safety: the pointer is stored in a std::atomic (release on set,
// acquire on read).  set_current_engine_context() is main-thread-only (called
// once from EngineContext::init/shutdown); current_engine_context() is a
// lock-free read callable from any C-ABI caller thread.

namespace xash {
struct EngineContext;

namespace abi {

// Set by the launcher / host immediately after EngineContext is constructed,
// and cleared just before it is destroyed.  Until the launcher initialises
// the context, current_engine_context() returns nullptr.
void           set_current_engine_context( EngineContext *ctx ) noexcept;
[[nodiscard]] EngineContext *current_engine_context() noexcept;

} // namespace abi
} // namespace xash
