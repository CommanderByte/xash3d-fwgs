// xash3dpp — enginefuncs_t table pins (Chunk 6 S6b)
// Covers: full slot population (no null slot among the 159), the peoei
// bugcomp patch, edict identity/lifecycle slots over the S4 arena,
// string-pool slots, the entity search pair, trace slots composed over
// the S5 world kernel (incl. the trace_flags reset side effect), point
// contents, math/anglemod goldens, lightstyle + illum, group masking,
// fat-PVS buffer identity, CheckVisibility guards, the external-cvar
// chain, and the cross-DLL probe (the fake game DLL calling back INTO
// the engine through the received table).
// Fixture: make_minimal_world — hull 0 WATER x<128, EMPTY beyond;
// hull 1 SOLID x<128 && y<64 (S5 goldens reused: Minkowski face at 174).

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp>
#include <xash3dpp/abi/entity_view.hpp>

#include "../../content/studio_builder.hpp"
#include "../../map_loader/bsp/test_bsp_builder.hpp"
#include "fake_dll_state.hpp"

#include "../../test_helpers.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace sv  = xash::server;
namespace abi = xash::abi;
namespace ml  = xash::map_loader;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

namespace {

int g_host_errors = 0;

void count_host_error( void *, const char * )
{
    ++g_host_errors;
}

struct FixtureResolver final : sv::IModelResolver
{
    std::optional<sv::BrushModel> brush_model( int modelindex ) noexcept override
    {
        if ( modelindex == 1 )
            return sv::BrushModel{ 0 };
        return std::nullopt;
    }
    bool is_studio( int ) noexcept override { return false; }

    // A synthetic studio model at modelindex 2 (studio_data set by the test).
    std::vector<std::byte> studio_data;
    std::span<const std::byte> studio_bytes( int modelindex ) noexcept override
    {
        if ( modelindex == 2 && !studio_data.empty() )
            return studio_data;
        return {};
    }
};

struct LinkHooks final : sv::IWorldLinkHooks
{
    void set_abs_box( abi::edict_t *ent ) noexcept override
    {
        sv::EntityView v( ent );
        const Vec3 org = v.origin();
        v.set_absmin( org + v.mins() );
        v.set_absmax( org + v.maxs() );
    }
    void dispatch_touch( abi::edict_t *, abi::edict_t * ) noexcept override {}
};

struct BridgeFixture
{
    xash::memory::PoolHandle     pool;
    sv::EdictArena               arena;
    sv::StringPool               strings;
    sv::WorldLinks               links;
    LinkHooks                    hooks;
    FixtureResolver              resolver;
    std::optional<ml::WorldData> world;
    sv::LinkEnv                  lenv;
    sv::MoveEnv                  env;
    sv::LightStyles              styles;
    abi::globalvars_t            globals{};
    sv::EngineBridge             bridge;
    abi::enginefuncs_t           table{};

    BridgeFixture()
    {
        g_host_errors = 0;

        pool = xash::memory::create_pool( "test_engine_table" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( arena.init( pool, 32, 3 )); // world + 2 client slots
        REQUIRE( strings.init( pool, 32 ));

        ml::WorldLoadOptions opts;
        opts.is_world = true;
        auto w = ml::load_world_data( test_bsp::make_minimal_world().build(),
                                      "t", opts );
        REQUIRE( w.has_value() );
        world.emplace( std::move( *w ));

        links.set_hooks( &hooks );
        links.clear_world( { -256, -256, -256 }, { 256, 256, 256 } );
        styles.reset();

        // SV_SpawnServer claims the world + client slots via SV_InitEdict
        // (sv_init.c:1054-1064); a fresh arena is all-free like legacy
        // post-SV_LoadProgs.
        arena.init_edict( arena.edict_num( 0 ));
        arena.init_edict( arena.edict_num( 1 ));
        arena.init_edict( arena.edict_num( 2 ));

        abi::edict_t *ws = arena.edict_num( 0 );
        ws->v.modelindex = 1;
        ws->v.solid      = abi::k_solid_bsp;
        ws->v.movetype   = abi::k_movetype_push;

        lenv.world      = &*world;
        lenv.worldspawn = ws;

        env.world      = &*world;
        env.models     = &resolver;
        env.area_root  = links.root();
        env.worldspawn = ws;

        bridge.arena       = &arena;
        bridge.strings     = &strings;
        bridge.globals     = &globals;
        bridge.move_env    = &env;
        bridge.links       = &links;
        bridge.link_env    = &lenv;
        bridge.lightstyles = &styles;
        bridge.misc_pool   = pool;
        bridge.sv_time     = 1.0;
        bridge.max_clients = 2;
        bridge.host_error  = count_host_error;

        sv::install_engine_bridge( &bridge );
        table = sv::build_engine_table( false );
    }

    ~BridgeFixture()
    {
        sv::reset_external_cvars( bridge );
        sv::install_engine_bridge( nullptr );
        arena.shutdown();
        strings.shutdown();
        xash::memory::destroy_pool( pool );
    }

    [[nodiscard]] abi::edict_t *world_edict() const noexcept
    {
        return arena.edict_num( 0 );
    }
};

} // namespace

// ---------------------------------------------------------------------------
// population / bugcomp
// ---------------------------------------------------------------------------

static void test_every_slot_populated()
{
    BridgeFixture f;

    // Byte-level scan: no slot of the 159 may be null.
    const auto *bytes = reinterpret_cast<const unsigned char *>( &f.table );
    const unsigned char zero[sizeof( void * )]{};
    int null_slots = 0;

    for ( std::size_t i = 0; i < 159; ++i )
    {
        if ( std::memcmp( bytes + i * sizeof( void * ), zero,
                          sizeof( void * )) == 0 )
            ++null_slots;
    }

    CHECK_EQ( null_slots, 0 );
}

static void test_peoei_bugcomp_patch()
{
    BridgeFixture f;

    const abi::enginefuncs_t broken = sv::build_engine_table( true );

    // The patch swaps exactly one slot.
    CHECK( broken.pfnPEntityOfEntIndex != f.table.pfnPEntityOfEntIndex );
    CHECK( broken.pfnPEntityOfEntIndexAllEntities ==
           f.table.pfnPEntityOfEntIndexAllEntities );

    // Boundary: index == maxclients (2), valid edict, NO private data.
    // Fixed rule (player when index <= maxclients) → the edict;
    // broken GoldSrc rule (index < maxclients) → NULL.
    CHECK( f.table.pfnPEntityOfEntIndex( 2 ) == f.arena.edict_num( 2 ));
    CHECK( broken.pfnPEntityOfEntIndex( 2 ) == nullptr );

    // Index 0 always returns the raw slot.
    CHECK( f.table.pfnPEntityOfEntIndex( 0 ) == f.world_edict() );
    CHECK( broken.pfnPEntityOfEntIndex( 0 ) == f.world_edict() );

    // The core rule for non-player slots: private data required.
    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    const int idx = f.table.pfnIndexOfEdict( ent );

    CHECK( f.table.pfnPEntityOfEntIndex( idx ) == nullptr ); // no pvPrivateData
    REQUIRE( f.table.pfnPvAllocEntPrivateData( ent, 8 ) != nullptr );
    CHECK( f.table.pfnPEntityOfEntIndex( idx ) == ent );
}

// ---------------------------------------------------------------------------
// edicts / strings
// ---------------------------------------------------------------------------

static void test_edict_identity_slots()
{
    BridgeFixture f;

    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );

    const int idx = f.table.pfnIndexOfEdict( ent );
    CHECK( idx >= 3 ); // above the reserved world + client slots

    const int off = f.table.pfnEntOffsetOfPEntity( ent );
    CHECK( f.table.pfnPEntityOfEntOffset( off ) == ent );
    CHECK( f.table.pfnGetVarsOfEnt( ent ) == &ent->v );
    CHECK( f.table.pfnFindEntityByVars( &ent->v ) == ent );
    CHECK_EQ( f.table.pfnNumberOfEntities(),
              static_cast<int>( f.arena.num_entities() ));

    // private data through the ABI (16-byte round-up pinned in S4 tests)
    void *priv = f.table.pfnPvAllocEntPrivateData( ent, 17 );
    REQUIRE( priv != nullptr );
    CHECK( f.table.pfnPvEntPrivateData( ent ) == priv );
    f.table.pfnFreeEntPrivateData( ent );
    CHECK( f.table.pfnPvEntPrivateData( ent ) == nullptr );

    // world/client guard: refuses, edict stays alive
    f.table.pfnRemoveEntity( f.world_edict() );
    CHECK( !f.world_edict()->free );

    f.table.pfnRemoveEntity( ent );
    CHECK( ent->free != 0 );

    // no game DLL installed → no spawn export → alloc + free + NULL
    const int cls = f.table.pfnAllocString( "monster_probe" );
    CHECK( f.table.pfnCreateNamedEntity( cls ) == nullptr );
}

static void test_string_slots()
{
    BridgeFixture f;

    const int s = f.table.pfnAllocString( "some\\nvalue" );
    CHECK( s > 0 );
    // escape processing applied (S4 pins the details)
    CHECK( std::strcmp( f.table.pfnSzFromIndex( s ), "some\nvalue" ) == 0 );
}

static void test_entity_search_slots()
{
    BridgeFixture f;

    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    ent->v.targetname = f.table.pfnAllocString( "gate1" );

    CHECK( f.table.pfnFindEntityByString( nullptr, "targetname", "gate1" ) ==
           ent );
    // miss → WORLD, not NULL
    CHECK( f.table.pfnFindEntityByString( nullptr, "targetname", "nope" ) ==
           f.world_edict() );
    // non-string field → WORLD
    CHECK( f.table.pfnFindEntityByString( nullptr, "health", "1" ) ==
           f.world_edict() );

    // sphere: box distance against absmin/absmax
    abi::edict_t *box = f.table.pfnCreateEntity();
    REQUIRE( box != nullptr );
    sv::store_vec3( box->v.absmin, { 484, -16, -16 } );
    sv::store_vec3( box->v.absmax, { 516, 16, 16 } );

    const float org[3] = { 500, 0, 50 };
    CHECK( f.table.pfnFindEntityInSphere( nullptr, org, 100.0f ) == box );
    CHECK( f.table.pfnFindEntityInSphere( nullptr, org, 10.0f ) ==
           f.world_edict() );
}

// ---------------------------------------------------------------------------
// traces / contents
// ---------------------------------------------------------------------------

static void test_trace_slots()
{
    BridgeFixture f;

    // Hull-1 trace into the solid column: the fixture's hull-1 clipnode
    // face sits at x = 128 (y < 64 branch), tracing 200 → 0 at y = 0.
    abi::TraceResult tr{};
    f.globals.trace_flags = 5; // must be reset by the conversion

    const float start[3] = { 200, 0, 0 }, end[3] = { 0, 0, 0 };
    f.table.pfnTraceHull( start, end, abi::k_move_normal, 1, nullptr, &tr );

    CHECK( tr.flFraction < 1.0f );
    CHECK( std::fabs( tr.vecEndPos[0] - 128.0f ) < 0.5f );
    CHECK( tr.pHit == f.world_edict() );
    CHECK_EQ( f.globals.trace_flags, 0.0f );

    // Point trace (hull 0 has no solids in the fixture) → clean miss,
    // invalid hit ent rewritten to world.
    abi::TraceResult line{};
    f.table.pfnTraceLine( start, end, abi::k_move_normal, nullptr, &line );
    CHECK_EQ( line.flFraction, 1.0f );
    CHECK( line.pHit == f.world_edict() );

    // Monster hull: returns 1 when blocked.
    abi::edict_t *mon = f.table.pfnCreateEntity();
    REQUIRE( mon != nullptr );
    sv::store_vec3( mon->v.mins, { -16, -16, -36 } );
    sv::store_vec3( mon->v.maxs, { 16, 16, 36 } );
    abi::TraceResult mtr{};
    CHECK_EQ( f.table.pfnTraceMonsterHull( mon, start, end,
                                           abi::k_move_normal, nullptr,
                                           &mtr ),
              1 );
}

static void test_point_contents_slot()
{
    BridgeFixture f;

    const float water[3] = { 100, 0, 0 }, open[3] = { 200, 0, 0 };
    CHECK_EQ( f.table.pfnPointContents( water ), ml::k_contents_water );
    CHECK_EQ( f.table.pfnPointContents( open ), ml::k_contents_empty );
}

static void test_drop_to_floor_slot()
{
    BridgeFixture f;

    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    sv::store_vec3( ent->v.mins, { -16, -16, -36 } );
    sv::store_vec3( ent->v.maxs, { 16, 16, 36 } );

    // Inside the fixture's full-height solid column → allsolid → -1.
    sv::store_vec3( ent->v.origin, { 100, 30, 200 } );
    CHECK_EQ( f.table.pfnDropToFloor( ent ), -1 );

    // Open region with nothing below → fraction 1 → 0, flags untouched.
    sv::store_vec3( ent->v.origin, { 200, 200, 100 } );
    CHECK_EQ( f.table.pfnDropToFloor( ent ), 0 );
    CHECK_EQ( ent->v.flags & abi::k_fl_onground, 0 );
}

// ---------------------------------------------------------------------------
// math / misc
// ---------------------------------------------------------------------------

static void test_math_slots()
{
    BridgeFixture f;

    const float east[3] = { 0, 1, 0 };
    CHECK( std::fabs( f.table.pfnVecToYaw( east ) - 90.0f ) < 0.01f );

    // MakeVectors writes the globals triplet.
    const float angles[3] = { 0, 90, 0 };
    f.table.pfnMakeVectors( angles );
    CHECK( std::fabs( f.globals.v_forward[1] - 1.0f ) < 1e-5f );
    CHECK( std::fabs( f.globals.v_forward[0] ) < 1e-5f );

    float fwd[3], right[3], up[3];
    f.table.pfnAngleVectors( angles, fwd, right, up );
    CHECK( std::fabs( fwd[1] - 1.0f ) < 1e-5f );
    CHECK( std::fabs( up[2] - 1.0f ) < 1e-5f );

    // ChangeYaw: classic anglemod stepping, capped at yaw_speed.
    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    ent->v.ideal_yaw = 90.0f;
    ent->v.yaw_speed = 10.0f;
    f.table.pfnChangeYaw( ent );
    CHECK( std::fabs( ent->v.angles[1] - 10.0f ) < 0.01f );

    // CRC32 golden ("123456789" → 0xCBF43926).
    abi::CRC32_t crc = 0;
    f.table.pfnCRC32_Init( &crc );
    f.table.pfnCRC32_ProcessBuffer( &crc, "123456789", 9 );
    CHECK_EQ( f.table.pfnCRC32_Final( crc ), 0xCBF43926u );

    CHECK_EQ( f.table.pfnRandomLong( 5, 5 ), 5 );
    const int r = f.table.pfnRandomLong( 0, 10 );
    CHECK( r >= 0 && r <= 10 );
    const float rf = f.table.pfnRandomFloat( 1.0f, 2.0f );
    CHECK( rf >= 1.0f && rf <= 2.0f );

    CHECK( f.table.pfnTime() >= 0.0f );
    CHECK_EQ( f.table.pfnIsDedicatedServer(), 1 );
    CHECK_EQ( f.table.pfnGetPlayerWONId( nullptr ),
              static_cast<unsigned int>( -1 ));
}

static void test_set_size_and_origin_slots()
{
    BridgeFixture f;

    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    // SOLID_NOT + skin >= -1 entities are skipped by SV_LinkEdict (exact
    // legacy) — give it a real solid so the relink is observable.
    ent->v.solid = abi::k_solid_bbox;

    const float mins[3] = { -8, -8, -8 }, maxs[3] = { 8, 8, 8 };
    f.table.pfnSetSize( ent, mins, maxs );
    CHECK_EQ( ent->v.size[0], 16.0f );
    CHECK( f.links.linked( ent )); // relink = true (sv_game.c:1363)

    // backwards mins/maxs rejected, bounds unchanged
    const float bad[3] = { 10, 10, 10 };
    f.table.pfnSetSize( ent, bad, mins );
    CHECK_EQ( ent->v.maxs[0], 8.0f );

    sv::WorldLinks::unlink_edict( ent );
    const float org[3] = { 200, 200, 100 };
    f.table.pfnSetOrigin( ent, org );
    CHECK_EQ( ent->v.origin[0], 200.0f );
    CHECK( f.links.linked( ent ));
}

// ---------------------------------------------------------------------------
// light / group / visibility
// ---------------------------------------------------------------------------

static void test_lightstyle_and_illum_slots()
{
    BridgeFixture f;

    f.table.pfnLightStyle( 3, "am" );
    CHECK_EQ( f.styles.style( 3 )->map[1], 12.0f );

    CHECK_EQ( g_host_errors, 0 );
    f.table.pfnLightStyle( 999, "a" ); // legacy Host_Error path
    CHECK_EQ( g_host_errors, 1 );

    abi::edict_t *bright = f.table.pfnCreateEntity();
    REQUIRE( bright != nullptr );
    bright->v.effects = abi::k_ef_fullbright;
    CHECK_EQ( f.table.pfnGetEntityIllum( bright ), 255 );
}

static void test_group_mask_slot()
{
    BridgeFixture f;

    f.table.pfnSetGroupMask( 0x4, 1 );
    CHECK_EQ( f.bridge.group_mask, 0x4 );
    CHECK_EQ( f.env.group_mask, 0x4 );
    CHECK( f.env.group_op == sv::GroupOp::Nand );

    f.table.pfnSetGroupMask( 0, 0 );
    CHECK( f.env.group_op == sv::GroupOp::And );
}

static void test_visibility_slots()
{
    BridgeFixture f;

    // Fat buffers: stable identity (file-static, legacy sv_game.c:31).
    unsigned char *pvs1 = f.table.pfnSetFatPVS( nullptr );
    unsigned char *pvs2 = f.table.pfnSetFatPVS( nullptr );
    REQUIRE( pvs1 != nullptr );
    CHECK( pvs1 == pvs2 );
    unsigned char *pas = f.table.pfnSetFatPAS( nullptr );
    REQUIRE( pas != nullptr );
    CHECK( pas != pvs1 ); // PAS has its own buffer

    // NULL set → fullvis (GoldSrc rules).
    abi::edict_t *ent = f.table.pfnCreateEntity();
    REQUIRE( ent != nullptr );
    CHECK_EQ( f.table.pfnCheckVisibility( ent, nullptr ), 1 );

    // Leaf-cached path against an all-ones mask.
    ent->headnode      = -1;
    ent->num_leafs     = 1;
    ent->leafnums16[0] = 0;
    unsigned char all_on[64];
    std::memset( all_on, 0xFF, sizeof( all_on ));
    CHECK_EQ( f.table.pfnCheckVisibility( ent, all_on ), 1 );

    unsigned char all_off[64]{};
    CHECK_EQ( f.table.pfnCheckVisibility( ent, all_off ), 0 );
}

// ---------------------------------------------------------------------------
// external cvars
// ---------------------------------------------------------------------------

static void test_external_cvar_chain()
{
    BridgeFixture f;

    static char name[]  = "sv_probe";
    static char value[] = "42";
    abi::cvar_t var{ name, value, 0, 0.0f, nullptr };

    f.table.pfnCVarRegister( &var );
    CHECK(( var.flags & abi::k_fcvar_extdll ) != 0 ); // server variant
    CHECK_EQ( var.value, 42.0f );
    CHECK( f.table.pfnCVarGetPointer( "sv_probe" ) == &var );
    CHECK_EQ( f.table.pfnCVarGetFloat( "sv_probe" ), 42.0f );

    f.table.pfnCVarSetFloat( "sv_probe", 7.0f );
    CHECK_EQ( var.value, 7.0f );
    CHECK( std::strcmp( var.string, "7" ) == 0 );
    CHECK( var.string != value ); // engine-owned copy, DLL static untouched

    f.table.pfnCVarSetString( "sv_probe", "abc" );
    CHECK( std::strcmp( f.table.pfnCVarGetString( "sv_probe" ), "abc" ) == 0 );

    f.table.pfnCvar_DirectSet( &var, "1.5" );
    CHECK_EQ( var.value, 1.5f );

    // engine-lifetime variant: registered WITHOUT FCVAR_EXTDLL
    static char ename[]  = "host_probe";
    static char evalue[] = "1";
    abi::cvar_t evar{ ename, evalue, 0, 0.0f, nullptr };
    f.table.pfnCvar_RegisterVariable( &evar );
    CHECK_EQ( evar.flags & abi::k_fcvar_extdll, 0u );

    // unknown names: legacy-safe fallbacks (engine registry joins in S7)
    CHECK_EQ( f.table.pfnCVarGetFloat( "no_such" ), 0.0f );
    CHECK( std::strcmp( f.table.pfnCVarGetString( "no_such" ), "" ) == 0 );
}

// ---------------------------------------------------------------------------
// cross-DLL probe: the fake game DLL drives the REAL table
// ---------------------------------------------------------------------------

static void test_cross_dll_engine_probe()
{
    BridgeFixture f;
    sv::GameDll   dll;

    f.bridge.game = &dll;
    REQUIRE( dll.load( FAKE_DLL_FULL, &f.table, &f.globals ));

    auto probe = reinterpret_cast<void ( * )( void )>(
        dll.symbol( "fake_run_engine_probe" ));
    REQUIRE( probe != nullptr );
    probe();

    auto state_fn =
        reinterpret_cast<fake_dll::StateFn>( dll.symbol( "fake_state" ));
    REQUIRE( state_fn != nullptr );
    const fake_dll::State *st = state_fn();

    CHECK_EQ( st->probe_ran, 1 );
    CHECK_EQ( st->probe_string_ok, 1 );
    CHECK( st->probe_entity_index >= 3 );
    CHECK( st->probe_private != nullptr );
    CHECK_EQ( st->probe_crc, 0xCBF43926u );
    CHECK_EQ( st->probe_dedicated, 1 );

    // LINK_ENTITY dispatch through pfnCreateNamedEntity with a live DLL:
    // resolves the fake_item export by raw classname.
    const int cls = f.table.pfnAllocString( "fake_item" );
    abi::edict_t *item = f.table.pfnCreateNamedEntity( cls );
    REQUIRE( item != nullptr );
    CHECK_EQ( item->v.health, 123.0f );
    CHECK( item->v.pContainingEntity == item );

    dll.unload();
}

// ---------------------------------------------------------------------------
// studio pose slots (Chunk 7): pfnGetModelPtr / GetBonePosition / GetAttachment
// ---------------------------------------------------------------------------

static void test_studio_pose_slots()
{
    using namespace xash::content::test;

    BridgeFixture f;

    // Synthetic studio model at index 2: one bone at value pos {1,2,3}, no
    // rotation, no anim (bind pose), + one attachment (bone 0, local {1,0,0}).
    StudioBuilder b;
    const std::size_t bone_off = b.add_bone( -1, { -1, -1, -1, -1, -1, -1 },
                                             { 1, 2, 3, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } );
    const std::array<std::vector<std::int16_t>, 6> empty{};
    const std::size_t anim_off = b.add_anim_block( { empty } );
    const std::size_t seq_off  = b.add_seqdesc( 1, 0, 0, 1, static_cast<std::int32_t>( anim_off ), 0 );
    const std::size_t att_off  = b.add_attachment( 0, 1.0f, 0.0f, 0.0f );
    b.header_i32( 140, 1 ); b.header_i32( 144, static_cast<std::int32_t>( bone_off ) );
    b.header_i32( 164, 1 ); b.header_i32( 168, static_cast<std::int32_t>( seq_off ) );
    b.header_i32( 212, 1 ); b.header_i32( 216, static_cast<std::int32_t>( att_off ) );
    const auto &bytes = b.bytes();
    f.resolver.studio_data.assign( bytes.begin(), bytes.end() );

    // A studio entity at origin {10,20,30}, angles 0.
    abi::edict_t *e = f.arena.edict_num( 1 );
    e->v.modelindex = 2;
    e->v.origin[0] = 10.0f; e->v.origin[1] = 20.0f; e->v.origin[2] = 30.0f;
    e->v.angles[0] = e->v.angles[1] = e->v.angles[2] = 0.0f;
    e->v.sequence = 0;
    e->v.frame    = 0.0f;

    // pfnGetModelPtr -> the studiohdr byte image.
    void *ptr = f.table.pfnGetModelPtr( e );
    CHECK( ptr == f.resolver.studio_data.data() );

    // pfnGetBonePosition(bone 0) -> entity origin + bone pos; angles 0.
    float bo[3] = { -1, -1, -1 }, ba[3] = { -1, -1, -1 };
    f.table.pfnGetBonePosition( e, 0, bo, ba );
    CHECK( bo[0] == 11.0f && bo[1] == 22.0f && bo[2] == 33.0f );
    CHECK( ba[0] == 0.0f && ba[1] == 0.0f && ba[2] == 0.0f );

    // pfnGetAttachment(0) -> bone world + local {1,0,0}.
    float ao[3] = { 0, 0, 0 };
    f.table.pfnGetAttachment( e, 0, ao, nullptr );
    CHECK( ao[0] == 12.0f && ao[1] == 22.0f && ao[2] == 33.0f );

    // A brush model (index 1) -> NULL studiohdr; bone position untouched.
    abi::edict_t *br = f.arena.edict_num( 2 );
    br->v.modelindex = 1;
    CHECK( f.table.pfnGetModelPtr( br ) == nullptr );
    float ko[3] = { 7, 7, 7 };
    f.table.pfnGetBonePosition( br, 0, ko, nullptr );
    CHECK( ko[0] == 7.0f && ko[1] == 7.0f && ko[2] == 7.0f );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_every_slot_populated );
    RUN_TEST( test_peoei_bugcomp_patch );
    RUN_TEST( test_edict_identity_slots );
    RUN_TEST( test_string_slots );
    RUN_TEST( test_entity_search_slots );
    RUN_TEST( test_trace_slots );
    RUN_TEST( test_point_contents_slot );
    RUN_TEST( test_drop_to_floor_slot );
    RUN_TEST( test_math_slots );
    RUN_TEST( test_set_size_and_origin_slots );
    RUN_TEST( test_lightstyle_and_illum_slots );
    RUN_TEST( test_group_mask_slot );
    RUN_TEST( test_visibility_slots );
    RUN_TEST( test_external_cvar_chain );
    RUN_TEST( test_cross_dll_engine_probe );
    RUN_TEST( test_studio_pose_slots );

    std::printf( "engine_table: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
