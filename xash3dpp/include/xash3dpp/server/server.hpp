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

#include <xash3dpp/map_loader/map_loader.hpp> // ILevelChangeExecutor base

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace xash::cmd_cvar { class CmdCvarContext; }
namespace xash::filesystem { class Filesystem; }
namespace xash::networking { class NetworkContext; }

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
    // Injected dependencies (Q-4) — non-owning, must outlive the Server.
    // A default-constructed params (all null) yields an inert server: init()
    // succeeds but no spawn is possible until the deps are wired (the scaffold
    // lifecycle test relies on this).
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // @lifetime: engine
    ::xash::filesystem::Filesystem   *fs    = nullptr; // @lifetime: engine
    ::xash::MapLoader                *maps  = nullptr; // @lifetime: engine

    // Networking (Q-4) — the host owns the single NetworkContext + its UDP
    // sockets (EngineContext); the server holds this non-owning handle and
    // pulls its server-socket datagrams each frame (read_packets).  nullptr →
    // an offline server (spawn/physics run; no packet I/O — test fixtures).
    ::xash::networking::NetworkContext *net = nullptr; // @lifetime: engine

    // GoldSrc installs these exact no-capture addresses in both enginefuncs
    // and playermove_t. nullptr selects one shared inert callback pair.
    int ( *random_long )( int low, int high ) = nullptr;
    float ( *random_float )( float low, float high ) = nullptr;

    // SV_InitGame → SV_LoadProgs dll path + the active game folder.
    const char *game_dll = "";  // @lifetime: engine
    const char *game_dir = "";  // @lifetime: engine

    std::size_t max_edicts = 0; // 0 → the ServerConfig gameinfo default
    bool        dedicated  = true;
    int         developer  = 0;
    bool        peoei_broken = false; // BUGCOMP_PENTITYOFENTINDEX

    // Q-5 host error surface (installed by the host layer; nullptr →
    // core::log_error fallback).  Same signature as the private HostErrorHook.
    void ( *host_error )( void *ctx, const char *msg ) = nullptr;
    void  *host_error_ctx                              = nullptr; // @lifetime: caller-owned — the host_error callback context (installed by the host layer, not copied)

    // TODO(chunk6-S9): ITrustOracle seam (cmd_cvar D2), host feature flags +
    // ICompatPolicy (Q-12), the frame-rate gate (host OQ-11).
};

// ---------------------------------------------------------------------------
// Server (pimpl)
// ---------------------------------------------------------------------------

// @thread-safety: main-thread engine state. Server and every entry point
// (init/shutdown, frame(), the exec_* level-change hooks, active()/initialized(),
// and the free-function-over-ServerRuntime workers behind the private headers)
// run on ThreadRole::Main and assert it (T_Main; QN). The game DLL is only ever
// called from T_Main. The sole cross-thread surface is stats(): the ServerStats
// Tier-1 counters are std::atomic and readable from any thread (debug-stats seam).
class Server final : public ::xash::ILevelChangeExecutor
{
public:
    Server();
    ~Server() override;

    Server( const Server & )            = delete;
    Server &operator=( const Server & ) = delete;

    Server( Server && ) noexcept;
    Server &operator=( Server && ) noexcept;

    [[nodiscard]] bool init( const ServerInitParams &params );
    void               shutdown();

    // `active` = a map is loaded and activated (ss_active); `initialized` = a
    // server session has begun (SV_SpawnServer sets svs.initialized early —
    // the boundary Interface-table timing quirk).
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] const ServerStats &stats() const noexcept;

    // ILevelChangeExecutor — the MapLoader FSM drives these (registered via
    // MapLoader::set_level_executor).  exec_load_level runs the full
    // SV_SpawnServer → spawn_entities → SV_ActivateServer chain; the save
    // paths are Chunk 8 stubs behind the seam.
    [[nodiscard]] bool exec_load_level( std::string_view map,
                                        bool background ) noexcept override;
    [[nodiscard]] bool exec_load_game( std::string_view map ) noexcept override;
    [[nodiscard]] bool exec_change_level( std::string_view map,
                                          std::string_view landmark,
                                          bool background ) noexcept override;

    // Host_ServerFrame (S8): the per-host-frame server tick — movevars
    // refresh → fixed-step physics → prep-world-frame.  `host_frametime` is
    // the elapsed host wall-clock delta.  Preserves the zero-physics-frames
    // early-return quirk (see host_server_frame).  The client/networking
    // ingress + snapshot-send steps are S9 seams.
    void frame( double host_frametime ) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::server
