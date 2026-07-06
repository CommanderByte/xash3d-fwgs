#pragma once
// xash3dpp — server precache index registries (Chunk 6 S7)
// Legacy reference: engine/server/sv_init.c — SV_ModelIndex (:103),
// SV_SoundIndex (:148), SV_EventIndex (:199), SV_GenericIndex (:241) and
// the pfnModelIndex lookup-only variant (sv_game.c:1315).
// Deep dive: docs/legacy-survey/deep-dive-server-lifecycle.md §6.
//
// Four 1-based name tables living in `sv` (per-level — clear() runs at
// every spawn).  Registration quirks preserved exactly:
//   • dedup is case-insensitive (Q_stricmp) and the scan stops at the
//     first empty slot — a hole never forms because slots fill in order;
//   • model/sound strip ONE leading '/' or '\\'; event/generic do not;
//   • all four run COM_FixSlashes ('\\' → '/');
//   • sound rejects sentence names ('!' prefix) with a warning;
//   • table overflow is a hard Host_Error (routed via the error hook);
//   • an index registered while the server is NOT in the loading state is
//     a "late precache": all four kinds notify the sink (S9 wires the
//     svc_resource broadcast there), but only model/sound also log the
//     legacy "late precache of %s" console warning.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::server {

enum class PrecacheKind : std::uint8_t { Model, Sound, Event, Generic };

// Late-precache notification (legacy SV_SendSingleResource call site).
// `flags` is the resource-flag value legacy passes at that moment: the
// CURRENT model flags (0 for a fresh registration — pfnPrecacheModel sets
// RES_FATALIFMISSING only after SV_ModelIndex returns), 0 for sounds,
// RES_FATALIFMISSING for events/generic.
using LatePrecacheSink = void ( * )( void *ctx, PrecacheKind kind,
                                     const char *name, int index,
                                     std::uint32_t flags );

// Same shape as EngineBridge::HostErrorHook (kept a separate alias so this
// header does not depend on the bridge).
using PrecacheErrorHook = void ( * )( void *ctx, const char *msg );

// Capacities default to the legacy protocol limits; tests shrink them to
// exercise the overflow path without thousands of registrations.
struct PrecacheCaps
{
    std::size_t models   = ::xash::limits::sv_max_models;
    std::size_t sounds   = ::xash::limits::sv_max_sounds;
    std::size_t events   = ::xash::limits::sv_max_events;
    std::size_t generics = ::xash::limits::sv_max_generic;
};

class PrecacheTables
{
public:
    PrecacheTables() = default;

    PrecacheTables( const PrecacheTables & )            = delete;
    PrecacheTables &operator=( const PrecacheTables & ) = delete;

    [[nodiscard]] bool init( ::xash::memory::PoolHandle pool,
                             const PrecacheCaps &caps = {} );
    void shutdown();

    // Per-spawn wipe (legacy memset(&sv, 0) covers the inline arrays).
    void clear();

    // Host_SetServerState mirror: true during ss_loading only.
    void set_loading( bool loading ) noexcept { loading_ = loading; }
    [[nodiscard]] bool loading() const noexcept { return loading_; }

    void set_late_sink( LatePrecacheSink fn, void *ctx ) noexcept
    {
        late_sink_     = fn;
        late_sink_ctx_ = ctx;
    }

    void set_error_hook( PrecacheErrorHook fn, void *ctx ) noexcept
    {
        error_hook_     = fn;
        error_hook_ctx_ = ctx;
    }

    // SV_ModelIndex / SV_SoundIndex / SV_EventIndex / SV_GenericIndex:
    // register (or return the existing index of) a name.  0 = rejected.
    [[nodiscard]] int model_index( const char *name );
    [[nodiscard]] int sound_index( const char *name );
    [[nodiscard]] int event_index( const char *name );
    [[nodiscard]] int generic_index( const char *name );

    // pfnModelIndex: lookup WITHOUT registration; logs the legacy
    // "not precached" error and returns 0 on a miss.
    [[nodiscard]] int find_model( const char *name ) const;

    // Slot accessors ("" / 0 when the index is unset or out of range).
    [[nodiscard]] const char *model_name( std::size_t i ) const noexcept;
    [[nodiscard]] const char *sound_name( std::size_t i ) const noexcept;
    [[nodiscard]] const char *event_name( std::size_t i ) const noexcept;
    [[nodiscard]] const char *generic_name( std::size_t i ) const noexcept;

    // sv.model_precache_flags (RES_* bits; set_model_flags ORs like the
    // legacy SetBits call sites).
    [[nodiscard]] std::uint32_t model_flags( std::size_t i ) const noexcept;
    void set_model_flags( std::size_t i, std::uint32_t bits ) noexcept;

    [[nodiscard]] const PrecacheCaps &caps() const noexcept { return caps_; }

private:
    struct Table
    {
        char        *names = nullptr; // @lifetime: pool_-owned — cap × qpath contiguous name storage (mem_calloc'd from pool_)
        std::size_t  cap   = 0;

        [[nodiscard]] char *slot( std::size_t i ) const noexcept;
    };

    [[nodiscard]] int index_in( Table &t, const char *prepared,
                                const char *limit_msg, bool &fresh );
    void late_notify( PrecacheKind kind, const char *name, int index,
                      std::uint32_t flags, bool warn );
    void error( const char *fmt, std::size_t limit );

    ::xash::memory::PoolHandle pool_;
    Table         models_, sounds_, events_, generics_;
    std::uint32_t *model_flags_ = nullptr; // @lifetime: pool_-owned — sv.model_precache_flags mirror (mem_calloc'd from pool_)
    PrecacheCaps  caps_;
    bool          loading_ = false;

    LatePrecacheSink  late_sink_      = nullptr;
    void             *late_sink_ctx_  = nullptr; // @lifetime: caller-owned — opaque context for late_sink_ (installed with the sink, not copied)
    PrecacheErrorHook error_hook_     = nullptr;
    void             *error_hook_ctx_ = nullptr; // @lifetime: caller-owned — opaque context for error_hook_ (installed with the hook, not copied)
};

} // namespace xash::server
