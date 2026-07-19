// xash3dpp — LoadGameState `.HL1` per-level loader tests (Chunk 8, slice S8.4).
// Covers LevelStateLoader: the full `.HL1` image parse (preamble + token rebuild
// + ETABLE region with tableSize recompute + Save Header / ADJACENCY /
// LIGHTSTYLE reads), the data-region positioning, and the IEntityRestorer
// recreation + per-entity restore CONTRACT (id-dispatched create kind, the
// FENTTABLE_PLAYER client requirement + its non-skipping mismatch, FIELD_TIME
// rebase via header.time, pfnRestore<0 => FL_KILLME + pent=NULL).  Legacy
// reference: engine/server/sv_save.c :896-1004 (LoadSaveData/ParseSaveTables),
// :1410-1460 (CreateEntitiesInRestoreList), :1628-1695 (LoadGameState).
//
// Coverage strategy: the writer (S8.3) is the golden producer for the round-trip
// tests (WRITE->LOAD closure on a 3-entity level + the empty case), plus one
// hand-built witness that feeds the S8.3-derived byte image the loader did NOT
// itself produce in this test (extends the S8.2 "reads a GoldSrc-written stream"
// pattern to the whole file).  Positioning + FIELD_TIME rebase are proven by a
// FakeSaver that writes a distinct float per entity and a FakeRestorer that
// reads it back and applies the injected time basis.

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_loader.hpp>
#include <xash3dpp/private/save/level_state_writer.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

static float read_f32_le( std::span<const std::byte> p ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
    return std::bit_cast<float>( u );
}

static void write_f32_le( std::byte *p, float v ) noexcept
{
    const auto u = std::bit_cast<std::uint32_t>( v );
    p[0] = static_cast<std::byte>( u & 0xFFu );
    p[1] = static_cast<std::byte>( ( u >> 8 ) & 0xFFu );
    p[2] = static_cast<std::byte>( ( u >> 16 ) & 0xFFu );
    p[3] = static_cast<std::byte>( ( u >> 24 ) & 0xFFu );
}

template<std::size_t N>
static void set_arr( char ( &dst )[N], std::string_view s ) noexcept
{
    std::memset( dst, 0, N );
    const std::size_t n = ( s.size() < N ) ? s.size() : ( N - 1 );
    if ( n )
        std::memcpy( dst, s.data(), n );
}

// ---------------------------------------------------------------------------
// Seams: a per-entity float saver (write side) + a recording restorer (read).
// ---------------------------------------------------------------------------

// Writes ONE field record per entity — name "edata", payload = float(base +
// table_index).  The distinct value per entity lets the restorer prove BOTH the
// data-region positioning (raw == base + i) and the FIELD_TIME rebase (raw +
// time_basis) from the same record, exactly as a real game-DLL FIELD_TIME field.
struct FloatSaver final : save::IEntitySaver
{
    float base;
    explicit FloatSaver( float b ) noexcept : base( b ) {}

    save::Result<void> save_entity( std::size_t table_index, const abi::edict_t *,
                                    save::IFieldSink &sink, save::TokenTable &tokens ) noexcept override
    {
        auto tok = tokens.insert( "edata" );
        if ( !tok )
            return std::unexpected( tok.error() );
        std::array<std::byte, 4> payload{};
        write_f32_le( payload.data(), base + static_cast<float>( table_index ) );
        return sink.write_field_record( *tok, payload );
    }
};

struct RCall
{
    bool                    is_create = false;
    // create:
    save::RestoreCreateKind kind = save::RestoreCreateKind::Named;
    int                     id   = 0;
    std::string             classname;
    // restore:
    std::size_t             table_index = 0;
    std::size_t             data_size   = 0;
    float                   time_basis  = 0.0f;
    float                   raw_time    = 0.0f; // float decoded from the entity window
    float                   rebased     = 0.0f; // raw_time + time_basis
    abi::edict_t           *pent        = nullptr;
};

// The pfnRestore + CreateEntitiesInRestoreList stand-in.  Records every call in
// order, hands out stable edict pointers from an owned pool, and reads the
// FloatSaver's record back to witness positioning + time rebasing.
struct FakeRestorer final : save::IEntityRestorer
{
    std::array<abi::edict_t, 16> pool{};
    std::size_t                  created = 0;
    std::vector<RCall>           calls;

    int fail_index      = -1; // restore_entity returns -1 for this table index (kill)
    int refuse_client_id = -1; // create_entity returns nullptr for this Client id (invalid slot)

    abi::edict_t *create_entity( save::RestoreCreateKind kind, int id,
                                 std::string_view classname ) noexcept override
    {
        RCall c;
        c.is_create = true;
        c.kind      = kind;
        c.id        = id;
        c.classname = std::string( classname );

        if ( kind == save::RestoreCreateKind::Client && id == refuse_client_id )
        {
            c.pent = nullptr;
            calls.push_back( c );
            return nullptr; // legacy `active && SV_IsValidEdict(ed)` == false
        }
        abi::edict_t *p = &pool[created++];
        c.pent          = p;
        calls.push_back( c );
        return p;
    }

    int restore_entity( abi::edict_t *pent, std::size_t table_index,
                        std::span<const std::byte> entity_data, const save::TokenTable &,
                        float time_basis ) noexcept override
    {
        RCall c;
        c.is_create   = false;
        c.table_index = table_index;
        c.data_size   = entity_data.size();
        c.time_basis  = time_basis;
        c.pent        = pent;

        // Decode the FloatSaver's single field record (game-DLL FIELD_TIME view).
        std::size_t off = 0;
        auto        rec = save::next_field_record( entity_data, off );
        if ( rec.has_value() && rec->payload.size() == 4 )
        {
            c.raw_time = read_f32_le( rec->payload );
            c.rebased  = c.raw_time + time_basis;
        }
        calls.push_back( c );
        return ( static_cast<int>( table_index ) == fail_index ) ? -1 : 0;
    }
};

// ---------------------------------------------------------------------------
// 3-entity WRITE(S8.3) -> LOAD(S8.4) round trip + restore contract in ORDER
// ---------------------------------------------------------------------------
//
// world (id 0, "worldspawn") + player-flagged (id 1, FL_CLIENT, "player") +
// named (id 2, "func_door").  maxclients = 1 -> id 1 is the client slot, id 2 is
// a named entity.  Each entity's payload is float(100 + i); header.time = 30.
static void test_three_entity_roundtrip()
{
    abi::edict_t e0{}, e1{}, e2{};
    e1.v.flags = abi::k_fl_client; // FENTTABLE_PLAYER on row 1
    std::array<abi::edict_t *, 3>   edicts  = { &e0, &e1, &e2 };
    std::array<std::string_view, 3> classes = { "worldspawn", "player", "func_door" };

    abi::LEVELLIST conn{};
    set_arr( conn.mapName, "c1a1" );
    set_arr( conn.landmarkName, "lm1" );
    conn.vecLandmarkOrigin[0] = 1.0f;
    conn.vecLandmarkOrigin[1] = 2.0f;
    conn.vecLandmarkOrigin[2] = 3.0f;
    conn.pentLandmark         = nullptr;

    std::array<save::LightStyleInput, 2> lights = { { { 1, "m", 5.0f }, { 2, "a", 6.0f } } };

    FloatSaver saver( 100.0f );

    save::LevelStateParams p;
    p.skill_level = 2;
    p.time        = 30.0f;
    p.map_name    = "c1a0";
    p.sky_name    = "desert";
    p.sky_color_r = 10;
    p.sky_color_g = 20;
    p.sky_color_b = 30;
    p.sky_vec_x   = 0.5f;
    p.sky_vec_y   = 0.25f;
    p.sky_vec_z   = 0.75f;
    p.connections = std::span<const abi::LEVELLIST>( &conn, 1 );
    p.lightstyles = lights;
    p.edicts      = edicts;
    p.classnames  = classes;
    p.saver       = &saver;

    // --- WRITE ---
    auto wbuf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );

    // --- LOAD --- (a fresh buffer + table, proving no writer state leaks in)
    auto lbuf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );

    save::LevelState state;
    REQUIRE( loader.load( image, state ).has_value() );

    // Header — every field.
    CHECK_EQ( state.header.skill_level, 2 );
    CHECK_EQ( state.header.entity_count, 3 );
    CHECK_EQ( state.header.connection_count, 1 );
    CHECK_EQ( state.header.light_style_count, 2 );
    CHECK( state.header.time == 30.0f );
    CHECK_STREQ( state.header.map_name, "c1a0" );
    CHECK_STREQ( state.header.sky_name, "desert" );
    CHECK_EQ( state.header.sky_color_r, 10 );
    CHECK_EQ( state.header.sky_color_g, 20 );
    CHECK_EQ( state.header.sky_color_b, 30 );
    CHECK( state.header.sky_vec_x == 0.5f );
    CHECK( state.header.sky_vec_y == 0.25f );
    CHECK( state.header.sky_vec_z == 0.75f );

    // Connections.
    REQUIRE( state.connections.size() == 1u );
    CHECK_STREQ( state.connections[0].mapName, "c1a1" );
    CHECK_STREQ( state.connections[0].landmarkName, "lm1" );
    CHECK( state.connections[0].vecLandmarkOrigin[0] == 1.0f );
    CHECK( state.connections[0].vecLandmarkOrigin[1] == 2.0f );
    CHECK( state.connections[0].vecLandmarkOrigin[2] == 3.0f );
    CHECK( state.connections[0].pentLandmark == nullptr );

    // Lightstyles.
    REQUIRE( state.lightstyles.size() == 2u );
    CHECK_EQ( state.lightstyles[0].index, 1 );
    CHECK_STREQ( state.lightstyles[0].style, "m" );
    CHECK( state.lightstyles[0].time == 5.0f );
    CHECK_EQ( state.lightstyles[1].index, 2 );
    CHECK_STREQ( state.lightstyles[1].style, "a" );
    CHECK( state.lightstyles[1].time == 6.0f );

    // ETABLE rows (the borrowed table is populated).
    REQUIRE( ltable.count() == 3u );
    CHECK_EQ( ltable.row( 0 ).id, 0 );
    CHECK( ltable.classname( 0 ) == "worldspawn" );
    CHECK_EQ( ltable.row( 1 ).id, 1 );
    CHECK( ltable.classname( 1 ) == "player" );
    CHECK( ( static_cast<unsigned>( ltable.row( 1 ).flags ) & abi::k_fenttable_player ) != 0 );
    CHECK_EQ( ltable.row( 2 ).id, 2 );
    CHECK( ltable.classname( 2 ) == "func_door" );
    CHECK( ( static_cast<unsigned>( ltable.row( 0 ).flags ) & abi::k_fenttable_player ) == 0 );

    // The first per-entity payload sits at the first entity's ETABLE.location.
    CHECK_EQ( state.entity_data_offset, static_cast<std::size_t>( ltable.row( 0 ).location ) );

    // --- RESTORE contract ---
    FakeRestorer restorer;
    REQUIRE( loader.restore_entities( state, restorer, save::RestoreConfig{ /*max_clients*/ 1 } )
                 .has_value() );

    // Two phases in legacy order: 3 creates, then 3 restores.
    REQUIRE( restorer.calls.size() == 6u );

    CHECK( restorer.calls[0].is_create );
    CHECK( restorer.calls[0].kind == save::RestoreCreateKind::World );
    CHECK_EQ( restorer.calls[0].id, 0 );
    CHECK( restorer.calls[0].classname == "worldspawn" );

    CHECK( restorer.calls[1].is_create );
    CHECK( restorer.calls[1].kind == save::RestoreCreateKind::Client );
    CHECK_EQ( restorer.calls[1].id, 1 );
    CHECK( restorer.calls[1].classname == "player" );

    CHECK( restorer.calls[2].is_create );
    CHECK( restorer.calls[2].kind == save::RestoreCreateKind::Named );
    CHECK_EQ( restorer.calls[2].id, 2 );
    CHECK( restorer.calls[2].classname == "func_door" );

    // Restores: right table_index, size (8 = one float record), time basis
    // (== header.time), and the positioned float (100 + i, proving each entity's
    // window was located correctly).
    for ( std::size_t i = 0; i < 3; ++i )
    {
        const RCall &r = restorer.calls[3 + i];
        CHECK( !r.is_create );
        CHECK_EQ( r.table_index, i );
        CHECK_EQ( r.data_size, 8u );
        CHECK( r.time_basis == 30.0f );
        CHECK( r.raw_time == 100.0f + static_cast<float>( i ) );   // positioning
        CHECK( r.rebased == 130.0f + static_cast<float>( i ) );    // FIELD_TIME rebase
    }

    // No entity was killed (all restores returned 0): every row keeps its pent.
    CHECK( ltable.row( 0 ).pent != nullptr );
    CHECK( ltable.row( 1 ).pent != nullptr );
    CHECK( ltable.row( 2 ).pent != nullptr );
}

// ---------------------------------------------------------------------------
// The client-slot FENTTABLE_PLAYER mismatch (sv_save.c:1440-1449)
// ---------------------------------------------------------------------------
//
// A row whose id is in the client range [1, maxclients] but WITHOUT
// FENTTABLE_PLAYER: legacy prints S_ERROR "ENTITY IS NOT A PLAYER" and DOES NOT
// skip — Client creation still proceeds.  Here row 1 is a NON-client entity
// (no FL_CLIENT), so the writer never sets FENTTABLE_PLAYER on it.
static void test_client_id_player_mismatch()
{
    abi::edict_t e0{}, e1{}; // e1 is NOT a client
    std::array<abi::edict_t *, 2>   edicts  = { &e0, &e1 };
    std::array<std::string_view, 2> classes = { "worldspawn", "monster_x" };

    FloatSaver             saver( 0.0f );
    save::LevelStateParams p;
    p.time       = 1.0f;
    p.map_name   = "m";
    p.edicts     = edicts;
    p.classnames = classes;
    p.saver      = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );

    auto lbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    REQUIRE( loader.load( image, state ).has_value() );

    // Row 1 (id 1) carries no FENTTABLE_PLAYER.
    CHECK( ( static_cast<unsigned>( ltable.row( 1 ).flags ) & abi::k_fenttable_player ) == 0 );

    FakeRestorer restorer;
    REQUIRE( loader.restore_entities( state, restorer, save::RestoreConfig{ 1 } ).has_value() );

    // Despite the mismatch, id 1 was STILL created as a Client (proceed, not
    // skip).  calls[0] = world, calls[1] = the client creation for id 1.
    REQUIRE( restorer.calls.size() >= 2u );
    CHECK( restorer.calls[1].is_create );
    CHECK( restorer.calls[1].kind == save::RestoreCreateKind::Client );
    CHECK_EQ( restorer.calls[1].id, 1 );
    CHECK( restorer.calls[1].classname == "monster_x" );
    CHECK( ltable.row( 1 ).pent != nullptr ); // creation proceeded
}

// ---------------------------------------------------------------------------
// pfnRestore < 0 => FL_KILLME set + table pointer nulled (sv_save.c:1674-1678)
// ---------------------------------------------------------------------------
static void test_restore_kill_semantics()
{
    abi::edict_t e0{}, e1{}, e2{};
    e1.v.flags = abi::k_fl_client;
    std::array<abi::edict_t *, 3>   edicts  = { &e0, &e1, &e2 };
    std::array<std::string_view, 3> classes = { "worldspawn", "player", "func_door" };

    FloatSaver             saver( 0.0f );
    save::LevelStateParams p;
    p.time       = 7.0f;
    p.map_name   = "m";
    p.edicts     = edicts;
    p.classnames = classes;
    p.saver      = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );

    auto lbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    REQUIRE( loader.load( image, state ).has_value() );

    FakeRestorer restorer;
    restorer.fail_index = 1; // pfnRestore returns < 0 for table index 1
    REQUIRE( loader.restore_entities( state, restorer, save::RestoreConfig{ 1 } ).has_value() );

    // Row 1's edict was FL_KILLME'd and its table pointer nulled; the others are
    // untouched (rc >= 0).
    const RCall *killed = nullptr;
    for ( const auto &c : restorer.calls )
        if ( !c.is_create && c.table_index == 1 )
            killed = &c;
    REQUIRE( killed != nullptr );
    REQUIRE( killed->pent != nullptr );
    CHECK( ( static_cast<unsigned>( killed->pent->v.flags ) &
             static_cast<unsigned>( abi::k_fl_killme ) ) != 0 );
    CHECK( ltable.row( 1 ).pent == nullptr );

    // Rows 0 and 2 survived (still linked, no FL_KILLME).
    CHECK( ltable.row( 0 ).pent != nullptr );
    CHECK( ltable.row( 2 ).pent != nullptr );
    CHECK( ( static_cast<unsigned>( ltable.row( 0 ).pent->v.flags ) &
             static_cast<unsigned>( abi::k_fl_killme ) ) == 0 );
}

// ---------------------------------------------------------------------------
// FIELD_TIME rebase basis is header.time (not 0), verified independently
// ---------------------------------------------------------------------------
static void test_field_time_basis()
{
    abi::edict_t e0{};
    std::array<abi::edict_t *, 1>   edicts  = { &e0 };
    std::array<std::string_view, 1> classes = { "worldspawn" };

    FloatSaver             saver( 5.0f ); // entity 0 payload = float(5.0)
    save::LevelStateParams p;
    p.time       = 42.0f; // header.time -> the rebase basis
    p.map_name   = "m";
    p.edicts     = edicts;
    p.classnames = classes;
    p.saver      = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 2048, 24, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );

    auto lbuf = save::create_save_buffer( g_pool, 2048, 24, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    REQUIRE( loader.load( image, state ).has_value() );

    FakeRestorer restorer;
    REQUIRE( loader.restore_entities( state, restorer, save::RestoreConfig{ 1 } ).has_value() );

    REQUIRE( restorer.calls.size() == 2u ); // 1 create + 1 restore
    const RCall &r = restorer.calls[1];
    CHECK( !r.is_create );
    CHECK( r.time_basis == 42.0f );          // basis IS header.time, not 0
    CHECK( r.raw_time == 5.0f );             // stored value
    CHECK( r.rebased == 47.0f );             // 5.0 + 42.0 (a basis of 0 would give 5.0)
}

// ---------------------------------------------------------------------------
// Hand-built witness: the S8.3-derived 194-byte empty image (foreign source)
// ---------------------------------------------------------------------------
//
// These bytes are the exact hand-derived golden pinned in test_hl1_writer's
// EMPTY case (skill 0, time 1.0, mapName "t0", 0 conn, 0 lightstyles, 1 entity
// classname "a", entity field "x"=AABBCCDD).  Feeding them directly proves the
// loader reads a byte image it did not itself produce in THIS test — extending
// the S8.2 "reads a GoldSrc-written stream" witness to the full file.
static void test_hand_built_witness()
{
    const std::array<std::byte, 194> witness = {
        // preamble (24): VALV, 0x71, size=98, tableCount=1, tokenCount=11, tokenSize=72
        b( 0x56 ), b( 0x41 ), b( 0x4C ), b( 0x56 ), b( 0x71 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x62 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x0B ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x48 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        // token blob (72), slot order 0..10
        b( 0x65 ), b( 0x6E ), b( 0x74 ), b( 0x69 ), b( 0x74 ), b( 0x79 ), b( 0x43 ), b( 0x6F ),
        b( 0x75 ), b( 0x6E ), b( 0x74 ), b( 0x00 ), b( 0x6D ), b( 0x61 ), b( 0x70 ), b( 0x4E ),
        b( 0x61 ), b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x45 ), b( 0x54 ), b( 0x41 ), b( 0x42 ),
        b( 0x4C ), b( 0x45 ), b( 0x00 ), b( 0x6C ), b( 0x6F ), b( 0x63 ), b( 0x61 ), b( 0x74 ),
        b( 0x69 ), b( 0x6F ), b( 0x6E ), b( 0x00 ), b( 0x73 ), b( 0x69 ), b( 0x7A ), b( 0x65 ),
        b( 0x00 ), b( 0x63 ), b( 0x6C ), b( 0x61 ), b( 0x73 ), b( 0x73 ), b( 0x6E ), b( 0x61 ),
        b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x53 ), b( 0x61 ), b( 0x76 ),
        b( 0x65 ), b( 0x20 ), b( 0x48 ), b( 0x65 ), b( 0x61 ), b( 0x64 ), b( 0x65 ), b( 0x72 ),
        b( 0x00 ), b( 0x74 ), b( 0x69 ), b( 0x6D ), b( 0x65 ), b( 0x00 ), b( 0x78 ), b( 0x00 ),
        // ETABLE region (30): row0 { location=60, size=8, classname="a\0" }
        b( 0x04 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x3C ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x02 ), b( 0x00 ), b( 0x05 ), b( 0x00 ), b( 0x61 ), b( 0x00 ),
        // data region (68): Save Header block (60) + entity0 rec("x",AABBCCDD)
        b( 0x04 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x03 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x04 ), b( 0x00 ), b( 0x09 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x80 ), b( 0x3F ),
        b( 0x20 ), b( 0x00 ), b( 0x01 ), b( 0x00 ), b( 0x74 ), b( 0x30 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x0A ), b( 0x00 ),
        b( 0xAA ), b( 0xBB ), b( 0xCC ), b( 0xDD ),
    };

    auto lbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    REQUIRE( loader.load( witness, state ).has_value() );

    CHECK_EQ( state.header.entity_count, 1 );
    CHECK_EQ( state.header.connection_count, 0 );
    CHECK_EQ( state.header.light_style_count, 0 );
    CHECK_EQ( state.header.skill_level, 0 );
    CHECK( state.header.time == 1.0f );
    CHECK_STREQ( state.header.map_name, "t0" );
    CHECK( state.connections.empty() );
    CHECK( state.lightstyles.empty() );

    REQUIRE( ltable.count() == 1u );
    CHECK_EQ( ltable.row( 0 ).id, 0 );
    CHECK_EQ( ltable.row( 0 ).location, 60 );
    CHECK_EQ( ltable.row( 0 ).size, 8 );
    CHECK( ltable.classname( 0 ) == "a" );
    CHECK_EQ( state.entity_data_offset, 60u ); // first entity begins right after the header block

    // Drive the restore: the single row (id 0) is the world.
    FakeRestorer restorer;
    REQUIRE( loader.restore_entities( state, restorer, save::RestoreConfig{ 1 } ).has_value() );
    REQUIRE( restorer.calls.size() == 2u );
    CHECK( restorer.calls[0].is_create );
    CHECK( restorer.calls[0].kind == save::RestoreCreateKind::World );
    CHECK( restorer.calls[0].classname == "a" );
    CHECK( !restorer.calls[1].is_create );
    CHECK_EQ( restorer.calls[1].table_index, 0u );
    CHECK_EQ( restorer.calls[1].data_size, 8u ); // the "x"=AABBCCDD record
}

// ---------------------------------------------------------------------------
// The S8.3 empty-image golden loads (writer -> loader closure)
// ---------------------------------------------------------------------------
static void test_empty_golden_writer_roundtrip()
{
    abi::edict_t e0{};
    std::array<abi::edict_t *, 1>   edicts  = { &e0 };
    std::array<std::string_view, 1> classes = { "a" };

    // Same scene as test_hl1_writer::test_empty_full_image (field "x"=AABBCCDD).
    struct XSaver final : save::IEntitySaver
    {
        save::Result<void> save_entity( std::size_t, const abi::edict_t *, save::IFieldSink &sink,
                                        save::TokenTable &tokens ) noexcept override
        {
            auto tok = tokens.insert( "x" );
            if ( !tok )
                return std::unexpected( tok.error() );
            const std::array<std::byte, 4> payload = { b( 0xAA ), b( 0xBB ), b( 0xCC ), b( 0xDD ) };
            return sink.write_field_record( *tok, payload );
        }
    } saver;

    save::LevelStateParams p;
    p.time       = 1.0f;
    p.map_name   = "t0";
    p.edicts     = edicts;
    p.classnames = classes;
    p.saver      = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 1024, 11, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );
    REQUIRE( image.size() == 194u ); // the S8.3 empty golden

    auto lbuf = save::create_save_buffer( g_pool, 1024, 11, 0.0f );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    REQUIRE( loader.load( image, state ).has_value() );

    CHECK_EQ( state.header.entity_count, 1 );
    CHECK_STREQ( state.header.map_name, "t0" );
    CHECK( state.header.time == 1.0f );
    REQUIRE( ltable.count() == 1u );
    CHECK_EQ( ltable.row( 0 ).location, 60 );
    CHECK_EQ( ltable.row( 0 ).size, 8 );
    CHECK( ltable.classname( 0 ) == "a" );
}

// ---------------------------------------------------------------------------
// Corrupt cases -> specific SaveErrors
// ---------------------------------------------------------------------------
static void test_corrupt_cases()
{
    // Build one good empty image to mutate.
    abi::edict_t e0{};
    std::array<abi::edict_t *, 1>   edicts  = { &e0 };
    std::array<std::string_view, 1> classes = { "a" };
    FloatSaver                      saver( 0.0f );
    save::LevelStateParams          p;
    p.time = 1.0f; p.map_name = "t0"; p.edicts = edicts; p.classnames = classes; p.saver = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> good;
    REQUIRE( writer.write( p, good ).has_value() );

    // Sanity: the good image loads.
    {
        auto lbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
        REQUIRE( lbuf != nullptr );
        save::EntityTable      lt;
        save::LevelStateLoader ld( *lbuf, lt );
        save::LevelState       st;
        REQUIRE( ld.load( good, st ).has_value() );
    }

    // 1. Truncated ETABLE region: shrink the declared `size` (bytes 8..11) below
    //    one ETABLE block so the ETABLE read runs off the loaded buffer.
    {
        std::vector<std::byte> bad = good;
        bad[8] = b( 0x0A ); bad[9] = b( 0x00 ); bad[10] = b( 0x00 ); bad[11] = b( 0x00 ); // size = 10
        auto lbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
        REQUIRE( lbuf != nullptr );
        save::EntityTable      lt;
        save::LevelStateLoader ld( *lbuf, lt );
        save::LevelState       st;
        auto                   r = ld.load( bad, st );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }

    // 2. tableCount mismatch: claim 2 rows (bytes 12..15) where only 1 ETABLE
    //    block exists — the 2nd "ETABLE" block resolves to the Save Header block
    //    name -> CorruptHeader.
    {
        std::vector<std::byte> bad = good;
        bad[12] = b( 0x02 ); bad[13] = b( 0x00 ); bad[14] = b( 0x00 ); bad[15] = b( 0x00 );
        auto lbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
        REQUIRE( lbuf != nullptr );
        save::EntityTable      lt;
        save::LevelStateLoader ld( *lbuf, lt );
        save::LevelState       st;
        auto                   r = ld.load( bad, st );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::CorruptHeader );
    }

    // 3. Oversize ENTITYTABLE.location: a well-formed image whose entity window
    //    falls outside the data region -> CorruptHeader at restore time (legacy
    //    computes pBaseData+location with no bounds check).  Injected on the
    //    loaded table (the encoded location byte is buried in a field record).
    {
        auto lbuf = save::create_save_buffer( g_pool, 2048, 16, 0.0f );
        REQUIRE( lbuf != nullptr );
        save::EntityTable      lt;
        save::LevelStateLoader ld( *lbuf, lt );
        save::LevelState       st;
        REQUIRE( ld.load( good, st ).has_value() );

        lt.row( 0 ).location = 1000000; // far past the data region

        FakeRestorer restorer;
        auto         r = ld.restore_entities( st, restorer, save::RestoreConfig{ 1 } );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::CorruptHeader );
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "hl1_loader_test" );

    RUN_TEST( test_three_entity_roundtrip );
    RUN_TEST( test_client_id_player_mismatch );
    RUN_TEST( test_restore_kill_semantics );
    RUN_TEST( test_field_time_basis );
    RUN_TEST( test_hand_built_witness );
    RUN_TEST( test_empty_golden_writer_roundtrip );
    RUN_TEST( test_corrupt_cases );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "hl1_loader: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
