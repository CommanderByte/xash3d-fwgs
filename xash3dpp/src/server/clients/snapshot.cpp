// xash3dpp — server snapshot / baseline pipeline (Chunk 6, S9 completion)
// Legacy reference: engine/server/sv_init.c:459 SV_CreateBaseline (fill half),
// engine/server/sv_game.c:4425 pfnCreateInstancedBaseline, engine/server/
// sv_frame.c — SV_WriteEntitiesToClient (:613), SV_AddEntitiesToPacket (:58),
// SV_EmitPacketEntities (:235), SV_FindBestBaseline (:184), SV_EntityNumbers
// (:35).  Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §5.
//
// The byte-exact entity_state_t delta codec lives in networking (rt.delta);
// this module only orchestrates baselines, the per-client visible-entity gather
// (through the game DLL's pfnSetupVisibility + pfnAddToFullPack hooks), the
// shared circular packet-entity ring, and the per-client frames ring.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/snapshot.hpp>

#include <xash3dpp/abi/server_consts.hpp>          // k_fl_customentity, k_ef_*
#include <xash3dpp/cmd_cvar/context.hpp>           // sv_instancedbaseline read
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/networking/networking.hpp>       // NetworkContext::send_packet
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>

#include <array>
#include <cstdlib>  // std::qsort
#include <cstring>
#include <span>

namespace xash::server {

namespace net = ::xash::networking;

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

// CHECKVISBIT / SETVISBIT over the per-edict dedup mask (world.h).
[[nodiscard]] bool visbit_get( const std::uint8_t *bits, std::size_t i ) noexcept
{
    return ( bits[i >> 3] & ( 1u << ( i & 7 ))) != 0;
}
void visbit_set( std::uint8_t *bits, std::size_t i ) noexcept
{
    bits[i >> 3] |= static_cast<std::uint8_t>( 1u << ( i & 7 ));
}

// SV_EntityNumbers (sv_frame.c:35): qsort by entity number; the equal case is
// kept explicit (the "watcom libc compares ents with itself" guard).
int entity_numbers_cmp( const void *a, const void *b ) noexcept
{
    const int e1 = static_cast<const ::xash::abi::entity_state_t *>( a )->number;
    const int e2 = static_cast<const ::xash::abi::entity_state_t *>( b )->number;
    if ( e1 == e2 )
        return 0;
    return e1 < e2 ? -1 : 1;
}

[[nodiscard]] bool instancedbaseline_enabled( const ServerRuntime &rt ) noexcept
{
    return rt.cvars != nullptr &&
           rt.cvars->cvar_variable_value( "sv_instancedbaseline" ) != 0.0f;
}

// SV_ClassName( SV_EdictNum( num )) — the string used to match an entity to an
// instanced baseline (sv_frame.c:326).
[[nodiscard]] const char *classname_of( ServerRuntime &rt, int num ) noexcept
{
    ::xash::abi::edict_t *ed = rt.arena.edict_num( static_cast<std::size_t>( num ));
    if ( ed == nullptr )
        return "";
    return rt.strings.get_string( EntityView( ed ).classname() );
}

// Free the ring + gather scratch + every client's frames ring (shared by the
// setup_clients realloc path and snapshot_shutdown).
void free_rings( ServerRuntime &rt ) noexcept
{
    if ( rt.snapshot.packet_entities != nullptr )
    {
        ::xash::memory::mem_free( rt.snapshot.packet_entities );
        rt.snapshot.packet_entities = nullptr;
    }
    if ( rt.snapshot.gather_ents != nullptr )
    {
        ::xash::memory::mem_free( rt.snapshot.gather_ents );
        rt.snapshot.gather_ents = nullptr;
    }
    for ( ServerClient &cl : rt.clients.clients )
    {
        if ( cl.frames != nullptr )
        {
            ::xash::memory::mem_free( cl.frames );
            cl.frames = nullptr;
        }
    }
    rt.snapshot.num_client_entities  = 0;
    rt.snapshot.next_client_entities = 0;
    rt.snapshot.ring_maxclients      = 0;
    rt.snapshot.update_backup        = 0;
    rt.snapshot.update_mask          = 0;
}

// SV_FindBestBaseline (sv_frame.c:184): scan up to MAX_CUSTOM_BASELINES-1
// already-emitted states in THIS frame's ring window for the delta costing the
// fewest bits (Delta_TestBaseline).  Returns the positive offset (index -
// bestfound) and repoints `*baseline` when a better one is found.
int find_best_baseline( ServerRuntime &rt, int index,
                        const ::xash::abi::entity_state_t **baseline,
                        const ::xash::abi::entity_state_t *to,
                        const ClientFrame &frame, bool player ) noexcept
{
    const double t   = rt.level.time;
    const int    ring = rt.snapshot.num_client_entities;

    int best      = rt.delta.test_baseline( *baseline, to, player, t );
    int bestfound = index;

    for ( int i = index - 1;
          best > 0 && i >= 0 && ( index - i ) < ( k_max_custom_baselines - 1 );
          --i )
    {
        const ::xash::abi::entity_state_t *test =
            &rt.snapshot.packet_entities[( frame.first_entity + i ) % ring];
        if ( to->entityType == test->entityType )
        {
            const int bits = rt.delta.test_baseline( test, to, player, t );
            if ( bits < best )
            {
                best      = bits;
                bestfound = i;
            }
        }
    }

    if ( index != bestfound )
        *baseline =
            &rt.snapshot.packet_entities[( frame.first_entity + bestfound ) % ring];
    return index - bestfound;
}

// SV_AddEntitiesToPacket (sv_frame.c:58): fill the gather scratch with the
// entities the game DLL accepts, honouring the portal dedup mask, EF_REQUEST_PHS
// mask switch, and EF_MERGE_VISIBILITY portal recursion.  `num` tracks the
// accepted count (capped at MAX_VISIBLE_PACKET-1, overflow silently discarded).
void add_entities_to_packet( ServerRuntime &rt, ::xash::abi::edict_t *view,
                             ::xash::abi::edict_t *client, int &num,
                             bool from_client ) noexcept
{
    if ( rt.level.state == ServerState::Dead )
        return;

    const int maxclients = rt.clients.maxclients;

    if ( from_client )
    {
        // FCL_LOCAL_WEAPONS drives SVF_SKIPLOCALHOST; weapon prediction is a
        // send-sub-slice seam, so no local weapons yet — clear the bit.
        rt.level.hostflags &= ~k_svf_skiplocalhost;
        // XASH3DPP-STUB(chunk6): cl->num_viewents = 0 (portal-camera reset).
    }

    unsigned char *pvs = nullptr;
    unsigned char *phs = nullptr;
    if ( rt.game.funcs().pfnSetupVisibility != nullptr )
        rt.game.funcs().pfnSetupVisibility( view, client, &pvs, &phs );
    const bool fullvis = ( pvs == nullptr );

    const std::size_t numents = rt.arena.num_entities();
    for ( std::size_t e = 1; e < numents; ++e ) // e=0 (world) never sent
    {
        if ( visbit_get( rt.snapshot.sended, e ))
            continue; // already added through a portal pass

        ::xash::abi::edict_t *ent = rt.arena.edict_num( e );
        if ( ent == nullptr )
            continue;

        const bool player = is_player_slot( e, maxclients );
        if ( player )
        {
            const ServerClient &pc = rt.clients.clients[e - 1];
            if ( pc.state != ClientState::Spawned )
                continue;
            if ( pc.hltv )
                continue; // HLTV proxies never appear in packs
        }

        unsigned char *pset =
            ( EntityView( ent ).effects() & ::xash::abi::k_ef_request_phs ) ? phs
                                                                            : pvs;

        ::xash::abi::entity_state_t *state = &rt.snapshot.gather_ents[num];

        // The game DLL does BOTH the vis test and the state fill (deep dive §5).
        if ( rt.game.funcs().pfnAddToFullPack != nullptr &&
             rt.game.funcs().pfnAddToFullPack( state, static_cast<int>( e ), ent,
                                               client, rt.level.hostflags,
                                               player ? 1 : 0, pset ))
        {
            visbit_set( rt.snapshot.sended, e );

            // XASH3DPP-STUB(chunk6): EF_MERGE_VISIBILITY aiment → cl->viewentity[]
            // portal-camera registration (consumed only by SV_Multicast's portal
            // vis — needs the per-client viewentity list, send sub-slice).

            if ( num < k_max_visible_packet - 1 )
                ++num; // accepted; else silently discarded (overflow counting)
        }

        if ( fullvis )
            continue; // portal recursion is pointless under fullvis

        if ( from_client &&
             ( EntityView( ent ).effects() & ::xash::abi::k_ef_merge_visibility ))
        {
            rt.level.hostflags |= k_svf_merge_visibility;
            rt.bridge.merge_visibility = true;
            add_entities_to_packet( rt, ent, client, num, false );
            rt.level.hostflags &= ~k_svf_merge_visibility;
            rt.bridge.merge_visibility = false;
        }
    }
}

// SV_EmitPacketEntities (sv_frame.c:235): the two-pointer old/new merge that
// writes the svc_(delta)packetentities body into `msg`.
void emit_packet_entities( ServerRuntime &rt, ServerClient &cl,
                           const ClientFrame &to, net::MessageBuf &msg ) noexcept
{
    const int    maxclients = rt.clients.maxclients;
    const double timebase   = rt.level.time;
    const int    max_edicts = static_cast<int>( rt.cfg.max_edicts );
    const int    ring       = rt.snapshot.num_client_entities;

    const ClientFrame *from   = nullptr;
    int                oldmax = 0;

    if ( cl.delta_sequence != -1 )
    {
        const ClientFrame *cand = &cl.frames[cl.delta_sequence & rt.snapshot.update_mask];

        // Staleness: the from-frame's window may have rolled off the ring.
        if ( cand->first_entity <=
             ( rt.snapshot.next_client_entities - rt.snapshot.num_client_entities ))
        {
            msg.write_byte( static_cast<std::uint8_t>( k_svc_packetentities ));
            msg.write_ubit_long( static_cast<std::uint32_t>( to.num_entities - 1 ),
                                 k_max_visible_packet_bits );
            from   = nullptr;
            oldmax = 0;
        }
        else
        {
            from   = cand;
            oldmax = cand->num_entities;
            msg.write_byte( static_cast<std::uint8_t>( k_svc_deltapacketentities ));
            msg.write_ubit_long( static_cast<std::uint32_t>( to.num_entities - 1 ),
                                 k_max_visible_packet_bits );
            msg.write_byte( static_cast<std::uint8_t>( cl.delta_sequence ));
        }
    }
    else
    {
        msg.write_byte( static_cast<std::uint8_t>( k_svc_packetentities ));
        msg.write_ubit_long( static_cast<std::uint32_t>( to.num_entities - 1 ),
                             k_max_visible_packet_bits );
    }

    int newindex = 0;
    int oldindex = 0;

    while ( newindex < to.num_entities || oldindex < oldmax )
    {
        const ::xash::abi::entity_state_t *newent = nullptr;
        const ::xash::abi::entity_state_t *oldent = nullptr;
        int  newnum, oldnum;
        bool player = false;

        if ( newindex >= to.num_entities )
        {
            newnum = k_max_entnumber;
        }
        else
        {
            newent = &rt.snapshot.packet_entities[( to.first_entity + newindex ) % ring];
            newnum = newent->number;
            player = ( newnum >= 1 && newnum <= maxclients );
        }

        if ( oldindex >= oldmax )
        {
            oldnum = k_max_entnumber;
        }
        else
        {
            oldent = &rt.snapshot.packet_entities[( from->first_entity + oldindex ) % ring];
            oldnum = oldent->number;
        }

        if ( newnum == oldnum )
        {
            // Unchanged → force=false emits nothing.
            net::WriteDeltaEntityParams p;
            p.force      = false;
            p.kind       = player ? net::DeltaEntityKind::Player
                                  : net::DeltaEntityKind::Entity;
            p.timebase   = timebase;
            p.max_edicts = max_edicts;
            ( void )rt.delta.write_delta_entity( msg, oldent, newent, p );
            ++oldindex;
            ++newindex;
            continue;
        }

        if ( newnum < oldnum )
        {
            const ::xash::abi::entity_state_t *baseline =
                &rt.snapshot.baselines[newnum];
            int offset = 0;

            if ( !instancedbaseline_enabled( rt ) || rt.snapshot.num_instanced == 0 ||
                 rt.snapshot.last_valid_baseline > newnum )
            {
                offset = find_best_baseline( rt, newindex, &baseline, newent, to,
                                             player );
            }
            else
            {
                const char *cls = classname_of( rt, newnum );
                for ( int i = 0; i < rt.snapshot.num_instanced; ++i )
                {
                    if ( std::strcmp(
                             cls, rt.strings.get_string(
                                      rt.snapshot.instanced[i].classname )) == 0 )
                    {
                        baseline = &rt.snapshot.instanced[i].baseline;
                        offset   = -i - 1; // to avoid zero offset
                        break;
                    }
                }
            }

            net::WriteDeltaEntityParams p;
            p.force      = true; // new entity → full update from the baseline
            p.kind       = player ? net::DeltaEntityKind::Player
                                  : net::DeltaEntityKind::Entity;
            p.timebase   = timebase;
            p.baseline   = offset;
            p.max_edicts = max_edicts;
            ( void )rt.delta.write_delta_entity( msg, baseline, newent, p );
            ++newindex;
            continue;
        }

        // newnum > oldnum → removal.  force selects removeType 2 (full delete)
        // over 1 (left PVS) when the edict is actually gone.
        ::xash::abi::edict_t *ed =
            rt.arena.edict_num( static_cast<std::size_t>( oldent->number ));
        bool force = ( ed == nullptr );
        if ( ed != nullptr )
        {
            const EntityView v( ed );
            if ( v.freed() || ( v.flags() & ::xash::abi::k_fl_killme ) != 0 )
                force = true;
        }

        net::WriteDeltaEntityParams p;
        p.force      = force;
        p.kind       = net::DeltaEntityKind::Entity;
        p.timebase   = timebase;
        p.max_edicts = max_edicts;
        ( void )rt.delta.write_delta_entity( msg, oldent, nullptr, p );
        ++oldindex;
    }

    msg.write_ubit_long( static_cast<std::uint32_t>( k_last_edict ),
                         k_max_entity_bits ); // end of packetentities
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

bool snapshot_alloc_ring( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const int mc = rt.clients.maxclients;
    if ( mc <= 0 )
        return false;
    const int backup = ( mc == 1 ) ? k_singleplayer_backup : k_multiplayer_backup;

    // Idempotent when the size key is unchanged (legacy Z_Realloc no-op).
    if ( rt.snapshot.packet_entities != nullptr &&
         rt.snapshot.ring_maxclients == mc && rt.snapshot.update_backup == backup )
        return true;

    // maxclients changed → drop the old ring/scratch/frames, realloc fresh.
    free_rings( rt );

    const std::size_t count = static_cast<std::size_t>( mc ) *
                              static_cast<std::size_t>( backup ) *
                              static_cast<std::size_t>( k_num_packet_entities );

    // INVARIANT: num_client_entities is the element count of packet_entities
    // (snapshot.hpp), so the two must be published together.  Allocate into
    // locals and publish nothing until every allocation has succeeded —
    // stamping the count first left the struct as
    // `packet_entities == nullptr && num_client_entities > 0` on OOM, and
    // find_best_baseline takes `% num_client_entities` and then indexes
    // packet_entities, i.e. dereferences null.
    auto *packet_entities = static_cast<::xash::abi::entity_state_t *>(
        ::xash::memory::mem_calloc(
            rt.game_pool, sizeof( ::xash::abi::entity_state_t ) * count ));
    auto *gather_ents = static_cast<::xash::abi::entity_state_t *>(
        ::xash::memory::mem_calloc(
            rt.game_pool, sizeof( ::xash::abi::entity_state_t ) *
                              static_cast<std::size_t>( k_max_visible_packet )));

    bool ok = packet_entities != nullptr && gather_ents != nullptr;

    // Per-client frames rings (legacy allocs at connect; the fixed slot array
    // lets us size them all here with SV_UPDATE_BACKUP, freed in one sweep).
    for ( int i = 0; ok && i < mc; ++i )
    {
        void *f = ::xash::memory::mem_calloc(
            rt.game_pool, sizeof( ClientFrame ) * static_cast<std::size_t>( backup ));
        rt.clients.clients[i].frames = static_cast<ClientFrame *>( f );
        ok = f != nullptr;
    }

    if ( !ok )
    {
        // free_rings sweeps the per-client frames that did get allocated and
        // re-zeroes the ring counters; the two buffers above are not published
        // yet, so they are freed here.
        ::xash::memory::mem_free( packet_entities );
        ::xash::memory::mem_free( gather_ents );
        free_rings( rt );
        return false;
    }

    rt.snapshot.packet_entities      = packet_entities;
    rt.snapshot.gather_ents          = gather_ents;
    rt.snapshot.update_backup        = backup;
    rt.snapshot.update_mask          = backup - 1;
    rt.snapshot.ring_maxclients      = mc;
    rt.snapshot.num_client_entities  = static_cast<int>( count );
    rt.snapshot.next_client_entities = 0;
    return true;
}

bool snapshot_alloc_signon( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.signon_buf != nullptr )
        return true; // idempotent — allocated once per LoadProgs

    void *mem = ::xash::memory::mem_calloc( rt.game_pool, k_max_init_msg );
    if ( mem == nullptr )
        return false;

    rt.signon_buf = static_cast<std::byte *>( mem );
    rt.signon.rebind( std::span<std::byte>( rt.signon_buf, k_max_init_msg ),
                      "Signon" );
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
    // The packet_entities ring + next_client_entities persist across levels
    // (svs is server-static; the staleness guard retires stale frames).
}

void snapshot_shutdown( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    free_rings( rt );
    if ( rt.snapshot.baselines != nullptr )
        ::xash::memory::mem_free( rt.snapshot.baselines );
    if ( rt.signon_buf != nullptr )
    {
        ::xash::memory::mem_free( rt.signon_buf );
        rt.signon_buf = nullptr;
        rt.signon     = net::MessageBuf{};
    }
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

    // Serialize the baselines into the signon message (sv_init.c:510-544):
    // svc_spawnbaseline, then every valid edict's baseline delta'd against a
    // null state, the LAST_EDICT terminator, num_instanced, then the instanced
    // list.  A connecting client replays this to seed its entity states.
    if ( rt.signon_buf == nullptr )
        return; // signon buffer not allocated (pre-S9 fixtures)

    net::MessageBuf                  &signon    = rt.signon;
    const ::xash::abi::entity_state_t nullstate{};
    const int                         max_edicts = static_cast<int>( rt.cfg.max_edicts );

    signon.write_byte( static_cast<std::uint8_t>( k_svc_spawnbaseline ));
    for ( std::size_t entnum = 0; entnum < num; ++entnum )
    {
        ::xash::abi::edict_t *ed = rt.arena.edict_num( entnum );
        if ( ed == nullptr )
            continue;
        const EntityView v( ed );
        if ( v.freed() )
            continue;

        net::DeltaEntityKind kind;
        if ( entnum != 0 && static_cast<int>( entnum ) <= maxclients )
        {
            kind = net::DeltaEntityKind::Player;
        }
        else
        {
            if ( v.modelindex() == 0 )
                continue; // invisible non-player entity
            kind = net::DeltaEntityKind::Entity;
        }

        net::WriteDeltaEntityParams p;
        p.force      = true;
        p.kind       = kind;
        p.timebase   = 1.0;
        p.max_edicts = max_edicts;
        ( void )rt.delta.write_delta_entity( signon, &nullstate,
                                             &rt.snapshot.baselines[entnum], p );
    }
    signon.write_ubit_long( static_cast<std::uint32_t>( k_last_edict ),
                            k_max_entity_bits ); // end of baselines
    signon.write_ubit_long(
        static_cast<std::uint32_t>( rt.snapshot.num_instanced ), k_max_instanced_bits );

    for ( int i = 0; i < rt.snapshot.num_instanced; ++i )
    {
        net::WriteDeltaEntityParams p;
        p.force      = true;
        p.kind       = net::DeltaEntityKind::Entity;
        p.timebase   = 1.0;
        p.max_edicts = max_edicts;
        ( void )rt.delta.write_delta_entity(
            signon, &nullstate, &rt.snapshot.instanced[i].baseline, p );
    }
}

// -----------------------------------------------------------------------------
// Events + pings (the two datagram-riders SV_WriteEntitiesToClient appends).
// -----------------------------------------------------------------------------

void emit_events( ServerRuntime &rt, ServerClient &cl, ClientFrame &to,
                  net::MessageBuf &msg ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const ::xash::abi::event_args_t nullargs{};
    ::xash::abi::event_state_t     &es = cl.events;

    // Count queued events (index 0 ⇒ empty slot).
    int ev_count = 0;
    for ( int ev = 0; ev < k_max_event_queue; ++ev )
        if ( es.ei[ev].index != 0 )
            ++ev_count;

    if ( ev_count == 0 )
        return; // nothing to send

    if ( ev_count >= k_max_event_queue / 2 )
        ev_count = ( k_max_event_queue / 2 ) - 1;

    // Resolve each event's packet_index against this frame's ring window, and
    // strip the args the client re-derives (origin/angles unless flagged,
    // velocity + ducking always).
    for ( int i = 0; i < k_max_event_queue; ++i )
    {
        ::xash::abi::event_info_t &info = es.ei[i];
        if ( info.index == 0 )
            continue;

        const int ent_index = info.entity_index;

        int j = 0;
        for ( ; j < to.num_entities; ++j )
        {
            const ::xash::abi::entity_state_t &state =
                rt.snapshot.packet_entities[( to.first_entity + j ) %
                                            rt.snapshot.num_client_entities];
            if ( state.number == ent_index )
                break;
        }

        if ( j < to.num_entities )
        {
            info.packet_index = static_cast<std::int16_t>( j );
            info.args.ducking = 0;

            if ( ( info.args.flags & ::xash::abi::k_fevent_origin ) == 0 )
            {
                info.args.origin[0] = info.args.origin[1] = info.args.origin[2] = 0.0f;
            }
            if ( ( info.args.flags & ::xash::abi::k_fevent_angles ) == 0 )
            {
                info.args.angles[0] = info.args.angles[1] = info.args.angles[2] = 0.0f;
            }
            info.args.velocity[0] = info.args.velocity[1] = info.args.velocity[2] = 0.0f;
        }
        else
        {
            // Couldn't find — point past the packet list and carry the entity.
            info.packet_index  = static_cast<std::int16_t>( to.num_entities );
            info.args.entindex = ent_index;
        }
    }

    msg.write_byte( static_cast<std::uint8_t>( k_svc_event ));
    msg.write_ubit_long( static_cast<std::uint32_t>( ev_count ), 5 );

    int count = 0;
    for ( int i = 0; i < k_max_event_queue; ++i )
    {
        ::xash::abi::event_info_t &info = es.ei[i];

        if ( info.index == 0 )
        {
            info.packet_index = -1;
            info.entity_index = -1;
            continue;
        }

        // Only serialise while there is room in the clamped budget.
        if ( count < ev_count )
        {
            msg.write_ubit_long( info.index, k_max_event_bits );

            if ( info.packet_index == -1 )
            {
                msg.write_one_bit( 0 );
            }
            else
            {
                msg.write_one_bit( 1 );
                msg.write_ubit_long( static_cast<std::uint32_t>( info.packet_index ),
                                     k_max_entity_bits );

                if ( std::memcmp( &nullargs, &info.args,
                                  sizeof( ::xash::abi::event_args_t )) == 0 )
                {
                    msg.write_one_bit( 0 );
                }
                else
                {
                    msg.write_one_bit( 1 );
                    rt.delta.write_delta_event( msg, &nullargs, &info.args );
                }
            }

            if ( info.fire_time != 0.0f )
            {
                msg.write_one_bit( 1 );
                msg.write_word(
                    static_cast<std::uint16_t>( static_cast<int>( info.fire_time * 100.0f )));
            }
            else
            {
                msg.write_one_bit( 0 );
            }
        }

        info.index        = 0;
        info.packet_index = -1;
        info.entity_index = -1;
        ++count;
    }
}

// SV_CalcPing (sv_client.c:1099): average the recent frame-ring ping samples
// (set when the client acks each frame — the netchan receive seam).
static int calc_ping( const ServerRuntime &rt, const ServerClient &cl ) noexcept
{
    if ( cl.fakeclient || cl.frames == nullptr )
        return 0; // bots have no real ping

    int back;
    if ( rt.snapshot.update_backup <= 31 )
    {
        back = rt.snapshot.update_backup / 2;
        if ( back <= 0 )
            return 0;
    }
    else
    {
        back = 16;
    }

    float ping  = 0.0f;
    int   count = 0;
    for ( int i = 0; i < back; ++i )
    {
        const int          idx   = cl.incoming_acknowledged + ~i;
        const ClientFrame &frame = cl.frames[idx & rt.snapshot.update_mask];
        if ( frame.ping_time > 0.0f )
        {
            ping += frame.ping_time;
            ++count;
        }
    }

    if ( count > 0 )
        return static_cast<int>(( ping / static_cast<float>( count )) * 1000.0f );
    return 0;
}

// SV_GetPlayerStats (sv_client.c:1317): recompute ping/loss at most every 2s;
// the legacy function-static cache is held per-client here (Q-2).
static void get_player_stats( ServerRuntime &rt, ServerClient &cl, int &ping,
                              int &packet_loss ) noexcept
{
    if ( rt.clients.realtime >= cl.next_checkpingtime )
    {
        cl.next_checkpingtime = rt.clients.realtime + 2.0;
        cl.last_ping          = calc_ping( rt, cl );
        cl.last_loss          = cl.packet_loss;
    }

    ping        = cl.last_ping;
    packet_loss = cl.last_loss;
}

void emit_pings( ServerRuntime &rt, net::MessageBuf &msg ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    msg.write_byte( static_cast<std::uint8_t>( k_svc_pings ));

    for ( int i = 0; i < rt.clients.maxclients; ++i )
    {
        ServerClient &cl = rt.clients.clients[i];
        if ( cl.state != ClientState::Spawned )
            continue;

        int ping        = 0;
        int packet_loss = 0;
        get_player_stats( rt, cl, ping, packet_loss );

        // 25 bits per client: present-bit, idx(5), ping(12), packet_loss(7).
        msg.write_one_bit( 1 );
        msg.write_ubit_long( static_cast<std::uint32_t>( i ), k_max_client_bits );
        msg.write_ubit_long( static_cast<std::uint32_t>( ping ), 12 );
        msg.write_ubit_long( static_cast<std::uint32_t>( packet_loss ), 7 );
    }

    msg.write_one_bit( 0 ); // end marker
}

bool should_update_ping( ServerRuntime &rt, ServerClient &cl ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cl.hltv ) // FCL_HLTV_PROXY
    {
        if ( rt.clients.realtime < cl.next_checkpingtime )
            return false;
        cl.next_checkpingtime = rt.clients.realtime + 2.0;
        return true;
    }

    // Regular clients: only while the scoreboard (IN_SCORE) is held down.
    return ( cl.lastcmd.buttons & k_in_score ) != 0;
}

void write_entities_to_client( ServerRuntime &rt, ServerClient &cl, int frame_index,
                               net::MessageBuf &msg ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cl.frames == nullptr || rt.snapshot.packet_entities == nullptr )
        return; // ring not allocated (pre-spawn) — nothing to send

    ClientFrame &frame = cl.frames[frame_index & rt.snapshot.update_mask];

    // Computed before the gather (legacy sv_frame.c:621): the HLTV branch bumps
    // next_checkpingtime as a side effect, which SV_EmitPings then observes.
    const bool send_pings = should_update_ping( rt, cl );

    std::memset( rt.snapshot.sended, 0, sizeof( rt.snapshot.sended ));
    rt.level.hostflags &= ~k_svf_merge_visibility;
    rt.bridge.merge_visibility = false;

    // Gather everything visible to the eye (may include portal viewpoints).
    int num = 0;
    add_entities_to_packet( rt, cl.view_entity, cl.edict, num, true );

    // Portal recursion + dedup can interleave numbers — resort for the delta.
    std::qsort( rt.snapshot.gather_ents, static_cast<std::size_t>( num ),
                sizeof( ::xash::abi::entity_state_t ), entity_numbers_cmp );

    // Ring overflow guard (sv_frame.c:645): a week+ of uptime; breaks all
    // clients but never wraps mid-frame.
    if ( static_cast<std::uint32_t>( rt.snapshot.next_client_entities ) +
             static_cast<std::uint32_t>( num ) >=
         0x7FFFFFFEu )
    {
        rt.snapshot.next_client_entities = 0;
        // XASH3DPP-STUB(chunk6): SV_FinalMessage forced reconnect (send sub-slice).
    }

    // Copy the sorted states into the shared circular ring, recording cl's frame.
    frame.first_entity = rt.snapshot.next_client_entities;
    frame.num_entities = 0;
    for ( int i = 0; i < num; ++i )
    {
        ::xash::abi::entity_state_t *slot =
            &rt.snapshot.packet_entities[rt.snapshot.next_client_entities %
                                         rt.snapshot.num_client_entities];
        *slot = rt.snapshot.gather_ents[i];
        ++rt.snapshot.next_client_entities;
        ++frame.num_entities;
    }

    emit_packet_entities( rt, cl, frame, msg );
    emit_events( rt, cl, frame, msg );
    if ( send_pings )
        emit_pings( rt, msg );
}

void write_clientdata_to_message( ServerRuntime &rt, ServerClient &cl,
                                  int frame_index, net::MessageBuf &msg ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( cl.frames == nullptr || cl.edict == nullptr )
        return;

    ClientFrame &frame = cl.frames[frame_index & rt.snapshot.update_mask];
    frame.senttime  = rt.clients.realtime;
    frame.ping_time = -1.0f; // latency computed on the client's ack

    EntityView clent( cl.edict );

    if ( cl.chokecount != 0 )
    {
        msg.write_byte( static_cast<std::uint8_t>( k_svc_choke ));
        cl.chokecount = 0;
    }

    // fixangle: 1 = absolute view (svc_setangle), 2 = mover turn (svc_addangle,
    // consuming avelocity[YAW]).  Reset unconditionally afterwards.
    switch ( clent.fixangle() )
    {
    case 1:
    {
        const Vec3 a = clent.angles();
        msg.write_byte( static_cast<std::uint8_t>( k_svc_setangle ));
        msg.write_vec3_angles( a.x, a.y, a.z );
        break;
    }
    case 2:
    {
        Vec3 av = clent.avelocity();
        msg.write_byte( static_cast<std::uint8_t>( k_svc_addangle ));
        msg.write_bit_angle( av.y, 16 ); // avelocity[YAW]
        av.y = 0.0f;
        clent.set_avelocity( av );
        break;
    }
    default:
        break;
    }
    clent.set_fixangle( 0 );

    // pfnUpdateClientData fills the frame's clientdata (weapon prediction gated
    // by FCL_LOCAL_WEAPONS).
    frame.clientdata = ::xash::abi::clientdata_t{};
    const int sendweapons = cl.local_weapons ? 1 : 0;
    if ( rt.game.funcs().pfnUpdateClientData != nullptr )
        rt.game.funcs().pfnUpdateClientData( cl.edict, sendweapons, &frame.clientdata );

    msg.write_byte( static_cast<std::uint8_t>( k_svc_clientdata ));
    if ( cl.hltv )
        return; // HLTV proxy: header only, no clientdata body

    const ::xash::abi::clientdata_t  nullcd{};
    const ::xash::abi::clientdata_t *from_cd = &nullcd;
    if ( cl.delta_sequence == -1 )
    {
        msg.write_one_bit( 0 ); // no delta compression
    }
    else
    {
        msg.write_one_bit( 1 );
        msg.write_byte( static_cast<std::uint8_t>( cl.delta_sequence ));
        from_cd = &cl.frames[cl.delta_sequence & rt.snapshot.update_mask].clientdata;
    }

    rt.delta.write_clientdata( msg, from_cd, &frame.clientdata, rt.level.time );

    // Weapon prediction: 64-slot weapondata deltas.
    if ( cl.local_weapons && rt.game.funcs().pfnGetWeaponData != nullptr &&
         rt.game.funcs().pfnGetWeaponData( cl.edict, frame.weapondata ))
    {
        const ::xash::abi::weapon_data_t nullwd{};
        for ( int i = 0; i < k_max_local_weapons; ++i )
        {
            const ::xash::abi::weapon_data_t *from_wd = &nullwd;
            if ( cl.delta_sequence != -1 )
                from_wd = &cl.frames[cl.delta_sequence & rt.snapshot.update_mask]
                               .weapondata[i];
            rt.delta.write_weapon_data( msg, from_wd, &frame.weapondata[i],
                                        rt.level.time, i );
        }
    }

    msg.write_one_bit( 0 ); // clientdata blob end marker
}

namespace {

// One client datagram body (legacy MAX_DATAGRAM) + the framed wire packet.
inline constexpr std::size_t k_max_datagram = 16384; // net_ws.h MAX_DATAGRAM
inline constexpr std::size_t k_wire_max     = 65536; // NET_MAX_MESSAGE

// Netchan_TransmitBits + NET_SendPacket: frame `unreliable` (the datagram body,
// `bits` long) plus the netchan's reliable queue into a wire packet and hand it
// to the host-owned NetworkContext, addressed to the client.  Advances the
// bandwidth-choke cleartime by the bytes sent.  No-op when offline (rt.net null).
void transmit_client( ServerRuntime &rt, ServerClient &cl, int slot,
                      std::span<const std::byte> unreliable,
                      std::size_t bits ) noexcept
{
    if ( rt.net == nullptr )
        return;

    net::Netchan                     &nc = rt.clients.netchans[slot];
    std::array<std::byte, k_wire_max> out;
    const auto sent =
        nc.transmit_bits( unreliable, bits, std::span<std::byte>{ out } );
    if ( !sent )
        return;

    nc.update_choke( rt.clients.realtime, *sent );
    ( void )rt.net->send_packet( net::SocketKind::Server,
                                 std::span<const std::byte>{ out.data(), *sent },
                                 cl.adr );
}

} // namespace

void send_client_datagram( ServerRuntime &rt, ServerClient &cl, int frame_index,
                           net::MessageBuf &msg ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // Always send the server time at a new frame.
    msg.write_byte( static_cast<std::uint8_t>( k_svc_time ));
    msg.write_float( static_cast<float>( rt.level.time ));

    write_clientdata_to_message( rt, cl, frame_index, msg );
    write_entities_to_client( rt, cl, frame_index, msg );

    // Append the accumulated per-client unreliable staging (sounds / tempents /
    // multicast copies) if it fits, then clear it (legacy warns on overflow —
    // that 5s-rate-limited console path is a host-log seam).
    if ( cl.datagram_bits > 0 && cl.datagram_bits <= msg.num_bits_left() )
    {
        const std::size_t bytes = ( cl.datagram_bits + 7 ) / 8;
        ( void )msg.write_bits(
            std::span<const std::byte>( cl.datagram, bytes ), cl.datagram_bits );
    }
    cl.datagram_bits = 0;

    // Netchan_TransmitBits: frame the built body + the reliable queue into a
    // wire packet and send it to the client through the host NetworkContext.
    const int slot = static_cast<int>( &cl - rt.clients.clients );
    transmit_client( rt, cl, slot,
                     msg.data().first( ( msg.num_bits_written() + 7 ) / 8 ),
                     msg.num_bits_written() );
}

void update_to_reliable_messages( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    ClientMachinery &cm = rt.clients;

    // XASH3DPP-STUB(chunk6-S9): the per-spawned-client FCL_RESEND_USERINFO
    // (SV_FullClientUpdate) + FCL_RESEND_MOVEVARS (SV_FullUpdateMovevars) resends
    // queue into reliable_datagram / netchan.message before the fan-out —
    // deferred with those wire builders.

    // Drop the unreliable broadcast buffers whole if they overflowed (legacy).
    if ( cm.datagram.overflowed() )
        cm.datagram.reset();
    if ( cm.spec_datagram.overflowed() )
        cm.spec_datagram.reset();

    // Fan the reliable broadcast to every connected (non-fake) client's netchan
    // reliable queue.  reliable_datagram is byte-framed (svc_* commands), so a
    // byte-granular append matches; the Netchan_CreateFragments fallback for an
    // over-large broadcast (download-class) is an OQ-8 seam.
    const std::size_t reliable_bytes = cm.reliable_datagram.num_bytes_written();
    if ( reliable_bytes > 0 )
    {
        const std::span<const std::byte> payload =
            cm.reliable_datagram.data().first( reliable_bytes );
        for ( int i = 0; i < cm.maxclients; ++i )
        {
            ServerClient &cl = cm.clients[i];
            if ( cl.state == ClientState::Free ||
                 cl.state == ClientState::Zombie || cl.fakeclient )
                continue;
            ( void )cm.netchans[i].write_reliable( payload );
        }
    }

    // XASH3DPP-STUB(chunk6-S9): the unreliable sv.datagram / sv.spec_datagram →
    // per-client cl.datagram append (particle / HLTV-spec broadcasts) — their
    // producers are stubbed; the raw-buffer bit-append lands with them.

    cm.reliable_datagram.reset();
    cm.spec_datagram.reset();
    cm.datagram.reset();
}

void send_client_messages( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.level.state == ServerState::Dead )
        return;

    // Fan pending broadcasts (reliable_datagram → netchans) before the per-client
    // send (SV_SendClientMessages runs SV_UpdateToReliableMessages first).
    update_to_reliable_messages( rt );

    ClientMachinery &cm        = rt.clients;
    const double     realtime  = cm.realtime;
    const double     frametime = static_cast<double>( rt.level.frametime );

    for ( int i = 0; i < cm.maxclients; ++i )
    {
        ServerClient &cl = cm.clients[i];

        // legacy `state <= cs_zombie`: the xash3dpp enum is reordered, so the
        // two terminal states (Free / Zombie) are tested explicitly.
        if ( cl.state == ClientState::Free || cl.state == ClientState::Zombie ||
             cl.fakeclient )
            continue;

        if ( cl.skip_net_message ) // FCL_SKIP_NET_MESSAGE (SV_SkipUpdates)
        {
            cl.skip_net_message = false;
            continue;
        }

        // XASH3DPP-STUB(chunk6-S9): host_limitlocal + NET_IsLocalAddress force
        // send (listen-server local client) — dedicated has no local client.

        if ( cl.state == ClientState::Spawned )
        {
            const double until = cl.next_messagetime - ( realtime + frametime );
            if ( until <= 0.0 || until > 2.0 ) // due now, or a hosed clock
                cl.send_net_message = true;
        }

        // XASH3DPP-STUB(chunk6-S9): reliable-overflow drop (MSG_CheckOverflow on
        // netchan.message) + the sv_failuretime "stop sending" gate.

        if ( !cl.send_net_message )
            continue;

        net::Netchan &nc = cm.netchans[i];
        if ( !nc.can_packet( realtime, cl.state == ClientState::Spawned ) )
        {
            cl.chokecount++; // bandwidth choke active — skip this frame
            continue;
        }

        cl.next_messagetime = realtime + frametime + cl.next_messageinterval;
        cl.send_net_message = false;

        if ( cl.state == ClientState::Spawned )
        {
            std::array<std::byte, k_max_datagram> body;
            net::MessageBuf msg{ std::span<std::byte>{ body } };
            send_client_datagram(
                rt, cl, static_cast<int>( nc.outgoing_sequence() ), msg );
        }
        else
        {
            // keepalive: flush the reliable queue with an empty datagram body.
            transmit_client( rt, cl, i, {}, 0 );
        }
    }
}

} // namespace xash::server
