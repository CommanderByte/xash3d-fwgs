// xash3dpp — server snapshot / baseline pipeline (Chunk 6, S9 completion)
// Legacy reference: engine/server/sv_init.c:459 SV_CreateBaseline (fill half),
// engine/server/sv_game.c:4425 pfnCreateInstancedBaseline.
// Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §5.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/snapshot.hpp>

#include <xash3dpp/abi/server_consts.hpp>          // k_fl_customentity
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include <cstring>

namespace xash::server {

namespace {

// DEFAULT_PLAYER_PATH_HALFLIFE (sv_init.c:471).  The ENGINE_QUAKE_COMPATIBLE
// variant (models/player.mdl vs the Quake path) is a host-feature branch left
// for the compat policy; HL is the milestone target.
constexpr const char *k_default_player_model = "models/player.mdl";

// pfnCreateBaseline's `player` flag == the legacy DELTA_PLAYER / DELTA_ENTITY
// selector (1 picks DT_ENTITY_STATE_PLAYER_T).
[[nodiscard]] bool is_player_slot( std::size_t entnum, int maxclients ) noexcept
{
    return entnum != 0 && static_cast<int>( entnum ) <= maxclients;
}

} // namespace

bool snapshot_alloc_baselines( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const int count = rt.cfg.max_edicts;
    if ( count <= 0 )
        return false;
    if ( rt.snapshot.baselines != nullptr && rt.snapshot.baseline_count == count )
        return true; // idempotent (legacy allocs once per LoadProgs)

    void *mem = ::xash::memory::mem_calloc(
        rt.game_pool,
        sizeof( ::xash::abi::entity_state_t ) * static_cast<std::size_t>( count ) );
    if ( mem == nullptr )
        return false;

    rt.snapshot.baselines      = static_cast<::xash::abi::entity_state_t *>( mem );
    rt.snapshot.baseline_count = count;
    return true;
}

void snapshot_reset( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.snapshot.baselines != nullptr )
        std::memset( rt.snapshot.baselines, 0,
                     sizeof( ::xash::abi::entity_state_t ) *
                         static_cast<std::size_t>( rt.snapshot.baseline_count ) );

    rt.snapshot.num_instanced       = 0;
    rt.snapshot.last_valid_baseline = 0;
    for ( InstancedBaseline &ib : rt.snapshot.instanced )
        ib = InstancedBaseline{};
}

void snapshot_free_baselines( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.snapshot.baselines != nullptr )
        ::xash::memory::mem_free( rt.snapshot.baselines );
    rt.snapshot = SnapshotState{};
}

int create_instanced_baseline( SnapshotState &snap, ::xash::abi::string_t classname,
                               const ::xash::abi::entity_state_t *baseline ) noexcept
{
    // Legacy silently ignores registrations past MAX_CUSTOM_BASELINES.
    if ( baseline == nullptr || snap.num_instanced >= k_max_custom_baselines )
        return snap.num_instanced;

    const int index = snap.num_instanced;
    snap.instanced[index].classname = classname;
    snap.instanced[index].baseline  = *baseline;
    ++snap.num_instanced;
    return index;
}

void create_baselines( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.snapshot.baselines == nullptr )
        return;

    // XASH3DPP-STUB(chunk6): SV_WriteVoiceCodec into sv.signon (SP voice /
    // MP) — OQ-8 voice trim.

    // SV_ModelIndex(DEFAULT_PLAYER_PATH_HALFLIFE) — registers if absent, so the
    // player baseline carries the standard model even on maps that never
    // precache it explicitly (sv_init.c:470-471).
    const int playermodel = rt.precache.model_index( k_default_player_model );

    // host.player_mins[0]/maxs[0] — the standing hull, from the game DLL's
    // pfnGetHullBounds (queried at load into rt.hull_bounds).  Vec3 is three
    // contiguous floats, so a copy into the vec3_t the ABI expects is exact.
    float pmins[3];
    float pmaxs[3];
    std::memcpy( pmins, &rt.hull_bounds[0].mins, sizeof( pmins ) );
    std::memcpy( pmaxs, &rt.hull_bounds[0].maxs, sizeof( pmaxs ) );

    const int         maxclients = rt.clients.maxclients;
    const std::size_t num        = rt.arena.num_entities();

    for ( std::size_t entnum = 0; entnum < num; ++entnum )
    {
        ::xash::abi::edict_t *ed = rt.arena.edict_num( entnum );
        if ( ed == nullptr )
            continue;
        const EntityView view( ed );
        if ( view.freed() )
            continue; // SV_IsValidEdict

        const bool player = is_player_slot( entnum, maxclients );
        if ( !player && view.modelindex() == 0 )
            continue; // invisible non-player entity

        ::xash::abi::entity_state_t *base = &rt.snapshot.baselines[entnum];
        base->number     = static_cast<int>( entnum );
        base->entityType = ( view.flags() & ::xash::abi::k_fl_customentity )
                               ? ::xash::abi::k_entity_beam
                               : ::xash::abi::k_entity_normal;

        // The game DLL fills the state — there is no engine-side
        // SV_FillEntityState (deep dive §5).
        if ( rt.game.funcs().pfnCreateBaseline != nullptr )
            rt.game.funcs().pfnCreateBaseline( player ? 1 : 0,
                                               static_cast<int>( entnum ), base, ed,
                                               playermodel, pmins, pmaxs );

        rt.snapshot.last_valid_baseline = static_cast<int>( entnum );
    }

    // The DLL registers its instanced baselines here; each call reaches back
    // into pfn_create_instanced_baseline (→ create_instanced_baseline).
    if ( rt.game.funcs().pfnCreateInstancedBaselines != nullptr )
        rt.game.funcs().pfnCreateInstancedBaselines();

    // XASH3DPP-STUB(chunk6): the signon-write half (sv_init.c:510-544 —
    // MSG_WriteDeltaEntity of every baseline + the instanced list into
    // sv.signon) lands with the signon buffer in the send sub-slice.
}

} // namespace xash::server
