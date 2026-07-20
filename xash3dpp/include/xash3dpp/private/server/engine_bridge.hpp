#pragma once
// xash3dpp — engine bridge: the state behind the enginefuncs_t table
// Legacy reference: engine/server/sv_game.c — the file-scope svgame
// aggregate (server.h :301-337) that every gEngfuncs slot reaches for,
// and the gEngfuncs table itself (:4705-4866).
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md §2/§6.
//
// The 159 slots are plain C function pointers — they cannot capture
// state, so (exactly like legacy) the implementations reach a file-scope
// bridge installed before the table is built.  Lifecycle (S7) owns the
// bridge instance and keeps its pointers current across map changes;
// S6-era tests install a fixture bridge directly.
//
// Pointers may be null before their owning slice wires them (world
// interaction until a map is loaded, game until the DLL is up); every
// slot degrades to its documented legacy-safe default in that case.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/abi/pm_defs.hpp> // playermove_t (pmove bridge, P3b)
#include <xash3dpp/map_loader/phs.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/game_dll.hpp>
#include <xash3dpp/private/server/lightstyles.hpp>
#include <xash3dpp/private/server/precache.hpp>
#include <xash3dpp/private/server/string_pool.hpp>
#include <xash3dpp/world/links.hpp>
#include <xash3dpp/world/trace.hpp>

namespace xash::networking {
class DeltaTables; // S9 — usercmd/event/entity_state delta tables (delta.hpp)
}

namespace xash::server {

struct ServerRuntime;   // lifecycle.hpp — the full sv/svs/svgame aggregate
struct ClientMachinery; // S9 — svs.clients + svgame.msg + sv.multicast (clients.hpp)
struct SnapshotState;   // S9 — svs.baselines + sv.instanced (snapshot.hpp)

// Q-5: the legacy Host_Error surface — installed by the host layer;
// slots that legacy hard-errors from (edict exhaustion, bad WriteEntity)
// route here.  The hook may not return control flow guarantees; slot
// implementations still return a safe value afterwards.
using HostErrorHook = void ( * )( void *ctx, const char *msg );
using RandomLongHook = int ( * )( int low, int high );
using RandomFloatHook = float ( * )( float low, float high );

struct EngineBridge
{
    // S4 stores
    EdictArena *arena   = nullptr;                  // @lifetime: engine
    StringPool *strings = nullptr;                  // @lifetime: engine
    ::xash::abi::globalvars_t *globals = nullptr;   // @lifetime: engine
    GameDll    *game    = nullptr;                  // @lifetime: engine

    // S5 world interaction (null until lifecycle loads a map)
    ::xash::world::MoveEnv    *move_env = nullptr;  // @lifetime: engine
    ::xash::world::WorldLinks *links    = nullptr;  // @lifetime: engine
    ::xash::world::LinkEnv    *link_env = nullptr;  // @lifetime: engine
    LightStyles *lightstyles = nullptr;             // @lifetime: engine
    const ::xash::map_loader::PhsTable *phs = nullptr; // @lifetime: engine

    // S7 lifecycle (null in pre-lifecycle fixtures: the precache slots
    // then return 0, the legacy no-server answer)
    PrecacheTables *precache = nullptr;             // @lifetime: engine

    // The owning runtime — only the rare full-orchestration slots that must run
    // a whole server operation reach it (pfnRunPlayerMove drives SV_RunCmd).
    // Wired in load_progs; null in pre-lifecycle fixtures (those slots no-op).
    ServerRuntime *runtime = nullptr;              // @lifetime: engine

    // pmove bridge (P3b): the single player-move working set the PM_* trace
    // callbacks reach, plus the pfnGetHullBounds player-hull table they index.
    // Both wired by SV_InitClientMove (load_progs); null before then, so the
    // pmove callbacks degrade to their clear-trace / no-op defaults.
    ::xash::abi::playermove_t                 *pmove         = nullptr; // @lifetime: engine
    const ::xash::map_loader::HullBoundsTable *player_bounds = nullptr; // @lifetime: engine

    // S9 clients/messaging (null in pre-S9 fixtures: RegUserMsg / MessageBegin
    // / Write* / GetPlayerUserId then degrade to their legacy no-server value)
    ClientMachinery *clients = nullptr;             // @lifetime: engine

    // S9 snapshot pipeline — svs.baselines + sv.instanced.  pfnCreateInstanced-
    // Baseline reaches this to append; null in pre-lifecycle fixtures.
    SnapshotState *snapshot = nullptr;              // @lifetime: engine

    // S9 delta tables (usercmd/event/entity_state).  SV_PlaybackReliableEvent
    // null-compresses event args through this; null until load_progs wires it.
    ::xash::networking::DeltaTables *delta = nullptr; // @lifetime: engine

    // Misc allocations the ABI forces on the engine (cvar string
    // replacements); typically the svgame mempool equivalent.
    ::xash::memory::PoolHandle misc_pool;

    // Host state mirrored for the slots.
    // INVARIANT: every mirrored scalar below must be re-stamped wherever its
    // source changes. A mirror that is read but never written is invisible to
    // stub_scan (no marker) and to compliance_scan (no rule) — `sv_time` was
    // exactly that from Chunk 6 until the 2026-07-20 modernization audit, which
    // left legacy's 0.5 s edict slot-reuse grace permanently inoperative on the
    // game-DLL path (see below).
    //
    // sv.time — drives EdictArena's reuse grace (sv_game.c:1051) and
    // SV_SetLightStyle's change stamp. Re-stamped at the four sites where
    // `rt.level.time` moves: physics.cpp (both frame-advance paths),
    // spawn.cpp (spawn epoch), save_bridge.cpp (loadgame header time).
    double      sv_time     = 0.0;  // sv.time (edict freetime/reuse)
    int         max_clients = 0;    // svs.maxclients
    int         developer   = 0;    // host_developer (AlertMessage gates)
    int         server_state = 0;   // sv.state mirror (ServerState int) — the
                                    // SV_SetModel ss_active guard reads it
    bool        dedicated   = true;
    bool        merge_visibility = false; // SVF_MERGE_VISIBILITY (portal pass)
    bool        novis       = false;      // sv_novis
    const char *game_dir    = "";         // GI->gamefolder  @lifetime: engine

    // One process-owned legacy stream, injected from EngineContext. These
    // addresses are installed verbatim into enginefuncs and playermove_t.
    RandomLongHook  random_long  = nullptr;
    RandomFloatHook random_float = nullptr;

    // pfnSetGroupMask mirror (svs.groupmask/groupop); also pushed into
    // move_env/links when present.
    int group_mask = 0;
    int group_op   = 0;

    // pfnGetAimVector: legacy seeds bestdist from sv_aim.value only when
    // sv_allow_autoaim is set, else 0 (autoaim disabled).  Lifecycle
    // wires the cvars (S7); 0 keeps autoaim off.
    float autoaim_threshold = 0.0f;

    // pfnCVarRegister / pfnCvar_RegisterVariable chain: the game's own
    // cvar_t structs, linked through THEIR next pointers (the struct is
    // the storage — game DLLs read .value/.string directly).
    // TODO(chunk6-S7): unify with the cmd_cvar engine registry.
    ::xash::abi::cvar_t *external_cvars = nullptr;  // @lifetime: game DLL

    // Engine-owned replacement strings from pfnCVarSetFloat/SetString
    // (opaque chain inside the allocations; legacy leaks these into the
    // svgame mempool and bulk-frees at unload — misc_pool asserts on
    // leaks instead, so reset_external_cvars() must run at teardown).
    void *cvar_string_allocs = nullptr; // @lifetime: misc_pool-owned — the CvarStringNode chain lives in the server misc_pool, freed by reset_external_cvars() at teardown

    HostErrorHook host_error     = nullptr;
    void         *host_error_ctx = nullptr; // @lifetime: caller-owned — the host_error hook's context (installed with the bridge, not copied)
};

// Install the bridge the table implementations reach (legacy svgame).
// Passing nullptr detaches (tests).  @lifetime: engine — the pointer is
// kept, not copied.
void install_engine_bridge( EngineBridge *bridge ) noexcept;
[[nodiscard]] EngineBridge *engine_bridge() noexcept;

// Return the injected callbacks, or the one stateless inert pair when no
// EngineContext-backed stream is available. Both table builders call these so
// their stored function pointers are literally identical.
[[nodiscard]] RandomLongHook effective_random_long() noexcept;
[[nodiscard]] RandomFloatHook effective_random_float() noexcept;

// Build a fresh table copy (legacy gpEngfuncs local-copy semantics: the
// caller's copy goes to the DLL so "bots.dll etc. can't corrupt the
// master table").  `peoei_broken` applies the BUGCOMP_PENTITYOFENTINDEX
// patch (sv_game.c:5250-5251) — pfnPEntityOfEntIndex gets the broken
// GoldSrc player-range variant.  Every slot is populated; pre-wiring
// milestone slots are XASH3DPP-STUB(chunk6)-marked no-ops.
[[nodiscard]] ::xash::abi::enginefuncs_t
build_engine_table( bool peoei_broken ) noexcept;

// SV_AllocPrivateData (sv_game.c:1092): re-init/alloc the edict, stamp its
// classname, resolve the LINK_ENTITY spawn export by raw name and run it.
// When `customentity` is non-null it is set true if the classname had no
// export and the "custom" fallback export was used instead (the Xash
// extension parse path relies on this out-param).  Exposed from the ABI
// shim so the lifecycle entity-parse path reuses the one LINK dispatch.
[[nodiscard]] ::xash::abi::edict_t *
alloc_private_data( ::xash::abi::edict_t *ent, ::xash::abi::string_t className,
                    bool *customentity ) noexcept;

// SV_UnloadProgs counterpart for the cvar chain: unlink the game's
// cvar_t structs and free every engine-owned replacement string (their
// .string pointers dangle afterwards, exactly like legacy post-unload).
// Must run before the misc_pool is destroyed.
void reset_external_cvars( EngineBridge &bridge ) noexcept;

} // namespace xash::server
