// xash3dpp — SaveGameState `.HL1` per-level writer core (Chunk 8, slice S8.3).
// See level_state_writer.hpp for the derived `.HL1` layout + sv_save.c cites.

#include <xash3dpp/private/save/level_state_writer.hpp>

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/format.hpp>

#include <xash3dpp/abi/server_consts.hpp> // k_fl_client
#include <xash3dpp/core/thread_role.hpp>

#include <cstring>

namespace xash::save {

namespace
{
// SV_IsValidEdict core (sv_save.c): a table row is saved only for a live edict.
[[nodiscard]] bool is_valid_edict( const ::xash::abi::edict_t *e ) noexcept
{
    return e != nullptr && e->free == 0;
}

// Clean fixed-width char copy: zero-fill then copy the truncated string (a
// deterministic replacement for legacy's stack-garbage Q_strncpy tail — see
// the parity note in write()).  Always NUL-terminated within `n`.
void copy_fixed( char *dst, std::size_t n, std::string_view src ) noexcept
{
    std::memset( dst, 0, n );
    const std::size_t count = ( src.size() < n ) ? src.size() : ( n - 1 );
    if ( count > 0 )
        std::memcpy( dst, src.data(), count );
}

void append_i32_le( std::vector<std::byte> &out, std::int32_t v ) noexcept
{
    const auto u = static_cast<std::uint32_t>( v );
    out.push_back( static_cast<std::byte>( u & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 8 ) & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 16 ) & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 24 ) & 0xFFu ) );
}

[[nodiscard]] std::int32_t read_i32_le( std::span<const std::byte> b ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}
} // namespace

// ---------------------------------------------------------------------------
// parse_hl1_preamble
// ---------------------------------------------------------------------------

Result<Hl1Preamble> parse_hl1_preamble( std::span<const std::byte> image ) noexcept
{
    if ( image.size() < k_hl1_preamble_bytes )
        return std::unexpected( SaveError::TruncatedBlock );

    const std::int32_t id      = read_i32_le( image.subspan( 0, 4 ) );
    const std::int32_t version = read_i32_le( image.subspan( 4, 4 ) );
    if ( id != k_savefile_magic )
        return std::unexpected( SaveError::BadMagic );
    if ( version != k_savegame_version )
        return std::unexpected( SaveError::VersionMismatch );

    Hl1Preamble p;
    p.size        = read_i32_le( image.subspan( 8, 4 ) );
    p.table_count = read_i32_le( image.subspan( 12, 4 ) );
    p.token_count = read_i32_le( image.subspan( 16, 4 ) );
    p.token_size  = read_i32_le( image.subspan( 20, 4 ) );

    // Bounds the real legacy load path omits (deep-dive "Untrusted-input
    // asymmetry") but SV_GetSaveComment enforces — the rewrite rejects rather
    // than trusting attacker-controlled counts into an allocation/read.
    if ( p.size < 0 || p.table_count < 0 || p.token_count < 0 || p.token_size < 0 )
        return std::unexpected( SaveError::CorruptHeader );
    if ( static_cast<std::size_t>( p.token_count ) > ::xash::limits::save_hash_strings ||
         static_cast<std::size_t>( p.token_size ) > ::xash::limits::save_heap_size ||
         static_cast<std::size_t>( p.size ) > ::xash::limits::save_heap_size )
        return std::unexpected( SaveError::CorruptHeader );

    // The declared regions must fit: preamble + tokens + (ETABLE+data == size).
    const std::size_t need = k_hl1_preamble_bytes +
                             static_cast<std::size_t>( p.token_size ) +
                             static_cast<std::size_t>( p.size );
    if ( need > image.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    return p;
}

// ---------------------------------------------------------------------------
// LevelStateWriter::write
// ---------------------------------------------------------------------------

Result<void>
LevelStateWriter::write( const LevelStateParams &params, std::vector<std::byte> &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    SaveBuffer  &buf   = *buf_;
    EntityTable &table = *table_;

    buf.reset(); // SaveClear (sv_save.c:742-754) — reusable working buffer

    // --- InitEntityTable (sv_save.c:1492) + pent population (save-boundary.md
    //     pent bullet: the writer sets row.pent from the arena BEFORE pfnSave). ---
    table.init( params.edicts.size() );
    for ( std::size_t i = 0; i < params.edicts.size(); ++i )
        table.row( i ).pent = params.edicts[i];

    SaveBufferSink sink( buf );
    TokenTable    &tokens = buf.tokens();

    // --- SAVE_HEADER struct (sv_save.c:1498-1517).  Zero-initialised so the
    //     char-array tails are deterministic zeros; legacy leaves stack garbage
    //     past each string's NUL (Q_strncpy does not pad) — readers stop at the
    //     NUL, so this is a semantically-equivalent, byte-clean deviation. ---
    SaveHeader header{};
    header.skill_level      = params.skill_level;
    header.entity_count     = static_cast<int>( table.count() ); // == tableCount
    header.connection_count = static_cast<int>( params.connections.size() );
    header.time             = params.time;
    copy_fixed( header.map_name, sizeof( header.map_name ), params.map_name );
    copy_fixed( header.sky_name, sizeof( header.sky_name ), params.sky_name );
    header.sky_color_r = params.sky_color_r;
    header.sky_color_g = params.sky_color_g;
    header.sky_color_b = params.sky_color_b;
    header.sky_vec_x   = params.sky_vec_x;
    header.sky_vec_y   = params.sky_vec_y;
    header.sky_vec_z   = params.sky_vec_z;

    // Count non-empty lightstyles (sv_save.c:1513-1517) for lightStyleCount.
    int light_style_count = 0;
    for ( const auto &ls : params.lightstyles )
        if ( !ls.pattern.empty() )
            ++light_style_count;
    header.light_style_count = light_style_count;

    // --- Write Save Header block (sv_save.c:1520-1522): pSaveData->time is 0
    //     while this block is written so FIELD_TIME is NOT rebased. ---
    if ( auto r = write_descriptor_block( sink, tokens, "Save Header", &header,
                                          k_save_header_desc, /*time_basis*/ 0.0f );
         !r )
        return r;

    // --- Adjacency list (sv_save.c:1525-1526). ---
    for ( const auto &conn : params.connections )
    {
        if ( auto r = write_descriptor_block( sink, tokens, "ADJACENCY", &conn,
                                              k_adjacency_desc, 0.0f, params.edict_index,
                                              params.edict_index_ctx );
             !r )
            return r;
    }

    // --- Lightstyles (sv_save.c:1529-1538). ---
    for ( const auto &ls : params.lightstyles )
    {
        if ( ls.pattern.empty() )
            continue; // sv_save.c:1531-1532

        SaveLightStyle light{};
        light.index = ls.index;
        copy_fixed( light.style, sizeof( light.style ), ls.pattern );
        light.time = ls.time;

        if ( auto r = write_descriptor_block( sink, tokens, "LIGHTSTYLE", &light,
                                              k_light_style_desc, 0.0f );
             !r )
            return r;
    }

    // --- Per-entity data (sv_save.c:1545-1559).  The engine sets location/size
    //     around each pfnSave call; DispatchSave (the saver stand-in) writes the
    //     payload + owns the classname; FL_CLIENT tags FENTTABLE_PLAYER. ---
    for ( std::size_t i = 0; i < table.count(); ++i )
    {
        ::xash::abi::ENTITYTABLE &row = table.row( i );
        row.location = static_cast<int>( buf.size() ); // sv_save.c:1548
        row.size     = 0;                              // sv_save.c:1550
        // pSaveData->currentIndex = i (sv_save.c:1549) is passed to the saver as
        // `table_index` directly; the ABI-window projection is a later slice.

        if ( !is_valid_edict( row.pent ) || params.saver == nullptr )
            continue; // sv_save.c:1552-1553

        // DispatchSave: pTable->classname = pEntity->pev->classname — the caller
        // pre-resolved the text (string pool is server-core, deferred).
        table.set_classname( i, ( i < params.classnames.size() )
                                    ? params.classnames[i]
                                    : std::string_view{} );

        if ( auto r = params.saver->save_entity( i, row.pent, sink, tokens ); !r )
            return r;

        // DispatchSave: pTable->size = pSaveData->size - pTable->location.
        row.size = static_cast<int>( buf.size() ) - row.location;

        if ( ( static_cast<unsigned>( row.pent->v.flags ) &
               static_cast<unsigned>( ::xash::abi::k_fl_client ) ) != 0 )
            row.flags |= static_cast<int>( ::xash::abi::k_fenttable_player ); // sv_save.c:1557-1558
    }

    const std::size_t data_size = buf.size(); // sv_save.c:1566 (dataSize)

    // --- ETABLE region (sv_save.c:1568-1574): appended AFTER the data region in
    //     the buffer; the file reorders it BEFORE the data region. ---
    if ( auto r = table.serialize( sink, tokens ); !r )
        return r;
    const std::size_t table_size = buf.size() - data_size; // sv_save.c:1574

    // --- Token blob (StoreHashTable, sv_save.c:1577/792-814): flatten AFTER all
    //     names are interned; tokenCount == the table's slot count. ---
    const std::size_t token_size = tokens.flattened_size();
    std::vector<std::byte> token_blob( token_size );
    if ( auto r = tokens.flatten( token_blob ); !r )
        return std::unexpected( r.error() );

    // --- Assemble the file image: preamble, tokens, ETABLE, data (the reorder
    //     at sv_save.c:1593-1603). ---
    const std::span<const std::byte> buffer   = buf.data(); // [data][ETABLE]
    const std::span<const std::byte> data_reg = buffer.subspan( 0, data_size );
    const std::span<const std::byte> etbl_reg = buffer.subspan( data_size, table_size );

    out.clear();
    out.reserve( k_hl1_preamble_bytes + token_size + table_size + data_size );
    append_i32_le( out, k_savefile_magic );                              // :1590,1593
    append_i32_le( out, k_savegame_version );                            // :1589,1594
    append_i32_le( out, static_cast<std::int32_t>( data_size + table_size ) ); // size :1597
    append_i32_le( out, static_cast<std::int32_t>( table.count() ) );    // tableCount :1598
    append_i32_le( out, static_cast<std::int32_t>( tokens.token_count() ) ); // tokenCount :1599
    append_i32_le( out, static_cast<std::int32_t>( token_size ) );       // tokenSize :1600
    out.insert( out.end(), token_blob.begin(), token_blob.end() );       // :1601
    out.insert( out.end(), etbl_reg.begin(), etbl_reg.end() );           // :1602
    out.insert( out.end(), data_reg.begin(), data_reg.end() );           // :1603

    return {};
}

} // namespace xash::save
