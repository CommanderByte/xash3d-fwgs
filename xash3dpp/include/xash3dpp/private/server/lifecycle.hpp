#pragma once
// xash3dpp — server lifecycle runtime: the sv/svs/svgame aggregate and the
// game-DLL load/unload orchestration (Chunk 6 S7)
// Legacy reference: engine/server/sv_init.c (server_t sv, server_static_t
// svs — :24-28), sv_game.c SV_LoadProgs (:5214) / SV_UnloadProgs (:5171).
// Deep dives: docs/legacy-survey/deep-dive-server-lifecycle.md,
//             deep-dive-server-game-dll-bridge.md.
//
// Q-2 (no globals): the legacy `sv`/`svs`/`svgame` file-scope triple
// becomes ServerRuntime, owned by Server::Impl; lifecycle steps are free
// functions over it (mirroring the legacy sv_init.c/sv_game.c split).
// The ONE deliberate exception is the engine-bridge installation
// (install_engine_bridge) — the 159 C slots cannot capture state, exactly
// like legacy's svgame reach-through (see engine_bridge.hpp).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/private/server/game_dll.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/string_pool.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::cmd_cvar { class CmdCvarContext; }
namespace xash::filesystem { class Filesystem; }
namespace xash { class MapLoader; }

namespace xash::server {

// sv.state — engine-internal, but the integer VALUES are behavioural
// contract: they are mirrored verbatim into the read-only cvar
// `host_serverstate` (sv_init.c:35-39; server.h ss_dead/ss_loading/
// ss_active enum order).
enum class ServerState : int
{
    Dead    = 0,
    Loading = 1,
    Active  = 2,
};

// Static configuration injected once (Server::init / test fixtures).
struct ServerConfig
{
    const char *game_dir = "";          // GI->gamefolder  @lifetime: caller
    std::size_t max_edicts = 900;       // GI->max_edicts (gameinfo default;
                                        // S7b wires the real gameinfo value)
    bool dedicated    = true;
    int  developer    = 0;
    bool peoei_broken = false;          // BUGCOMP_PENTITYOFENTINDEX

    // Q-5 host error surface (see engine_bridge.hpp).
    HostErrorHook host_error     = nullptr;
    void         *host_error_ctx = nullptr;
};

// legacy server_t (subset — fields land with the slice that uses them;
// wiped every SV_SpawnServer).
struct LevelState
{
    ServerState state = ServerState::Dead;

    bool background = false;
    bool loadgame   = false;

    double time          = 0.0;  // spawn epoch is 1.0 (sv_init.c:983)
    double time_residual = 0.0;
    float  frametime     = 0.0f;

    char name[64]      = {};  // stripped map name
    char startspot[64] = {};

    std::uint32_t worldmap_crc = 0;
    int           progs_crc    = 0;  // Quake-compat progs.dat CRC

    // TODO(chunk6-S7b): worldmodel/models cache, sizebufs, consistency +
    // resource lists, instanced baselines, lightstyle mirrors.
    // TODO(chunk6-S8): hostflags, paused/simulating, playersonly.
};

// legacy server_static_t (subset — persists across map changes).
struct PersistentState
{
    bool initialized = false;

    // Zero until the first SV_SetupClients pass (S7b) — legacy loads the
    // game DLL at engine start with svs.maxclients still 0, so the edict
    // floor right after load_progs is 1 (world only).
    int maxclients = 0;
    int spawncount = 0;

    std::uint32_t challenge_salt[16] = {};
    double        timestart          = 0.0;

    // TODO(chunk6-S9): client array, snapshot ring, baselines pointers,
    // serverinfo/localinfo, testpacket, log state.
};

// The aggregate replacing the legacy sv/svs/svgame triple.
struct ServerRuntime
{
    ServerConfig cfg;

    // Injected dependencies (Q-4). @lifetime: engine
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // optional pre-S7b
    ::xash::filesystem::Filesystem   *fs    = nullptr; // required by load_progs
    ::xash::MapLoader                *maps  = nullptr; // S7b spawn path

    // Owned game binding (legacy svgame equivalents).
    ::xash::memory::PoolHandle game_pool;   // svgame.mempool
    GameDll                    game;
    EdictArena                 arena;
    StringPool                 strings;
    ::xash::abi::globalvars_t  globals{};
    ::xash::abi::enginefuncs_t engine_table{}; // handed to GiveFnptrsToDll;
                                               // must outlive the DLL (mods
                                               // keep the received pointer)
    ::xash::networking::DeltaTables     delta;
    ::xash::map_loader::HullBoundsTable hull_bounds{}; // pfnGetHullBounds ×4

    EngineBridge   bridge;
    PrecacheTables precache;  // per-level content; cleared each spawn

    LevelState      level;
    PersistentState persistent;

    bool game_loaded = false;

    // pfnGameInit has run — gates pfnGameShutdown at unload so the Q-5
    // OOM-unwind path (unreachable in legacy, which Host_Errors instead)
    // never delivers an unpaired Shutdown to the game.
    bool game_initialized = false;
};

// SV_LoadProgs: full legacy order — pool, bridge install, table build,
// handshake (GameDll::load), operator commands, string pool, globals,
// edict arena, host_gameloaded, GameInit, hull bounds, Delta_Init,
// RegisterEncoders.  Idempotent (early-returns true when loaded).
[[nodiscard]] bool load_progs( ServerRuntime &rt, const char *dll_path ) noexcept;

// SV_UnloadProgs: deactivate → delta shutdown → cvar prepare-to-unlink →
// pfnGameShutdown → host_gameloaded 0 → kill operator commands → unlink
// cvars/commands → free string pool → free library → free pool.
void unload_progs( ServerRuntime &rt ) noexcept;

// SV_DeactivateServer.  Quirk kept: the disconnect-cfg execs are queued
// BEFORE the initialized/dead guard (sv_init.c:685-694).
void deactivate_server( ServerRuntime &rt ) noexcept;

// Host_SetServerState: mirrors the value into the read-only cvar
// `host_serverstate` and updates the precache loading gate.
void set_server_state( ServerRuntime &rt, ServerState state ) noexcept;

} // namespace xash::server
