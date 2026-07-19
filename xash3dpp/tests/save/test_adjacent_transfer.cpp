// xash3dpp — landmark-transition machinery tests (Chunk 8, slice S8.6).
// Covers adjacent_transfer.hpp: the pure landmark/decal/mask math, the
// EntityInSolid port, CreateEntityTransitionList's levelMask selection matrix +
// the two restore branches (global merge, moveable transfer) + post-transfer
// pruning, and the LoadAdjacentEnts orchestration (dedup, foundprevious hard-
// fail, the FIELD_TIME sv.time quirk, landmark-offset delivery, .HL3 patch
// interplay + rewrite).  Legacy reference: engine/server/sv_save.c :452-489,
// :1836-1922, :1931-2015.
//
// Coverage strategy: the writer (S8.3) is the golden producer for each adjacent
// level's `.HL1` image; the loaded ENTITYTABLE's per-entity flags are then set
// directly (the writer only emits FENTTABLE_PLAYER from FL_CLIENT — the PVS/
// global bits are game-DLL-set at save time, so the matrix is driven by
// overlaying the flags a real adjacent save would carry, exactly as the S8.4
// loader test overrides a loaded row's location for its corrupt case).

#include <xash3dpp/private/save/adjacent_transfer.hpp>
#include <xash3dpp/private/save/entity_patch.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/field_sink.hpp>
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
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;

static int g_pass = 0, g_fail = 0;
static mem::PoolHandle g_pool;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

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
    p[0]         = static_cast<std::byte>( u & 0xFFu );
    p[1]         = static_cast<std::byte>( ( u >> 8 ) & 0xFFu );
    p[2]         = static_cast<std::byte>( ( u >> 16 ) & 0xFFu );
    p[3]         = static_cast<std::byte>( ( u >> 24 ) & 0xFFu );
}

template<std::size_t N>
static void set_arr( char ( &dst )[N], std::string_view s ) noexcept
{
    std::memset( dst, 0, N );
    const std::size_t n = ( s.size() < N ) ? s.size() : ( N - 1 );
    if ( n )
        std::memcpy( dst, s.data(), n );
}

static abi::LEVELLIST make_conn( std::string_view map, std::string_view landmark,
                                 float ox, float oy, float oz ) noexcept
{
    abi::LEVELLIST c{};
    set_arr( c.mapName, map );
    set_arr( c.landmarkName, landmark );
    c.vecLandmarkOrigin[0] = ox;
    c.vecLandmarkOrigin[1] = oy;
    c.vecLandmarkOrigin[2] = oz;
    c.pentLandmark         = nullptr;
    return c;
}

// One field record per entity: float(base + table_index) — a game-DLL FIELD_TIME
// stand-in, lets a restore witness the data window + time rebase (S8.4 pattern).
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

// Build one adjacent level `.HL1` image (writer golden).  `classes` sizes the
// entity set; `client` marks FL_CLIENT rows (FENTTABLE_PLAYER on the wire).
static std::vector<std::byte> make_hl1( std::string_view map, float time,
                                        std::span<const abi::LEVELLIST> conns,
                                        std::span<const std::string_view> classes,
                                        std::span<const bool>             client )
{
    std::vector<abi::edict_t>   ents( classes.size() );
    std::vector<abi::edict_t *> eptrs;
    for ( std::size_t i = 0; i < ents.size(); ++i )
    {
        ents[i] = abi::edict_t{};
        if ( i < client.size() && client[i] )
            ents[i].v.flags = abi::k_fl_client;
        eptrs.push_back( &ents[i] );
    }

    FloatSaver             saver( 100.0f );
    save::LevelStateParams p;
    p.time        = time;
    p.map_name    = map;
    p.connections = conns;
    p.edicts      = eptrs;
    p.classnames  = classes;
    p.saver       = &saver;

    auto wbuf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
    REQUIRE( wbuf != nullptr );
    save::EntityTable      wtable;
    save::LevelStateWriter writer( *wbuf, wtable );
    std::vector<std::byte> image;
    REQUIRE( writer.write( p, image ).has_value() );
    return image;
}

// ---------------------------------------------------------------------------
// Seams
// ---------------------------------------------------------------------------

struct TCall
{
    enum Kind
    {
        Create,
        Transfer,
        Merge
    } kind;
    // create
    save::RestoreCreateKind create_kind = save::RestoreCreateKind::Named;
    int                     id          = 0;
    std::string             classname;
    // transfer / merge
    std::size_t             table_index    = 0;
    std::size_t             window_size    = 0;
    float                   time_basis     = 0.0f;
    save::SaveVec3          landmark_offset{};
    bool                    use_landmark   = false;
    float                   decoded_float  = 0.0f;
    abi::edict_t           *pent           = nullptr;
};

// pfnRestore + CreateEntitiesInRestoreList (create_world=false) stand-in.
struct FakeTransitionRestorer final : save::ITransitionRestorer
{
    std::array<abi::edict_t, 32> pool{};
    std::size_t                  created = 0;
    std::vector<TCall>           calls;

    int          refuse_client_id = -1;  // create_entity(Client, this) -> nullptr (invalid slot)
    int          fail_transfer    = -1;  // transfer_entity returns -1 for this index (kill)
    int          stuck_index      = -1;  // transfer_entity parks this entity in solid
    bool         global_merges    = true; // merge_global_entity result
    abi::edict_t *global_repoint  = nullptr; // repoint target on a failed merge
    int          free_old_count   = 0;

    abi::edict_t *create_entity( save::RestoreCreateKind kind, int id,
                                 std::string_view classname ) noexcept override
    {
        TCall c;
        c.kind        = TCall::Create;
        c.create_kind = kind;
        c.id          = id;
        c.classname   = std::string( classname );
        if ( kind == save::RestoreCreateKind::Client && id == refuse_client_id )
        {
            c.pent = nullptr;
            calls.push_back( c );
            return nullptr;
        }
        abi::edict_t *p = &pool[created++];
        c.pent          = p;
        calls.push_back( c );
        return p;
    }

    int transfer_entity( abi::edict_t *pent, std::size_t idx, std::span<const std::byte> data,
                         const save::TokenTable &, const save::TransitionContext &ctx ) noexcept override
    {
        TCall c;
        c.kind            = TCall::Transfer;
        c.table_index     = idx;
        c.window_size     = data.size();
        c.time_basis      = ctx.time_basis;
        c.landmark_offset = ctx.landmark_offset;
        c.use_landmark    = ctx.use_landmark;
        c.pent            = pent;
        std::size_t off   = 0;
        auto        rec   = save::next_field_record( data, off );
        if ( rec.has_value() && rec->payload.size() == 4 )
            c.decoded_float = read_f32_le( rec->payload );
        calls.push_back( c );

        // Park the "stuck" entity so EntityInSolid (via FakeSolid) fires.
        if ( static_cast<int>( idx ) == stuck_index )
        {
            pent->v.absmin[0] = pent->v.absmax[0] = 1000.0f;
            pent->v.absmin[1] = pent->v.absmax[1] = 1000.0f;
            pent->v.absmin[2] = pent->v.absmax[2] = 1000.0f;
        }
        return ( static_cast<int>( idx ) == fail_transfer ) ? -1 : 0;
    }

    save::GlobalMergeResult
    merge_global_entity( abi::edict_t *pent, std::size_t idx, std::span<const std::byte> data,
                         const save::TokenTable &, const save::TransitionContext &ctx ) noexcept override
    {
        TCall c;
        c.kind            = TCall::Merge;
        c.table_index     = idx;
        c.window_size     = data.size();
        c.time_basis      = ctx.time_basis;
        c.landmark_offset = ctx.landmark_offset;
        c.use_landmark    = ctx.use_landmark;
        c.pent            = pent;
        calls.push_back( c );

        save::GlobalMergeResult r;
        r.merged         = global_merges;
        r.repoint_target = global_merges ? nullptr : global_repoint;
        return r;
    }

    void free_old_entities() noexcept override { ++free_old_count; }

    std::size_t count_kind( TCall::Kind k ) const noexcept
    {
        std::size_t n = 0;
        for ( const auto &c : calls )
            if ( c.kind == k )
                ++n;
        return n;
    }
    const TCall *find_index( TCall::Kind k, std::size_t idx ) const noexcept
    {
        for ( const auto &c : calls )
            if ( c.kind == k && c.table_index == idx )
                return &c;
        return nullptr;
    }
};

// SV_PointContents stand-in: solid iff the probed center is far out (x > 500).
struct FakeSolid final : save::ISolidTestProvider
{
    int                          last_group_mask = -12345;
    std::vector<save::SaveVec3>  points;
    bool point_in_solid( const save::SaveVec3 &point, int group_mask ) noexcept override
    {
        last_group_mask = group_mask;
        points.push_back( point );
        return point.x > 500.0f;
    }
};

// A whole two-level fixture source for the run() orchestration tests.
struct FakeSource final : save::IAdjacentLevelSource
{
    struct Entry
    {
        std::string            map;
        std::vector<std::byte> hl1;
        bool                   has_patch = false;
        std::vector<std::byte> hl3;
    };
    std::vector<Entry>                                          levels;
    std::vector<std::pair<std::string, std::vector<std::int32_t>>> writes;

    const Entry *find( std::string_view map ) const noexcept
    {
        for ( const auto &e : levels )
            if ( e.map == map )
                return &e;
        return nullptr;
    }

    std::optional<std::vector<std::byte>> load_level_image( std::string_view map ) noexcept override
    {
        if ( const Entry *e = find( map ) )
            return e->hl1;
        return std::nullopt;
    }
    std::optional<std::vector<std::byte>> load_entity_patch( std::string_view map ) noexcept override
    {
        if ( const Entry *e = find( map ); e && e->has_patch )
            return e->hl3;
        return std::nullopt;
    }
    save::Result<void> store_entity_patch( std::string_view map,
                                           const save::EntityTable &table ) noexcept override
    {
        std::vector<std::byte> bytes = save::write_entity_patch( table );
        auto                   idx   = save::read_entity_patch( bytes );
        REQUIRE( idx.has_value() );
        writes.emplace_back( std::string( map ), *idx );
        return {};
    }
};

// Load an image into a fresh (buf, table, loader) and return the LevelState.
struct Loaded
{
    std::unique_ptr<save::SaveBuffer> buf;
    save::EntityTable                 table;
    save::LevelState                  state;
};
static void load_image( Loaded &out, std::span<const std::byte> image )
{
    out.buf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
    REQUIRE( out.buf != nullptr );
    save::LevelStateLoader loader( *out.buf, out.table );
    REQUIRE( loader.load( image, out.state ).has_value() );
}

// ===========================================================================
// 1. Pure landmark math
// ===========================================================================
static void test_landmark_math()
{
    std::array<abi::LEVELLIST, 2> conns = {
        make_conn( "c1a1", "lm_a", 10.0f, 20.0f, 30.0f ),
        make_conn( "c1a2", "lm_b", -5.0f, 0.0f, 7.0f ),
    };

    // landmark_origin: exact match returns that connection's origin.
    const save::SaveVec3 a = save::landmark_origin( conns, "lm_a" );
    CHECK( a == ( save::SaveVec3{ 10.0f, 20.0f, 30.0f } ) );
    const save::SaveVec3 b = save::landmark_origin( conns, "lm_b" );
    CHECK( b == ( save::SaveVec3{ -5.0f, 0.0f, 7.0f } ) );

    // Missing landmark -> {0,0,0} (VectorClear).
    CHECK( save::landmark_origin( conns, "nope" ) == ( save::SaveVec3{} ) );

    // Landmark name match is CASE-SENSITIVE (legacy Q_strcmp) -> no match.
    CHECK( save::landmark_origin( conns, "LM_A" ) == ( save::SaveVec3{} ) );

    // compute_landmark_offset = origin(new) - origin(adjacent).
    std::array<abi::LEVELLIST, 1> newlvl = { make_conn( "old", "lm_a", 100.0f, 50.0f, 10.0f ) };
    const save::SaveVec3 off = save::compute_landmark_offset( newlvl, conns, "lm_a" );
    CHECK( off == ( save::SaveVec3{ 90.0f, 30.0f, -20.0f } ) ); // (100-10, 50-20, 10-30)
}

// ===========================================================================
// 2. compute_transition_mask
// ===========================================================================
static void test_compute_transition_mask()
{
    std::array<abi::LEVELLIST, 3> conns = {
        make_conn( "newmap", "lm0", 0, 0, 0 ), // slot 0 -> new map
        make_conn( "other", "lm1", 0, 0, 0 ),  // slot 1 -> unrelated
        make_conn( "NEWMAP", "lm2", 0, 0, 0 ), // slot 2 -> new map (case-insensitive)
    };

    // Not old level: only the back-connection BIT()s, case-insensitive on map.
    const int m = save::compute_transition_mask( conns, "newmap", /*is_old*/ false );
    CHECK_EQ( static_cast<unsigned>( m ), ( 1u << 0 ) | ( 1u << 2 ) );

    // Old level: FENTTABLE_PLAYER OR'd in on top.
    const int mo = save::compute_transition_mask( conns, "newmap", /*is_old*/ true );
    CHECK_EQ( static_cast<unsigned>( mo ),
              abi::k_fenttable_player | ( 1u << 0 ) | ( 1u << 2 ) );

    // No back-connection, not old: mask is empty.
    CHECK_EQ( save::compute_transition_mask( conns, "elsewhere", false ), 0 );

    // No back-connection but IS old: player bit only (still non-zero).
    CHECK_EQ( static_cast<unsigned>( save::compute_transition_mask( conns, "elsewhere", true ) ),
              abi::k_fenttable_player );
}

// ===========================================================================
// 3. Decal offset hook
// ===========================================================================
static void test_decal_offset()
{
    const save::SaveVec3 pos{ 100.0f, 200.0f, 300.0f };
    const save::SaveVec3 off{ 5.0f, -10.0f, 15.0f };

    // Gated ON: save subtracts, load adds; round-trips.
    const save::SaveVec3 saved =
        save::decal_offset_for_save( pos, off, /*use_landmark*/ true, save::k_fdecal_use_landmark );
    CHECK( saved == ( save::SaveVec3{ 95.0f, 210.0f, 285.0f } ) );
    const save::SaveVec3 loaded =
        save::decal_offset_for_load( saved, off, true, save::k_fdecal_use_landmark );
    CHECK( loaded == pos );

    // Gate OFF: no landmark -> unchanged either direction.
    CHECK( save::decal_offset_for_save( pos, off, false, save::k_fdecal_use_landmark ) == pos );
    CHECK( save::decal_offset_for_load( pos, off, false, save::k_fdecal_use_landmark ) == pos );

    // Gate OFF: landmark save but decal is NOT FDECAL_USE_LANDMARK -> unchanged.
    CHECK( save::decal_offset_for_save( pos, off, true, /*flags*/ 0 ) == pos );
    CHECK( save::decal_offset_for_load( pos, off, true, /*flags*/ 0 ) == pos );
}

// ===========================================================================
// 4. EntityInSolid
// ===========================================================================
static void test_entity_in_solid()
{
    FakeSolid solid;

    // Center of AABB is probed; group_mask forwarded.
    abi::edict_t e{};
    e.v.absmin[0] = 0.0f;  e.v.absmax[0] = 200.0f; // center x = 100 (not solid: <=500)
    e.v.absmin[1] = -4.0f; e.v.absmax[1] = 4.0f;   // center y = 0
    e.v.absmin[2] = 10.0f; e.v.absmax[2] = 30.0f;  // center z = 20
    e.v.groupinfo = 7;
    CHECK( !save::entity_in_solid( e, solid ) );
    REQUIRE( !solid.points.empty() );
    CHECK( solid.points.back() == ( save::SaveVec3{ 100.0f, 0.0f, 20.0f } ) );
    CHECK_EQ( solid.last_group_mask, 7 );

    // Far-out center -> solid.
    abi::edict_t stuck{};
    stuck.v.absmin[0] = stuck.v.absmax[0] = 1000.0f;
    CHECK( save::entity_in_solid( stuck, solid ) );

    // MOVETYPE_FOLLOW attached to a client -> always go through (no probe).
    abi::edict_t client{};
    client.v.flags = abi::k_fl_client;
    abi::edict_t follower{};
    follower.v.movetype           = abi::k_movetype_follow;
    follower.v.aiment             = &client;
    follower.v.absmin[0] = follower.v.absmax[0] = 1000.0f; // would be solid if probed
    const std::size_t before = solid.points.size();
    CHECK( !save::entity_in_solid( follower, solid ) );
    CHECK_EQ( solid.points.size(), before ); // seam NOT consulted

    // FOLLOW but aiment is NOT a client -> probed normally (solid).
    abi::edict_t nonclient{};
    abi::edict_t follower2{};
    follower2.v.movetype = abi::k_movetype_follow;
    follower2.v.aiment   = &nonclient;
    follower2.v.absmin[0] = follower2.v.absmax[0] = 1000.0f;
    CHECK( save::entity_in_solid( follower2, solid ) );
}

// ===========================================================================
// 5. levelMask selection matrix + branches (CreateEntityTransitionList)
// ===========================================================================
static void test_transition_matrix()
{
    // 5 named entities (maxclients 0 -> every row dispatches Named).
    std::array<std::string_view, 5> classes = { "worldspawn", "env_global", "func_door",
                                                 "info_node", "func_wall" };
    std::array<bool, 5>             client   = { false, false, false, false, false };
    std::array<abi::LEVELLIST, 1>   conns    = { make_conn( "newmap", "lm", 0, 0, 0 ) };

    std::vector<std::byte> image = make_hl1( "adjacent", 30.0f, conns, classes, client );

    Loaded L;
    load_image( L, image );
    REQUIRE( L.table.count() == 5u );

    // Overlay the per-entity flags a real adjacent save would carry.
    const int mask = static_cast<int>( 1u << 0 ); // levelMask = BIT(0)
    L.table.row( 0 ).flags = static_cast<int>( abi::k_fenttable_global | ( 1u << 0 ) ); // global + match
    L.table.row( 1 ).flags = static_cast<int>( 1u << 0 );                               // moveable + match
    L.table.row( 2 ).flags = static_cast<int>( 1u << 1 );                               // matches a DIFFERENT slot
    L.table.row( 3 ).flags = 0;                                                          // no PVS bit
    L.table.row( 4 ).flags = static_cast<int>( abi::k_fenttable_moveable );              // MOVEABLE only (inert)

    FakeTransitionRestorer r;
    FakeSolid              solid;
    save::TransitionContext ctx;
    ctx.time_basis      = 55.0f;
    ctx.use_landmark    = true;
    ctx.landmark_offset = save::SaveVec3{ 9.0f, 18.0f, 27.0f };

    auto moved = save::create_entity_transition_list( L.state, L.table, r, solid, mask, ctx,
                                                      save::RestoreConfig{ 0 } );
    REQUIRE( moved.has_value() );

    // Only rows 0 and 1 are active (flags & mask) -> created.  Rows 2/3/4 not.
    CHECK_EQ( r.count_kind( TCall::Create ), 2u );
    // Row 0 -> global merge branch; row 1 -> moveable transfer branch.
    CHECK_EQ( r.count_kind( TCall::Merge ), 1u );
    CHECK_EQ( r.count_kind( TCall::Transfer ), 1u );
    CHECK( r.find_index( TCall::Merge, 0 ) != nullptr );
    CHECK( r.find_index( TCall::Transfer, 1 ) != nullptr );

    // Transfer decisions: row2 (BIT1), row3 (0), row4 (MOVEABLE only) NOT touched.
    CHECK( r.find_index( TCall::Transfer, 2 ) == nullptr );
    CHECK( r.find_index( TCall::Transfer, 3 ) == nullptr );
    CHECK( r.find_index( TCall::Merge, 4 ) == nullptr );
    CHECK( r.find_index( TCall::Transfer, 4 ) == nullptr );

    // movedCount = global-merge(1) + moveable-move(1) = 2.
    CHECK_EQ( *moved, 2 );

    // Moveable row 1 got FENTTABLE_REMOVED (plain assign) + counted; global row 0 not remarked.
    CHECK_EQ( static_cast<unsigned>( L.table.row( 1 ).flags ), abi::k_fenttable_removed );
    CHECK( ( static_cast<unsigned>( L.table.row( 0 ).flags ) & abi::k_fenttable_removed ) == 0 );

    // ctx reached the restorer intact on both branches (time quirk + landmark offset).
    const TCall *t = r.find_index( TCall::Transfer, 1 );
    const TCall *m = r.find_index( TCall::Merge, 0 );
    REQUIRE( t != nullptr && m != nullptr );
    CHECK( t->time_basis == 55.0f );
    CHECK( m->time_basis == 55.0f );
    CHECK( t->use_landmark && m->use_landmark );
    CHECK( t->landmark_offset == ( save::SaveVec3{ 9.0f, 18.0f, 27.0f } ) );
    CHECK( m->landmark_offset == ( save::SaveVec3{ 9.0f, 18.0f, 27.0f } ) );

    // Each processed row got a distinct data window (positioning); row 1 decodes 100+1.
    CHECK( t->decoded_float == 101.0f );
    CHECK( t->window_size > 0u );

    // SV_FreeOldEntities called once per processed row (rows 0 and 1).
    CHECK_EQ( r.free_old_count, 2 );
}

// ===========================================================================
// 6. Global-entity merge: repoint-on-failure vs merge-success
// ===========================================================================
static void test_global_merge_repoint()
{
    std::array<std::string_view, 1> classes = { "env_global" };
    std::array<bool, 1>             client  = { false };
    std::array<abi::LEVELLIST, 1>   conns   = { make_conn( "newmap", "lm", 0, 0, 0 ) };
    std::vector<std::byte>          image   = make_hl1( "adj", 10.0f, conns, classes, client );

    const int mask = static_cast<int>( 1u << 0 );

    // --- Merge FAILS with a valid repoint target: row.pent repointed, temp killed.
    {
        Loaded L;
        load_image( L, image );
        L.table.row( 0 ).flags = static_cast<int>( abi::k_fenttable_global | ( 1u << 0 ) );

        abi::edict_t           existing{}; // the already-spawned global on the new level
        FakeTransitionRestorer r;
        r.global_merges   = false;
        r.global_repoint  = &existing;
        FakeSolid              solid;
        save::TransitionContext ctx;

        auto moved = save::create_entity_transition_list( L.state, L.table, r, solid, mask, ctx,
                                                          save::RestoreConfig{ 0 } );
        REQUIRE( moved.has_value() );
        CHECK_EQ( *moved, 0 ); // failed merge does NOT count as moved

        const TCall *m = r.find_index( TCall::Merge, 0 );
        REQUIRE( m != nullptr );
        // The temp edict (the created one) got FL_KILLME; the table repoints to `existing`.
        CHECK( ( static_cast<unsigned>( m->pent->v.flags ) & static_cast<unsigned>( abi::k_fl_killme ) ) != 0 );
        CHECK( L.table.row( 0 ).pent == &existing );
    }

    // --- Merge SUCCEEDS: movedCount++, no repoint, no kill.
    {
        Loaded L;
        load_image( L, image );
        L.table.row( 0 ).flags = static_cast<int>( abi::k_fenttable_global | ( 1u << 0 ) );

        FakeTransitionRestorer r;
        r.global_merges = true;
        FakeSolid              solid;
        save::TransitionContext ctx;

        auto moved = save::create_entity_transition_list( L.state, L.table, r, solid, mask, ctx,
                                                          save::RestoreConfig{ 0 } );
        REQUIRE( moved.has_value() );
        CHECK_EQ( *moved, 1 );

        const TCall *m = r.find_index( TCall::Merge, 0 );
        REQUIRE( m != nullptr );
        CHECK( ( static_cast<unsigned>( m->pent->v.flags ) & static_cast<unsigned>( abi::k_fl_killme ) ) == 0 );
        CHECK( L.table.row( 0 ).pent == m->pent ); // unchanged
    }
}

// ===========================================================================
// 7. Post-transfer pruning (in-solid killed, moved marked+counted, rc<0 killed)
// ===========================================================================
static void test_pruning()
{
    std::array<std::string_view, 3> classes = { "func_door", "monster_x", "func_wall" };
    std::array<bool, 3>             client  = { false, false, false };
    std::array<abi::LEVELLIST, 1>   conns   = { make_conn( "newmap", "lm", 0, 0, 0 ) };
    std::vector<std::byte>          image   = make_hl1( "adj", 10.0f, conns, classes, client );

    Loaded L;
    load_image( L, image );
    const int mask = static_cast<int>( 1u << 0 );
    L.table.row( 0 ).flags = static_cast<int>( 1u << 0 ); // will be moved cleanly
    L.table.row( 1 ).flags = static_cast<int>( 1u << 0 ); // will be stuck-in-solid
    L.table.row( 2 ).flags = static_cast<int>( 1u << 0 ); // transfer returns < 0

    FakeTransitionRestorer r;
    r.stuck_index   = 1;
    r.fail_transfer = 2;
    FakeSolid               solid;
    save::TransitionContext ctx;

    auto moved = save::create_entity_transition_list( L.state, L.table, r, solid, mask, ctx,
                                                      save::RestoreConfig{ 0 } );
    REQUIRE( moved.has_value() );

    // Row 0: cleanly moved -> FENTTABLE_REMOVED + counted.
    CHECK_EQ( static_cast<unsigned>( L.table.row( 0 ).flags ), abi::k_fenttable_removed );
    // Row 1: in solid -> FL_KILLME, NOT REMOVED, NOT counted.
    const TCall *t1 = r.find_index( TCall::Transfer, 1 );
    REQUIRE( t1 != nullptr );
    CHECK( ( static_cast<unsigned>( t1->pent->v.flags ) & static_cast<unsigned>( abi::k_fl_killme ) ) != 0 );
    CHECK( static_cast<unsigned>( L.table.row( 1 ).flags ) != abi::k_fenttable_removed );
    // Row 2: transfer rc<0 -> FL_KILLME, NOT counted.
    const TCall *t2 = r.find_index( TCall::Transfer, 2 );
    REQUIRE( t2 != nullptr );
    CHECK( ( static_cast<unsigned>( t2->pent->v.flags ) & static_cast<unsigned>( abi::k_fl_killme ) ) != 0 );

    // Only row 0 counted.
    CHECK_EQ( *moved, 1 );
}

// ===========================================================================
// 8. .HL3 patch interplay — a patched-out row doesn't transfer
// ===========================================================================
static void test_hl3_patch_interplay()
{
    std::array<std::string_view, 2> classes = { "func_door", "func_button" };
    std::array<bool, 2>             client  = { false, false };
    std::array<abi::LEVELLIST, 1>   conns   = { make_conn( "newmap", "lm", 0, 0, 0 ) };
    std::vector<std::byte>          image   = make_hl1( "adj", 10.0f, conns, classes, client );

    Loaded L;
    load_image( L, image );
    L.table.row( 0 ).flags = static_cast<int>( 1u << 0 ); // would transfer...
    L.table.row( 1 ).flags = static_cast<int>( 1u << 0 );

    // Patch row 0 out (apply_entity_patch: PLAIN ASSIGN row.flags = REMOVED,
    // clobbering its PVS bit) BEFORE the transition.
    std::array<std::int32_t, 1> removed = { 0 };
    REQUIRE( save::apply_entity_patch( L.table, removed ).has_value() );
    CHECK_EQ( static_cast<unsigned>( L.table.row( 0 ).flags ), abi::k_fenttable_removed );

    FakeTransitionRestorer  r;
    FakeSolid               solid;
    save::TransitionContext ctx;
    const int               mask = static_cast<int>( 1u << 0 );
    auto moved = save::create_entity_transition_list( L.state, L.table, r, solid, mask, ctx,
                                                      save::RestoreConfig{ 0 } );
    REQUIRE( moved.has_value() );

    // Row 0 (patched-out) never created, never transferred; only row 1 moves.
    CHECK( r.find_index( TCall::Transfer, 0 ) == nullptr );
    CHECK( r.find_index( TCall::Transfer, 1 ) != nullptr );
    CHECK_EQ( *moved, 1 );
}

// ===========================================================================
// 9. LoadAdjacentEnts orchestration — full run() end-to-end
// ===========================================================================
static void test_run_orchestration()
{
    // Adjacent (== OLD) level "c1a0": worldspawn (id0) + player (id1, FL_CLIENT).
    // Its own connection list points back to the new map with landmark "lm"
    // (origin B) so the landmark offset resolves.
    std::array<std::string_view, 2> classes = { "worldspawn", "player" };
    std::array<bool, 2>             client  = { false, true };
    std::array<abi::LEVELLIST, 1>   old_conns = { make_conn( "c1a1", "lm", 1.0f, 2.0f, 3.0f ) }; // B
    std::vector<std::byte> old_hl1 = make_hl1( "c1a0", 30.0f, old_conns, classes, client );

    FakeSource src;
    src.levels.push_back( { "c1a0", old_hl1, false, {} } );

    // New level "c1a1" (just spawned): one connection to the old level, landmark
    // "lm" origin A -> offset A-B.
    std::array<abi::LEVELLIST, 1> new_conns = { make_conn( "c1a0", "lm", 10.0f, 20.0f, 30.0f ) }; // A

    save::AdjacentTransferParams p;
    p.new_map_name          = "c1a1";
    p.old_level             = "c1a0";
    p.landmark_name         = "lm";
    p.new_level_connections = new_conns;
    p.sv_time               = 55.0f; // NOT the old level's header.time (30)
    p.config                = save::RestoreConfig{ 1 }; // maxclients 1 -> id1 is a client

    auto buf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
    REQUIRE( buf != nullptr );
    save::EntityTable       table;
    save::AdjacentTransfer   xfer( *buf, table );
    FakeTransitionRestorer   r;
    FakeSolid                solid;

    REQUIRE( xfer.run( p, src, r, solid ).has_value() );

    // The player (row 1) transferred: created as a Client + moved.
    bool created_client = false;
    for ( const auto &c : r.calls )
        if ( c.kind == TCall::Create && c.create_kind == save::RestoreCreateKind::Client && c.id == 1 )
            created_client = true;
    CHECK( created_client );
    const TCall *pt = r.find_index( TCall::Transfer, 1 );
    REQUIRE( pt != nullptr );

    // Time quirk: restorer basis is sv.time (55), NOT header.time (30).
    CHECK( pt->time_basis == 55.0f );
    // Landmark offset delivered = A - B = (9,18,27).
    CHECK( pt->landmark_offset == ( save::SaveVec3{ 9.0f, 18.0f, 27.0f } ) );

    // The world (row 0, not FL_CLIENT, no PVS bit) did NOT transfer.
    CHECK( r.find_index( TCall::Transfer, 0 ) == nullptr );

    // moved -> the adjacent level's .HL3 was rewritten, listing the moved row (1).
    REQUIRE( src.writes.size() == 1u );
    CHECK( src.writes[0].first == "c1a0" );
    REQUIRE( src.writes[0].second.size() == 1u );
    CHECK_EQ( src.writes[0].second[0], 1 );
}

// ===========================================================================
// 10. Orchestration edge cases: dedup, missing-image skip, foundprevious fail
// ===========================================================================
static void test_run_edge_cases()
{
    std::array<std::string_view, 1> classes   = { "worldspawn" };
    std::array<bool, 1>             client    = { false };
    std::array<abi::LEVELLIST, 1>   self_conn = { make_conn( "c1a1", "lm", 0, 0, 0 ) };
    std::vector<std::byte>          hl1       = make_hl1( "c1a0", 10.0f, self_conn, classes, client );

    // --- foundprevious hard-fail: new connections do NOT include the old level.
    {
        FakeSource src; // empty is fine; the missing back-connection fails first
        std::array<abi::LEVELLIST, 1> new_conns = { make_conn( "somewhere_else", "lm", 0, 0, 0 ) };
        save::AdjacentTransferParams  p;
        p.new_map_name          = "c1a1";
        p.old_level             = "c1a0";
        p.landmark_name         = "lm";
        p.new_level_connections = new_conns;

        auto buf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
        REQUIRE( buf != nullptr );
        save::EntityTable      table;
        save::AdjacentTransfer xfer( *buf, table );
        FakeTransitionRestorer r;
        FakeSolid              solid;
        auto                   res = xfer.run( p, src, r, solid );
        CHECK( !res.has_value() );
        CHECK( res.error() == save::SaveError::TransitionBroken );
    }

    // --- dedup + missing-image skip: two connections to the SAME missing map ->
    //     load_level_image queried once; foundprevious satisfied (old==that map).
    {
        struct CountingSource final : save::IAdjacentLevelSource
        {
            int loads = 0;
            std::optional<std::vector<std::byte>> load_level_image( std::string_view ) noexcept override
            {
                ++loads;
                return std::nullopt; // missing -> skip
            }
            std::optional<std::vector<std::byte>> load_entity_patch( std::string_view ) noexcept override
            {
                return std::nullopt;
            }
            save::Result<void> store_entity_patch( std::string_view,
                                                   const save::EntityTable & ) noexcept override
            {
                return {};
            }
        } src;

        std::array<abi::LEVELLIST, 2> new_conns = {
            make_conn( "c1a0", "lm", 0, 0, 0 ),
            make_conn( "C1A0", "lm", 0, 0, 0 ), // duplicate (case-insensitive)
        };
        save::AdjacentTransferParams p;
        p.new_map_name          = "c1a1";
        p.old_level             = "c1a0";
        p.landmark_name         = "lm";
        p.new_level_connections = new_conns;

        auto buf = save::create_save_buffer( g_pool, 8192, 64, 0.0f );
        REQUIRE( buf != nullptr );
        save::EntityTable      table;
        save::AdjacentTransfer xfer( *buf, table );
        FakeTransitionRestorer r;
        FakeSolid              solid;
        // foundprevious is satisfied (old level IS in the list) -> Ok despite the
        // missing image, and the dedup means only ONE load attempt.
        REQUIRE( xfer.run( p, src, r, solid ).has_value() );
        CHECK_EQ( src.loads, 1 );
    }
}

// ===========================================================================
// 11. Corrupt case: oversize ENTITYTABLE.location -> CorruptHeader
// ===========================================================================
static void test_corrupt_window()
{
    std::array<std::string_view, 1> classes = { "func_door" };
    std::array<bool, 1>             client  = { false };
    std::array<abi::LEVELLIST, 1>   conns   = { make_conn( "newmap", "lm", 0, 0, 0 ) };
    std::vector<std::byte>          image   = make_hl1( "adj", 10.0f, conns, classes, client );

    Loaded L;
    load_image( L, image );
    L.table.row( 0 ).flags    = static_cast<int>( 1u << 0 );
    L.table.row( 0 ).location = 1000000; // far past the data region

    FakeTransitionRestorer  r;
    FakeSolid               solid;
    save::TransitionContext ctx;
    auto moved = save::create_entity_transition_list( L.state, L.table, r, solid,
                                                      static_cast<int>( 1u << 0 ), ctx,
                                                      save::RestoreConfig{ 0 } );
    CHECK( !moved.has_value() );
    CHECK( moved.error() == save::SaveError::CorruptHeader );
}

// ---------------------------------------------------------------------------
int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "adjacent_transfer_test" );

    RUN_TEST( test_landmark_math );
    RUN_TEST( test_compute_transition_mask );
    RUN_TEST( test_decal_offset );
    RUN_TEST( test_entity_in_solid );
    RUN_TEST( test_transition_matrix );
    RUN_TEST( test_global_merge_repoint );
    RUN_TEST( test_pruning );
    RUN_TEST( test_hl3_patch_interplay );
    RUN_TEST( test_run_orchestration );
    RUN_TEST( test_run_edge_cases );
    RUN_TEST( test_corrupt_window );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "adjacent_transfer: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
