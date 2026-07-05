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

#include <xash3dpp/abi/pm_movevars.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/private/server/game_dll.hpp>
#include <xash3dpp/private/server/lightstyles.hpp>
#include <xash3dpp/private/server/model_resolver.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/snapshot.hpp>
#include <xash3dpp/private/server/string_pool.hpp>
#include <xash3dpp/private/server/world_hooks.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::cmd_cvar { class CmdCvarContext; }
namespace xash::filesystem { class Filesystem; }
namespace xash { class MapLoader; }
namespace xash::networking { class NetworkContext; }

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

    // SV_InitGame → SV_LoadProgs dll path (COM_GetCommonLibraryPath); the
    // spawn path loads it lazily on the first SV_SpawnServer.  @lifetime: caller
    const char *game_dll = "";

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

    // Per-frame simulation state (S8; sv.framecount/hostflags/paused/…).
    std::uint32_t framecount = 0;    // sv.framecount (SV_Physics increments)
    int           hostflags  = 0;    // sv.hostflags (SVF_* snapshot bits)
    bool          paused      = false; // sv.paused (listen-server freeze)
    bool          playersonly = false; // sv.playersonly (sv_playersonly cvar)
    bool          simulating  = false; // sv.simulating (SV_IsSimulating cache)

    // TODO(chunk6-S7b): worldmodel/models cache, sizebufs, consistency +
    // resource lists, instanced baselines, lightstyle mirrors.
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

// legacy sv_pushed_t (server.h:277-283): one saved pusher/pushed state on
// the SV_PushMove/SV_PushRotate rollback stack (svgame.pushed[256]).
struct PushedEnt
{
    ::xash::abi::edict_t *ent = nullptr;
    ::xash::utilities::Vec3 origin{};
    ::xash::utilities::Vec3 angles{};
    int fixangle = 0;
};

// The aggregate replacing the legacy sv/svs/svgame triple.
struct ServerRuntime
{
    ServerConfig cfg;

    // Injected dependencies (Q-4). @lifetime: engine
    ::xash::cmd_cvar::CmdCvarContext *cvars = nullptr; // optional pre-S7b
    ::xash::filesystem::Filesystem   *fs    = nullptr; // required by load_progs
    ::xash::MapLoader                *maps  = nullptr; // S7b spawn path
    ::xash::networking::NetworkContext *net = nullptr; // S9 packet I/O (read_packets)

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

    // svgame.movevars / oldmovevars (S8): SV_UpdateMovevars mirrors the sv_*
    // cvars here; the pmove bridge and delta layer read them.
    ::xash::abi::movevars_t movevars{};
    ::xash::abi::movevars_t oldmovevars{};

    // sv.lightstyles (S8 animates them in SV_RunLightStyles); the pfnLightStyle
    // slot writes through the bridge pointer install_world_bridge wires.
    LightStyles lightstyles;

    // svgame.pushed[256] — the pusher rollback stack (SV_PushMove/PushRotate).
    PushedEnt pushed[::xash::limits::server_pushed_ents];

    EngineBridge   bridge;
    PrecacheTables precache;  // per-level content; cleared each spawn

    // World-interaction instances rebound every SV_SpawnServer (install_world_
    // bridge) and unbound on deactivate.  The world itself is owned by
    // MapLoader (Q-6); these hold borrowed pointers into it + the areanode
    // tree the clip/link walks traverse.
    ModelResolver  models;
    WorldLinks     links;
    GameWorldHooks hooks;
    MoveEnv        move_env;
    LinkEnv        link_env;

    LevelState      level;
    PersistentState persistent;

    // S9 — svs.clients array, svgame.msg[] registry, sv.multicast scratch,
    // ban filters, server log.  One aggregate keeps the S9 surface out of
    // LevelState/PersistentState (clean merge boundary with the S8 slice).
    ClientMachinery clients;

    // S9 completion — svs.baselines + sv.instanced + the packet_entities ring
    // + per-client frames rings (snapshot.hpp).
    SnapshotState snapshot;

    // sv.signon (server.h:169): the reliable signon message every connecting
    // client replays — baselines (now), precache lists / user messages (later
    // slices).  Pool-allocated once (snapshot_alloc_signon), the MessageBuf
    // rebinds it; reset each spawn; the buffer is freed in snapshot_shutdown.
    std::byte                     *signon_buf = nullptr; // [k_max_init_msg]
    ::xash::networking::MessageBuf signon;

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

// --- entity-string parse (sv_game.c:4885-5169) ------------------------------
// SpawnServer (S7c) seeds the world model + precache and installs the world
// interaction bridge; these three drive the entity lump through the game DLL.
// They are free functions over ServerRuntime + the loaded world (so the
// parse path is exercised without the full SpawnServer orchestration).

// SV_ParseEdict: pull one { ... } dictionary from `cursor` (advanced in
// place) into `ent`, applying the classname-first / angle→angles / custom-
// entity / trailing-space quirks.  false = inhibited (no classname, or
// AllocPrivateData rejected the edict); true = spawned private data live.
[[nodiscard]] bool parse_edict( ServerRuntime &rt,
                                const ::xash::map_loader::WorldData &world,
                                const char *&cursor,
                                ::xash::abi::edict_t *ent ) noexcept;

// SV_LoadFromFile: the '{'-delimited entity loop — world edict is slot 0
// (already initialised), the rest are SV_AllocEdict; pfnSpawn == -1 without
// FL_KILLME frees + counts the entity as inhibited; world origin/angles are
// cleared afterwards.
void load_from_file( ServerRuntime &rt,
                     const ::xash::map_loader::WorldData &world,
                     const char *entities ) noexcept;

// SV_SpawnEntities: reset sky/water cvars, stamp the world edict
// (model/modelindex/solid/movetype) + globals (maxEntities/mapname/
// startspot/time), then SV_LoadFromFile over the world's entity lump.
void spawn_entities( ServerRuntime &rt,
                     const ::xash::map_loader::WorldData &world ) noexcept;

// --- level orchestration (sv_init.c) ----------------------------------------

// SV_SetupClients (sv_init.c:790-834): latch svs.maxclients from the
// sv_maxclients cvar (act only on a real change), clamp (dedicated
// bound(4,·,MAX_CLIENTS) / listen bound(1,·,MAX_CLIENTS)), deathmatch/coop
// consistency, the maxplayers FCVAR_LATCH feedback, and the arena reserved/
// num_entities floor (maxclients + 1).
void setup_clients( ServerRuntime &rt ) noexcept;

// SV_SpawnServer (sv_init.c:935-1078): setup_clients → ensure progs loaded →
// reset the per-level state → ss_loading → world load through MapLoader +
// submodel precache → client-slot SV_InitEdict → install the world-interaction
// bridge (SV_ClearWorld).  Does NOT run the entity lump — the caller drives
// spawn_entities next (exec_load_level).  Returns false on a load failure
// (routed through the host-error hook, Q-5).
[[nodiscard]] bool spawn_server( ServerRuntime &rt, const char *mapname,
                                 const char *startspot, bool background ) noexcept;

// SV_ActivateServer (sv_init.c:579-673): SV_FreeOldEntities, pfnServerActivate,
// string pool → dynamic (AFTER activate), the settle frames (SP 2 / MP 8 @
// SV_SPAWN_TIME; the restore path is a single 0.001 frame), ss_active.
// run_physics=false is the save-restore path.
void activate_server( ServerRuntime &rt, bool run_physics ) noexcept;

} // namespace xash::server
