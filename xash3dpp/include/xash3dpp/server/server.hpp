#pragma once
// xash3dpp — Server: dedicated-server core (Chunk 6)
// Legacy reference: engine/server/
//   (sv_main.c, sv_game.c, sv_world.c, sv_phys.c, sv_pmove.c, sv_frame.c,
//    sv_client.c — plus sv_query.c/sv_filter.c/sv_log.c same-target
//    satellites per the boundary Q-11 table)
//
// Boundary spec: docs/boundaries/server-boundary.md (authoritative surface).
// Decision refs:
//   • Q-19 PHS_PLACEMENT — PHS lives in map_loader as a load-time query
//     module; the server consumes it through the query API only.
//   • Q-20 EDICT_STORE — the ABI-exact edict array is the single
//     authoritative store behind a zero-cost typed accessor seam; raw
//     entvars_t access is confined to src/server/abi/, the pmove bridge,
//     and the Chunk 8 save serializer.

#include <atomic>
#include <cstdint>
#include <memory>

namespace xash::server {

// ---------------------------------------------------------------------------
// Stats (three-tier model — docs/design/debug-stats-design.md)
// ---------------------------------------------------------------------------

struct ServerStats
{
    // Tier 1 — always-on: ≤ 1 relaxed atomic per event.
    std::atomic<std::uint64_t> frames_run{ 0 };

#if XASH_STATS
    // Tier 2 — lightweight bookkeeping (peaks, high-water marks).
    // TODO(chunk6-S8): peak_active_entities, snapshot high-water marks.
#endif

#if XASH_DEBUG_SERVER
    // Tier 3 — dev-only heavy tracing.
#endif
};

// ---------------------------------------------------------------------------
// ServerInitParams
// ---------------------------------------------------------------------------

struct ServerInitParams
{
    // Injected dependencies (Q-4).  Wired when the server joins
    // EngineContext (ladder S7); until then the scaffold runs standalone.
    // TODO(chunk6-S7): cmd_cvar context (+ ITrustOracle seam per cmd_cvar
    // D2), networking, map_loader (WorldData + GameState FSM observer),
    // filesystem, host services (feature flags, ICompatPolicy per Q-12,
    // frame-rate gate per host OQ-11).
};

// ---------------------------------------------------------------------------
// Server (pimpl)
// ---------------------------------------------------------------------------

class Server
{
public:
    Server();
    ~Server();

    Server( const Server & )            = delete;
    Server &operator=( const Server & ) = delete;

    Server( Server && ) noexcept;
    Server &operator=( Server && ) noexcept;

    [[nodiscard]] bool init( const ServerInitParams &params );
    void               shutdown();

    // `active` = a map is loaded; `initialized` = a server session has
    // begun (SV_SpawnServer sets it early — see the boundary Interface
    // table for the exact timing quirk).
    // TODO(chunk6-S7): drive these from the real lifecycle FSM.
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] const ServerStats &stats() const noexcept;

    // TODO(chunk6-S7): exec_load_level / exec_load_game / exec_change_level
    //                  (MapLoader GameState FSM entry points), shutdown_game.
    // TODO(chunk6-S8): frame() — Host_ServerFrame order incl. the
    //                  zero-physics-frames early-return quirk.
    // TODO(chunk6-S6): engine-internal trace/query surface (SV_Move,
    //                  SV_PointContents, SV_LinkEdict, lightstyles).

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::server
