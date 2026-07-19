// xash3dpp — landmark-transition machinery (Chunk 8, slice S8.6).
// See adjacent_transfer.hpp for the derived flow + sv_save.c cites.

#include <xash3dpp/private/save/adjacent_transfer.hpp>

#include <xash3dpp/private/save/entity_patch.hpp> // read_entity_patch / apply_entity_patch

#include <xash3dpp/abi/eiface.hpp>       // k_fenttable_* flags
#include <xash3dpp/abi/server_consts.hpp> // k_fl_killme / k_fl_client / k_movetype_follow
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/string.hpp> // ci_equal (Q_stricmp)

#include <cstring>
#include <string_view>

namespace xash::save {

namespace
{
constexpr char k_tag[] = "save";

// SV_IsValidEdict (server.h:635 SV_CheckEdict) projected to the save-target
// component (it does not own the arena bounds): a non-null, non-freed edict.
// Mirrors level_state_writer.cpp's identical local helper.
[[nodiscard]] bool is_valid_edict( const ::xash::abi::edict_t *e ) noexcept
{
    return e != nullptr && e->free == 0;
}

// FBitSet over the raw ABI flag ints, unsigned-clean (FENTTABLE_PLAYER is
// 0x80000000 — bit 31, negative as a signed int).
[[nodiscard]] bool bits_overlap( int a, int b ) noexcept
{
    return ( static_cast<unsigned>( a ) & static_cast<unsigned>( b ) ) != 0;
}

// A fixed-width ABI char array (mapName[32] / landmarkName[32]) as a C string
// view, stopping at the first NUL — the wire strings are NUL-terminated.
template<std::size_t N>
[[nodiscard]] std::string_view cstr_view( const char ( &arr )[N] ) noexcept
{
    std::size_t n = 0;
    while ( n < N && arr[n] != '\0' )
        ++n;
    return std::string_view( arr, n );
}
} // namespace

// ---------------------------------------------------------------------------
// Landmark offset math (LandmarkOrigin, sv_save.c:452-466).
// ---------------------------------------------------------------------------

SaveVec3 landmark_origin( std::span<const ::xash::abi::LEVELLIST> connections,
                          std::string_view landmark_name ) noexcept
{
    for ( const auto &c : connections )
    {
        // sv_save.c:458 uses Q_strcmp — CASE-SENSITIVE on the landmark name.
        if ( cstr_view( c.landmarkName ) == landmark_name )
            return SaveVec3{ c.vecLandmarkOrigin[0], c.vecLandmarkOrigin[1], c.vecLandmarkOrigin[2] };
    }
    return SaveVec3{}; // VectorClear (sv_save.c:465)
}

SaveVec3 compute_landmark_offset( std::span<const ::xash::abi::LEVELLIST> new_level_connections,
                                  std::span<const ::xash::abi::LEVELLIST> adjacent_connections,
                                  std::string_view landmark_name ) noexcept
{
    // sv_save.c:1976-1978: offset = origin(new) - origin(adjacent).
    const SaveVec3 nl = landmark_origin( new_level_connections, landmark_name );
    const SaveVec3 al = landmark_origin( adjacent_connections, landmark_name );
    return SaveVec3{ nl.x - al.x, nl.y - al.y, nl.z - al.z };
}

// ---------------------------------------------------------------------------
// levelMask construction (LoadAdjacentEnts, sv_save.c:1972-1988).
// ---------------------------------------------------------------------------

int compute_transition_mask( std::span<const ::xash::abi::LEVELLIST> adjacent_connections,
                             std::string_view new_map_name, bool is_old_level ) noexcept
{
    unsigned mask = 0;

    // sv_save.c:1980-1981 — bring the player over from the level we came from.
    if ( is_old_level )
        mask |= ::xash::abi::k_fenttable_player;

    // sv_save.c:1983-1988 (EntryInTable while-loop, :414-425): every connection
    // slot in THIS adjacent level's list that leads back to the just-spawned map
    // (case-insensitive Q_stricmp) contributes BIT(idx).
    const std::size_t n = adjacent_connections.size();
    for ( std::size_t idx = 0; idx < n; ++idx )
    {
        if ( idx >= 31 )
            break; // BIT(31+) would collide with the semantic flags / overflow — cannot occur
                   // for a loader-bounded list (k_max_level_connections == 16), guarded defensively.
        if ( ::xash::utilities::ci_equal( cstr_view( adjacent_connections[idx].mapName ), new_map_name ) )
            mask |= ( 1u << idx );
    }

    return static_cast<int>( mask );
}

// ---------------------------------------------------------------------------
// Decal landmark-offset hook (SaveClientState :1239-1240 / LoadClientState :1349-1350).
// ---------------------------------------------------------------------------

SaveVec3 decal_offset_for_save( SaveVec3 position, SaveVec3 landmark_offset, bool use_landmark,
                                int decal_flags ) noexcept
{
    // sv_save.c:1239-1240: only for a landmark save AND an FDECAL_USE_LANDMARK
    // (no-origin-brush) decal; VectorSubtract.
    if ( use_landmark && bits_overlap( decal_flags, k_fdecal_use_landmark ) )
        return SaveVec3{ position.x - landmark_offset.x, position.y - landmark_offset.y,
                         position.z - landmark_offset.z };
    return position;
}

SaveVec3 decal_offset_for_load( SaveVec3 position, SaveVec3 landmark_offset, bool use_landmark,
                                int decal_flags ) noexcept
{
    // sv_save.c:1349-1350: the inverse — VectorAdd under the same gate.
    if ( use_landmark && bits_overlap( decal_flags, k_fdecal_use_landmark ) )
        return SaveVec3{ position.x + landmark_offset.x, position.y + landmark_offset.y,
                         position.z + landmark_offset.z };
    return position;
}

// ---------------------------------------------------------------------------
// EntityInSolid (sv_save.c:476-489).
// ---------------------------------------------------------------------------

bool entity_in_solid( const ::xash::abi::edict_t &pent, ISolidTestProvider &solid ) noexcept
{
    const ::xash::abi::edict_t *aiment = pent.v.aiment;

    // sv_save.c:482-483 — a MOVETYPE_FOLLOW entity attached to a client always
    // goes through (never suppressed).
    if ( pent.v.movetype == ::xash::abi::k_movetype_follow && is_valid_edict( aiment ) &&
         bits_overlap( aiment->v.flags, ::xash::abi::k_fl_client ) )
        return false;

    // sv_save.c:485 — VectorAverage(absmin, absmax) == the AABB center.
    const SaveVec3 point{
        ( pent.v.absmin[0] + pent.v.absmax[0] ) * 0.5f,
        ( pent.v.absmin[1] + pent.v.absmax[1] ) * 0.5f,
        ( pent.v.absmin[2] + pent.v.absmax[2] ) * 0.5f,
    };

    // sv_save.c:486-488 — svs.groupmask = pent->v.groupinfo; SV_PointContents ==
    // CONTENTS_SOLID.
    return solid.point_in_solid( point, pent.v.groupinfo );
}

// ---------------------------------------------------------------------------
// CreateEntityTransitionList (sv_save.c:1836-1922).
// ---------------------------------------------------------------------------

Result<int>
create_entity_transition_list( LevelState &state, EntityTable &table,
                               ITransitionRestorer &restorer, ISolidTestProvider &solid,
                               int level_mask, const TransitionContext &ctx,
                               const RestoreConfig &config ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    int movedCount = 0;

    // --- Phase 1: CreateEntitiesInRestoreList(pSaveData, level_mask, false)
    //     (sv_save.c:1845 -> 1410-1460 with create_world == false). ---
    if ( !restorer.create_entities_override( table, level_mask, config ) )
    {
        for ( std::size_t i = 0; i < table.count(); ++i )
        {
            ::xash::abi::ENTITYTABLE &row       = table.row( i );
            const std::string_view    classname = table.classname( i );
            ::xash::abi::edict_t     *pent      = nullptr;

            // Row guard, create_world == false (sv_save.c:1428): `classname &&
            // size` — the `|| !create_world` term makes the REMOVED test vacuous
            // (REMOVED rows fall through the guard but are inactive; see below).
            if ( !classname.empty() && row.size != 0 )
            {
                // active = FBitSet(pTable->flags, levelMask) (sv_save.c:1431).
                const bool active = bits_overlap( row.flags, level_mask );

                if ( row.id > 0 && row.id < config.max_clients + 1 ) // client (:1440-1449)
                {
                    // FENTTABLE_PLAYER mismatch diagnostic (:1444-1445) — printed
                    // whenever the client branch is entered, regardless of active;
                    // does NOT skip creation.
                    if ( !bits_overlap( row.flags, static_cast<int>( ::xash::abi::k_fenttable_player ) ) )
                        ::xash::core::logf( ::xash::core::LogLevel::Error, k_tag,
                                            "ENTITY IS NOT A PLAYER: %zu", i );
                    // Create only if active; the seam applies the residual
                    // SV_IsValidEdict(ed) gate and returns nullptr otherwise.
                    if ( active )
                        pent = restorer.create_entity( RestoreCreateKind::Client, row.id, classname );
                }
                else if ( active ) // named (:1451-1453); id==0 falls here (no worldspawn without create_world)
                {
                    pent = restorer.create_entity( RestoreCreateKind::Named, row.id, classname );
                }
            }

            row.pent = pent; // sv_save.c:1457
        }
    }

    // --- Phase 2: the per-entity restore loop (sv_save.c:1848-1919). ---
    for ( std::size_t i = 0; i < table.count(); ++i )
    {
        ::xash::abi::ENTITYTABLE &row  = table.row( i );
        ::xash::abi::edict_t     *pent = row.pent;

        // sv_save.c:1856 — SV_IsValidEdict(pent) && FBitSet(pTable->flags,
        // levelMask).  Only such rows are processed (and only they call
        // SV_FreeOldEntities, :1917 — inside the guard).
        if ( !is_valid_edict( pent ) || !bits_overlap( row.flags, level_mask ) )
            continue;

        // Position: pCurrentData = pBaseData + location; the bounded
        // [location, location+size) window (sv_save.c:1851-1852).  Reject an
        // out-of-range window (reject-gracefully; legacy is unchecked), matching
        // LevelStateLoader::restore_entities.
        if ( state.buffer == nullptr || row.location < 0 || row.size < 0 )
            return std::unexpected( SaveError::CorruptHeader );
        auto view = state.buffer->view_at(
            state.table_size + static_cast<std::size_t>( row.location ),
            static_cast<std::size_t>( row.size ) );
        if ( !view )
            return std::unexpected( SaveError::CorruptHeader );

        if ( bits_overlap( row.flags, static_cast<int>( ::xash::abi::k_fenttable_global ) ) )
        {
            // Global-entity merge pass (sv_save.c:1858-1888).
            const GlobalMergeResult res =
                restorer.merge_global_entity( pent, i, *view, *state.tokens, ctx );
            if ( res.merged )
            {
                movedCount++; // sv_save.c:1881
            }
            else
            {
                // sv_save.c:1885-1886 — repoint the table at the existing global
                // (so decals on its brush model find their parent) if valid.
                if ( res.repoint_target != nullptr )
                    row.pent = res.repoint_target;
                pent->v.flags |= ::xash::abi::k_fl_killme; // sv_save.c:1887 (kills the TEMP edict)
            }
        }
        else
        {
            // Moveable / landmark transfer pass (sv_save.c:1890-1912).
            const int rc = restorer.transfer_entity( pent, i, *view, *state.tokens, ctx );
            if ( rc < 0 )
            {
                pent->v.flags |= ::xash::abi::k_fl_killme; // sv_save.c:1896
            }
            else if ( !bits_overlap( row.flags, static_cast<int>( ::xash::abi::k_fenttable_player ) ) &&
                      entity_in_solid( *pent, solid ) )
            {
                // Stuck outside the new world — suppress (sv_save.c:1900-1905).
                pent->v.flags |= ::xash::abi::k_fl_killme;
            }
            else
            {
                // sv_save.c:1909 — PLAIN ASSIGN (clobbers the row's PVS/other
                // bits); marks the entity gone from the ADJACENT level so a later
                // transition does not re-transfer it.
                row.flags = static_cast<int>( ::xash::abi::k_fenttable_removed );
                movedCount++; // sv_save.c:1910
            }
        }

        restorer.free_old_entities(); // sv_save.c:1917
    }

    return movedCount;
}

// ---------------------------------------------------------------------------
// LoadAdjacentEnts (sv_save.c:1931-2015) — AdjacentTransfer.
// ---------------------------------------------------------------------------

Result<void>
AdjacentTransfer::run( const AdjacentTransferParams &params, IAdjacentLevelSource &source,
                       ITransitionRestorer &restorer, ISolidTestProvider &solid ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    LevelStateLoader loader( *buf_, *table_ );
    bool             foundprevious = false;

    const std::span<const ::xash::abi::LEVELLIST> conns = params.new_level_connections;

    for ( std::size_t i = 0; i < conns.size(); ++i )
    {
        const std::string_view map_i = cstr_view( conns[i].mapName );

        // sv_save.c:1950-1951 — set BEFORE the dedup continue (a duplicate old-
        // level connection still satisfies foundprevious).
        if ( ::xash::utilities::ci_equal( map_i, params.old_level ) )
            foundprevious = true;

        // sv_save.c:1953-1961 — only process each map once.
        bool duplicate = false;
        for ( std::size_t t = 0; t < i; ++t )
        {
            if ( ::xash::utilities::ci_equal( map_i, cstr_view( conns[t].mapName ) ) )
            {
                duplicate = true;
                break;
            }
        }
        if ( duplicate )
            continue;

        // LoadSaveData(map) — a missing `.HL1` is silently skipped (sv_save.c:1965).
        std::optional<std::vector<std::byte>> image = source.load_level_image( map_i );
        if ( !image )
            continue;

        // ParseSaveTables (sv_save.c:1967).
        LevelState state;
        if ( auto r = loader.load( *image, state ); !r )
            return std::unexpected( r.error() );

        // EntityPatchRead + apply (sv_save.c:1968) — REMOVED rows become inactive
        // for the transfer (their PVS bits are clobbered by the plain assign).
        if ( std::optional<std::vector<std::byte>> patch = source.load_entity_patch( map_i ) )
        {
            const Result<std::vector<std::int32_t>> indices = read_entity_patch( *patch );
            if ( !indices )
                return std::unexpected( indices.error() );
            if ( auto r = apply_entity_patch( *table_, *indices ); !r )
                return std::unexpected( r.error() );
        }

        // Transition context (sv_save.c:1970-1978).
        TransitionContext ctx;
        ctx.time_basis      = params.sv_time; // :1970 pSaveData->time = sv.time (NOT header-rebased)
        ctx.use_landmark    = true;           // :1971
        ctx.landmark_offset = compute_landmark_offset( conns, state.connections, params.landmark_name );

        // levelMask (sv_save.c:1980-1988).
        const bool is_old     = ::xash::utilities::ci_equal( map_i, params.old_level );
        const int  level_mask = compute_transition_mask( state.connections, params.new_map_name, is_old );

        // CreateEntityTransitionList only when the mask selected anything
        // (sv_save.c:1990 `if( flags )`).
        int moved_count = 0;
        if ( level_mask != 0 )
        {
            const Result<int> mc = create_entity_transition_list( state, *table_, restorer, solid,
                                                                  level_mask, ctx, params.config );
            if ( !mc )
                return std::unexpected( mc.error() );
            moved_count = *mc;
        }

        // EntityPatchWrite back if entities moved (sv_save.c:1993-2002); failure
        // is the transition-broken hard-fail (legacy Host_Error).
        if ( moved_count != 0 )
        {
            if ( auto r = source.store_entity_patch( map_i, *table_ ); !r )
                return std::unexpected( SaveError::TransitionBroken );
        }

        // sv_save.c:2005 LoadClientState(..., adjacent=true) — the DECAL move —
        // is DEFERRED (live decal application; only the offset math is exposed by
        // decal_offset_for_save/load).
    }

    // sv_save.c:2013-2014 — missing back-connection is the hard-fail contract.
    if ( !foundprevious )
        return std::unexpected( SaveError::TransitionBroken );

    return {};
}

} // namespace xash::save
