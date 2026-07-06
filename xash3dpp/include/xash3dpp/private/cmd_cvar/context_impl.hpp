#pragma once
// xash3dpp — cmd_cvar: CmdCvarContext::Impl + shared TU-local helpers (PRIVATE)
//
// This header is included by every context_*.cpp / *_ops.cpp implementation
// file in the cmd_cvar subsystem.  It must NOT be included by any code outside
// xash3dpp/src/cmd_cvar/.
//
// Provides:
//   - CmdCvarContext::Impl struct definition  (pimpl body)
//   - extern thread_local CmdCvarContext *tls_ctx  declaration
//   - inline pool_dup() helper

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>
#include <xash3dpp/private/cmd_cvar/circular_buffer.hpp>
#include <xash3dpp/private/cmd_cvar/cmd_hash_map.hpp>
#include <xash3dpp/private/cmd_cvar/registry_types.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// CmdCvarContext::Impl — pimpl body.
// Complete definition required in every TU that dereferences impl_.
// ---------------------------------------------------------------------------

struct CmdCvarContext::Impl {
    ::xash::memory::PoolHandle pool;

    // Injected dependencies (non-owning)
    ITrustOracle  *trust_oracle  = nullptr;  // @lifetime: caller (injected at init; host-owned)
    ICompatPolicy *compat_policy = nullptr;  // @lifetime: caller (injected at init; host-owned)

    // Observer table: fixed at init, never mutated during runtime.
    struct ObserverEntry {
        ICvarObserver *observer;  // @lifetime: caller (registered via add_cvar_observer; not owned)
        std::uint32_t  flag_mask;
    };
    std::array<ObserverEntry, ::xash::limits::cmd_observer_max> observers{};
    std::size_t   observer_count { 0 };

    // Command queues (deque provides O(1) push_front / push_back).
    // NOTE: std::deque<std::string> uses the system heap, not impl_->pool.
    // Acceptable: each string is a command line consumed within cbuf_execute;
    // the queue is bounded by limits::cbuf_size and is always empty at shutdown.
    std::deque<std::string> cmd_text;         // trusted queue  @pre-reserved: cbuf_size (std::deque: reserve N/A; informally bounded, drained each cbuf_execute, empty at shutdown)
    std::deque<std::string> filteredcmd_text; // stuffcmd (unprivileged) queue  @pre-reserved: cbuf_size (std::deque: reserve N/A; informally bounded, drained each cbuf_execute, empty at shutdown)

    // Scripting state
    int           cmd_wait      { 0 }; // frame-skip counter for 'wait' command
    int           condlevel     { 0 }; // nesting depth for if/else blocks
    std::uint32_t cmd_condition { 0 }; // bitmask of active condition results

    // Stats
    CmdCvarStats stats_block;

    // -----------------------------------------------------------------------
    // Registry — populated during init(); torn down during shutdown().
    // -----------------------------------------------------------------------

    // Cvar registry
    CmdHashMap<Cvar>   cvar_map;                  // case-insensitive lookup
    Cvar              *cvar_list_head { nullptr }; // ABI linked list (CvarAbi.next chain)

    // Built-in engine cvars (engine-static; registered by init()).
    Cvar builtin_cmd_scripting {};
    Cvar builtin_cl_filterstuffcmd {};

    // Command registry
    CmdHashMap<Command>   cmd_map;
    Command              *cmd_list_head { nullptr };

    // Alias registry
    CmdHashMap<AliasDef>  alias_map;
    AliasDef             *alias_list_head { nullptr };

    // Tokenizer scratch — valid only while dispatch_cmd() is on the call stack.
    // Written by cbuf_execute before invoking a CommandFn; reset after.
    // Built-in commands access these via the TLS context pointer.
    static constexpr int k_max_argc = static_cast<int>(::xash::limits::cmd_tokens_max);
    int                                        tok_argc          { 0 };
    std::array<const char *, k_max_argc>       tok_argv          {};
    std::array<char, ::xash::limits::cmd_line_max>     tok_argsBuffer    {};
    bool        tok_is_privileged     { false };

    // DLL lifecycle flags — set by the host layer.
    // cvar_unlink / cmd_unlink check these to prevent double-unlink during reload.
    bool server_dll_loaded { false };
    bool client_dll_loaded { false };

    // Pending safe-unlink list: populated by cvar_prepare_to_unlink() before DLL
    // unload while cvar structs are still valid; consumed by unlink_pending_cvars().
    struct PendingUnlinkEntry {
        const char   *name;        // pool-owned copy  @lifetime: impl-pool (pool_dup'd; reclaimed on destroy_pool)
        std::uint32_t owner_flags;
    };
    // NOTE: uses the system heap, not impl_->pool; bounded by server/client DLL
    // count and is always cleared (pending_unlink.clear()) before destroy_pool.
    std::vector<PendingUnlinkEntry> pending_unlink;  // @pre-reserved: cold path (DLL-unload only; bounded by a DLL's cvar count, cleared before destroy_pool)

#if XASH_DEBUG_CVARS
    // Break-on-write cvar name.  Set via debug_break_on_cvar_write().
    const char *break_on_write_name { nullptr };

    // Circular change log.
    detail::CircularBuffer<CvarChangeRecord, ::xash::limits::cvar_change_log_capacity> change_log;
#endif
};

// ---------------------------------------------------------------------------
// tls_ctx — thread-local context pointer set by cbuf_execute() before each
// CommandFn dispatch.  Defined once in context.cpp; declared here so every
// implementation TU in this subsystem can read/write it.
// ---------------------------------------------------------------------------
extern thread_local CmdCvarContext *tls_ctx;  // @lifetime: borrowed (set to the executing context for the duration of dispatch; nulled after)

// ---------------------------------------------------------------------------
// Cvar ABI list helpers — encapsulate the required reinterpret_cast between
// the ABI-frozen CvarAbi::next (typed CvarAbi*) and the engine-internal
// Cvar list.  abi must be first in Cvar (asserted in cvar.hpp).
// ---------------------------------------------------------------------------
[[nodiscard]] inline Cvar *cvar_list_next(Cvar *cv) noexcept
{
    return reinterpret_cast<Cvar *>(cv->abi.next);
}
[[nodiscard]] inline const Cvar *cvar_list_next(const Cvar *cv) noexcept
{
    return reinterpret_cast<const Cvar *>(cv->abi.next);
}
inline void cvar_list_set_next(Cvar *cv, Cvar *next) noexcept
{
    cv->abi.next = reinterpret_cast<CvarAbi *>(next);
}

// ---------------------------------------------------------------------------
// pool_dup — pool-duplicate a NUL-terminated string.
// Inline so it is available in every implementation TU without an extra TU.
// ---------------------------------------------------------------------------
[[nodiscard]] inline char *pool_dup(::xash::memory::PoolHandle pool, const char *src) noexcept
{
    if (!src) return nullptr;
    const std::size_t n = std::strlen(src) + 1;
    char *dst = static_cast<char *>(::xash::memory::mem_alloc(pool, n));
    if (!dst) return nullptr;
    std::memcpy(dst, src, n);
    return dst;
}

} // namespace xash::cmd_cvar
