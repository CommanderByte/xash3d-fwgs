// xash3dpp — LoadGameState `.HL1` per-level loader core (Chunk 8, slice S8.4).
// See level_state_loader.hpp for the derived load order + sv_save.c cites.

#include <xash3dpp/private/save/level_state_loader.hpp>

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/format.hpp>

#include <xash3dpp/abi/server_consts.hpp> // k_fl_killme
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp> // server_lightstyles (header count guard)

namespace xash::save {

namespace
{
constexpr char k_tag[] = "save";
} // namespace

// ---------------------------------------------------------------------------
// LevelState::data_region
// ---------------------------------------------------------------------------

std::span<const std::byte> LevelState::data_region() const noexcept
{
    if ( buffer == nullptr )
        return {};
    const std::span<const std::byte> loaded = buffer->data(); // [ETABLE][data]
    if ( table_size > loaded.size() )
        return {}; // defensive — table_size is always <= loaded.size() after load()
    return loaded.subspan( table_size );
}

// ---------------------------------------------------------------------------
// LevelStateLoader::load
// ---------------------------------------------------------------------------

Result<void> LevelStateLoader::load( std::span<const std::byte> image, LevelState &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    SaveBuffer  &buf   = *buf_;
    EntityTable &table = *table_;

    // --- 1. Counts preamble (LoadSaveData header read, sv_save.c:917-931; the
    //        rewrite adds the bounds the real load path lacks). ---
    auto pre = parse_hl1_preamble( image );
    if ( !pre )
        return std::unexpected( pre.error() );

    // --- 2. Token rebuild (BuildHashTable, sv_save.c:823-844/947).  Sized to the
    //        file's tokenCount (SaveInit, :939); constructed in place because a
    //        TokenTable is non-movable and its slot count is only now known. ---
    out.tokens.emplace( static_cast<std::size_t>( pre->token_count ) );
    const std::span<const std::byte> token_blob =
        image.subspan( pre->token_offset(), static_cast<std::size_t>( pre->token_size ) );
    if ( auto r = out.tokens->rebuild( token_blob ); !r )
        return std::unexpected( r.error() );

    // --- 3. Copy the size bytes ([ETABLE][data]) into the working buffer
    //        (LoadSaveData, sv_save.c:954 FS_Read into pBaseData). ---
    const std::span<const std::byte> payload =
        image.subspan( pre->etable_offset(), static_cast<std::size_t>( pre->size ) );
    if ( auto r = buf.load_from( payload ); !r )
        return std::unexpected( r.error() );

    const std::span<const std::byte> buffer = buf.data(); // [ETABLE][data]

    // --- 4. ETABLE region (ParseSaveTables, sv_save.c:973-979): init to
    //        tableCount, then deserialize that many blocks.  The end offset IS the
    //        recomputed tableSize — the data-region base (the file never stores
    //        it) — matching the pBaseData rebase at :981. ---
    table.init( static_cast<std::size_t>( pre->table_count ) );
    std::size_t offset = 0;
    if ( auto r = table.deserialize( buffer, offset, *out.tokens ); !r )
        return std::unexpected( r.error() );
    out.table_size = offset; // sv_save.c:981 (pBaseData = pCurrentData; size = 0)

    // --- 5. Save Header (sv_save.c:985).  time_basis 0: the header's own
    //        FIELD_TIME is not rebased (the writer wrote it with basis 0, :1520;
    //        pSaveData->time is still 0 here, :951).  Overlay onto a pre-zeroed
    //        struct (read_descriptor_block's overlay model). ---
    out.header = SaveHeader{};
    if ( auto r = read_descriptor_block( buffer, offset, *out.tokens, "Save Header",
                                         &out.header, k_save_header_desc, /*time_basis*/ 0.0f );
         !r )
        return std::unexpected( r.error() );

    // Guard the header's declared counts before the read loops (reject-gracefully;
    // legacy trusts them straight into a loop bound, sv_save.c:993/999).  The
    // lightstyle bound is the writer-side cap (sv.lightstyles[MAX_LIGHTSTYLES],
    // server.h:146 / limits::server_lightstyles) — a larger declared count can
    // only come from a corrupt image and would otherwise drive an unbounded
    // vector allocation (terminate under /EHs-c-) before the read loop fails.
    if ( out.header.connection_count < 0 || out.header.light_style_count < 0 ||
         static_cast<std::size_t>( out.header.connection_count ) > k_max_level_connections ||
         static_cast<std::size_t>( out.header.light_style_count ) >
             ::xash::limits::server_lightstyles )
        return std::unexpected( SaveError::CorruptHeader );

    // --- 6. ADJACENCY x connectionCount (sv_save.c:993-994).  gAdjacency carries
    //        no FIELD_TIME field, so the legacy basis (header.time here) is inert;
    //        read with the writer's basis 0 for a self-consistent round trip. ---
    out.connections.assign( static_cast<std::size_t>( out.header.connection_count ),
                            ::xash::abi::LEVELLIST{} );
    for ( auto &conn : out.connections )
    {
        if ( auto r = read_descriptor_block( buffer, offset, *out.tokens, "ADJACENCY", &conn,
                                             k_adjacency_desc, /*time_basis*/ 0.0f );
             !r )
            return std::unexpected( r.error() );
    }

    // --- 7. LIGHTSTYLE x lightStyleCount (sv_save.c:999-1003).  SAVE_LIGHTSTYLE.
    //        time is FIELD_FLOAT (not FIELD_TIME) -> never rebased. ---
    out.lightstyles.assign( static_cast<std::size_t>( out.header.light_style_count ),
                            SaveLightStyle{} );
    for ( auto &ls : out.lightstyles )
    {
        if ( auto r = read_descriptor_block( buffer, offset, *out.tokens, "LIGHTSTYLE", &ls,
                                             k_light_style_desc, /*time_basis*/ 0.0f );
             !r )
            return std::unexpected( r.error() );
    }

    // --- 8. `offset` now sits at the first per-entity payload (== the data-region
    //        base + the first valid entity's ENTITYTABLE.location).  Record it
    //        relative to the data-region base. ---
    out.entity_data_offset = offset - out.table_size;
    out.table              = table_;
    out.buffer             = buf_;
    return {};
}

// ---------------------------------------------------------------------------
// LevelStateLoader::restore_entities
// ---------------------------------------------------------------------------

Result<void>
LevelStateLoader::restore_entities( LevelState &state, IEntityRestorer &restorer,
                                    const RestoreConfig &config ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    EntityTable &table = *table_;

    // pSaveData->time = header.time (sv_save.c:989) is the FIELD_TIME rebase basis
    // the game DLL applies inside pfnRestore (deep-dive §7 "Time rebasing").
    const float time_basis = state.header.time;

    // --- Phase 1: CreateEntitiesInRestoreList (sv_save.c:1410-1460), create_world
    //     == true, levelMask == 0.  A physint override replaces the whole loop
    //     (:1417-1419); the default seam returns false so we run the contract. ---
    if ( !restorer.create_entities_override( table, config ) )
    {
        for ( std::size_t i = 0; i < table.count(); ++i )
        {
            ::xash::abi::ENTITYTABLE &row       = table.row( i );
            const std::string_view    classname = table.classname( i );
            ::xash::abi::edict_t     *pent      = nullptr;

            // Row guard (sv_save.c:1428): classname && size && !REMOVED (the
            // `|| !create_world` term is vacuous here — create_world is true).
            const bool removed = ( static_cast<unsigned>( row.flags ) &
                                   ::xash::abi::k_fenttable_removed ) != 0;
            if ( !classname.empty() && row.size != 0 && !removed )
            {
                // active == 1 for create_world (sv_save.c:1432).
                if ( row.id == 0 ) // worldspawn (:1434-1438)
                {
                    pent = restorer.create_entity( RestoreCreateKind::World, row.id, classname );
                }
                else if ( row.id > 0 && row.id < config.max_clients + 1 ) // client (:1440-1449)
                {
                    // FENTTABLE_PLAYER mismatch (:1444-1445): legacy prints
                    // S_ERROR "ENTITY IS NOT A PLAYER" and DOES NOT skip — client
                    // creation still proceeds.  Diagnostic only.
                    if ( ( static_cast<unsigned>( row.flags ) &
                           ::xash::abi::k_fenttable_player ) == 0 )
                        ::xash::core::logf( ::xash::core::LogLevel::Error, k_tag,
                                            "ENTITY IS NOT A PLAYER: %zu", i );
                    pent = restorer.create_entity( RestoreCreateKind::Client, row.id, classname );
                }
                else // named creation (:1451-1453)
                {
                    pent = restorer.create_entity( RestoreCreateKind::Named, row.id, classname );
                }
            }

            row.pent = pent; // sv_save.c:1457
        }
    }

    // --- Phase 2: the LoadGameState spawn loop (sv_save.c:1664-1685). ---
    for ( std::size_t i = 0; i < table.count(); ++i )
    {
        ::xash::abi::ENTITYTABLE &row  = table.row( i );
        ::xash::abi::edict_t     *pent = row.pent;
        if ( pent == nullptr )
            continue; // (:1672) — only rows with a live edict are restored

        // Position: pCurrentData = pBaseData + location; the bounded window is
        // [location, location+size) within the data region (:1667-1668).  Reject
        // an out-of-range window (reject-gracefully; legacy computes the pointer
        // with no check, deep-dive "Untrusted-input asymmetry").
        if ( row.location < 0 || row.size < 0 )
            return std::unexpected( SaveError::CorruptHeader );
        auto view = buf_->view_at( state.table_size + static_cast<std::size_t>( row.location ),
                                   static_cast<std::size_t>( row.size ) );
        if ( !view )
            return std::unexpected( SaveError::CorruptHeader ); // oversize location/size

        // pfnRestore (sv_save.c:1674); currentIndex == i (:1669); shouldPrecache 0.
        const int rc = restorer.restore_entity( pent, i, *view, *state.tokens, time_basis );
        if ( rc < 0 )
        {
            // pfnRestore < 0 => FL_KILLME set, table pointer nulled (:1676-1677).
            // SetBits(pent->v.flags, FL_KILLME): both operands are `int`, so the
            // bit-set needs no width cast (k_fl_killme == 1<<30, positive).
            pent->v.flags |= ::xash::abi::k_fl_killme;
            row.pent = nullptr;
        }
    }

    return {};
}

} // namespace xash::save
