// Chunk 11 synthetic role-parity witness. This executable deliberately links
// no server target. It drives the shared physics surface twice, sequentially,
// over value-initialized playermove fixtures that differ only in `server`.

#include <xash3dpp/physics/pm_trace.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/content/bone_solver.hpp>
#include <xash3dpp/core/legacy_random.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/world.hpp>
#include <xash3dpp/world/trace.hpp>

#include "../map_loader/bsp/test_bsp_builder.hpp"
#include "../test_helpers.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace abi = ::xash::abi;
namespace ct  = ::xash::content;
namespace ml  = ::xash::map_loader;
namespace phy = ::xash::physics;
namespace wr  = ::xash::world;
using ::xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

namespace {

[[nodiscard]] std::uint32_t float_bits( float value ) noexcept
{
    return std::bit_cast<std::uint32_t>( value );
}

void set_vec( abi::vec3_t out, const Vec3 &v ) noexcept
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] ct::StudioHitboxHull
make_studio_box( const Vec3 &center, float half, int hitgroup ) noexcept
{
    ct::StudioHitboxHull hull {};
    const Vec3 axes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    const float c[3] = { center.x, center.y, center.z };
    for ( int axis = 0; axis < 3; ++axis )
    {
        hull.planes[2 * axis].normal = axes[axis];
        hull.planes[2 * axis].dist = c[axis] + half;
        hull.planes[2 * axis + 1].normal = axes[axis];
        hull.planes[2 * axis + 1].dist = c[axis] - half;
    }
    hull.hitgroup = hitgroup;
    return hull;
}

struct FixtureResolver final : wr::IModelResolver
{
    const ml::WorldData *world = nullptr;
    std::array<std::byte, 1> studio_marker { std::byte{ 1 } };

    std::optional<wr::BrushModel> brush_model( int model_index ) noexcept override
    {
        if ( world == nullptr || model_index < 1 || model_index > 2 )
            return std::nullopt;
        return wr::BrushModel {
            static_cast<std::size_t>( model_index - 1 ), false
        };
    }

    bool is_studio( int model_index ) noexcept override
    {
        return model_index == 3;
    }

    std::span<const std::byte> studio_bytes( int model_index ) noexcept override
    {
        return model_index == 3
                   ? std::span<const std::byte>( studio_marker )
                   : std::span<const std::byte>();
    }

    int studio_hulls( int model_index, const wr::StudioHullPose &pose,
                      std::span<ct::StudioHitboxHull> out ) noexcept override
    {
        if ( model_index != 3 || out.empty() )
            return 0;
        out[0] = make_studio_box( pose.origin, 8.0f, 7 );
        return 1;
    }
};

struct Observation
{
    const char   *name = nullptr;
    std::uint64_t value = 0;
};

struct Projection
{
    std::array<Observation, 384> fields {};
    std::size_t count = 0;

    void integer( const char *name, std::int64_t value ) noexcept
    {
        fields[count++] = { name, static_cast<std::uint64_t>( value ) };
    }

    void floating( const char *name, float value ) noexcept
    {
        fields[count++] = { name, float_bits( value ) };
    }
};

struct ProbeContext
{
    abi::playermove_t *pm = nullptr;
    phy::PmTraceModelIndices indices {};
    ml::HullBoundsTable bounds = ml::k_default_hull_bounds;
    FixtureResolver resolver;
    phy::PmTraceEnv env {};
    ::xash::core::LegacyRandom rng;
    Projection projection;
    int callback_ordinal = 0;
    int rng_draw_position = 0;

    explicit ProbeContext( const ml::WorldData &world )
    {
        resolver.world = &world;
        env.world = &world;
        env.models = &resolver;
        env.model_indices = &indices;
        env.player_bounds = &bounds;
        env.pusher_ext = true;
    }
};

ProbeContext *g_probe = nullptr;

int random_long_thunk( int low, int high )
{
    ++g_probe->callback_ordinal;
    ++g_probe->rng_draw_position;
    return g_probe->rng.random_long( low, high );
}

float random_float_thunk( float low, float high )
{
    ++g_probe->callback_ordinal;
    ++g_probe->rng_draw_position;
    return g_probe->rng.random_float( low, high );
}

int filter_marked_physent( abi::physent_t *pe )
{
    ++g_probe->callback_ordinal;
    return pe->iuser1 == 777 ? 1 : 0;
}

void add_trace( ProbeContext &ctx, const char *prefix,
                const abi::pmtrace_t &trace, const Vec3 &start,
                const Vec3 &end, int physent_info, int model_index ) noexcept
{
    Projection &p = ctx.projection;
    p.integer( prefix, trace.allsolid );
    p.integer( "trace.startsolid", trace.startsolid );
    p.integer( "trace.inopen", trace.inopen );
    p.integer( "trace.inwater", trace.inwater );
    p.floating( "trace.fraction.bits", trace.fraction );
    p.floating( "trace.endpos.x.bits", trace.endpos[0] );
    p.floating( "trace.endpos.y.bits", trace.endpos[1] );
    p.floating( "trace.endpos.z.bits", trace.endpos[2] );
    p.floating( "trace.plane.x.bits", trace.plane.normal[0] );
    p.floating( "trace.plane.y.bits", trace.plane.normal[1] );
    p.floating( "trace.plane.z.bits", trace.plane.normal[2] );
    p.floating( "trace.plane.dist.bits", trace.plane.dist );
    p.integer( "trace.ent", trace.ent );
    p.integer( "trace.hitgroup", trace.hitgroup );
    p.integer( "trace.callback.ordinal", ctx.callback_ordinal );
    p.integer( "trace.physent.info", physent_info );
    p.integer( "trace.model.index", model_index );
    p.floating( "trace.input.start.x.bits", start.x );
    p.floating( "trace.input.start.y.bits", start.y );
    p.floating( "trace.input.start.z.bits", start.z );
    p.floating( "trace.input.end.x.bits", end.x );
    p.floating( "trace.input.end.y.bits", end.y );
    p.floating( "trace.input.end.z.bits", end.z );
    p.integer( "trace.rng.draw.position", ctx.rng_draw_position );
}

[[nodiscard]] phy::PmPhysentView list_view(
    abi::physent_t *entities, const int *indices, std::size_t count ) noexcept
{
    return { std::span<abi::physent_t>( entities, count ),
             std::span<const int>( indices, count ) };
}

void initialize_fixture( ProbeContext &ctx, abi::playermove_t &pm,
                         abi::movevars_t &movevars, int role_marker ) noexcept
{
    movevars = abi::movevars_t{};
    ctx.indices = phy::PmTraceModelIndices{};
    ctx.projection = Projection{};
    ctx.callback_ordinal = 0;
    ctx.rng_draw_position = 0;
    ctx.rng.set_seed( -12345 ); // reset between parity legs; oracle-verified seed

    pm.server = role_marker; // the sole value difference between the fixtures
    pm.movevars = &movevars;
    pm.RandomLong = &random_long_thunk;
    pm.RandomFloat = &random_float_thunk;
    movevars.gravity = 800.0f;
    movevars.maxspeed = 320.0f;
    movevars.stepsize = 18.0f;
    movevars.friction = 4.0f;

    // physents: world, two equal boxes, studio, glass, current brush, filtered
    // box, and a rotated brush. The model sidecars are aligned snapshots.
    pm.numphysent = 8;
    pm.physents[0].solid = abi::k_solid_bsp;
    pm.physents[0].info = 100;
    ctx.indices.physents[0] = 1;

    for ( int i : { 1, 2 } )
    {
        pm.physents[i].solid = abi::k_solid_bbox;
        pm.physents[i].info = 200 + i;
        set_vec( pm.physents[i].origin, Vec3{ 40, 0, 0 } );
        set_vec( pm.physents[i].mins, Vec3{ -8, -8, -8 } );
        set_vec( pm.physents[i].maxs, Vec3{ 8, 8, 8 } );
        ctx.indices.physents[static_cast<std::size_t>( i )] = 0;
    }

    pm.physents[3].solid = abi::k_solid_slidebox;
    pm.physents[3].info = 303;
    set_vec( pm.physents[3].origin, Vec3{ 60, 0, 0 } );
    ctx.indices.physents[3] = 3;

    pm.physents[4].solid = abi::k_solid_bbox;
    pm.physents[4].rendermode = 1;
    pm.physents[4].info = 404;
    set_vec( pm.physents[4].origin, Vec3{ 80, 0, 0 } );
    set_vec( pm.physents[4].mins, Vec3{ -8, -8, -8 } );
    set_vec( pm.physents[4].maxs, Vec3{ 8, 8, 8 } );

    pm.physents[5].solid = abi::k_solid_not;
    pm.physents[5].skin = ml::k_contents_current_0;
    pm.physents[5].info = 505;
    ctx.indices.physents[5] = 2;

    pm.physents[6].solid = abi::k_solid_bbox;
    pm.physents[6].iuser1 = 777;
    pm.physents[6].info = 606;
    set_vec( pm.physents[6].origin, Vec3{ 20, 0, 0 } );
    set_vec( pm.physents[6].mins, Vec3{ -4, -4, -4 } );
    set_vec( pm.physents[6].maxs, Vec3{ 4, 4, 4 } );

    pm.physents[7].solid = abi::k_solid_bsp;
    pm.physents[7].info = 707;
    set_vec( pm.physents[7].origin, Vec3{ 16, 0, 0 } );
    set_vec( pm.physents[7].angles, Vec3{ 15, 45, 5 } );
    ctx.indices.physents[7] = 2;

    pm.numvisent = 2;
    pm.visents[0] = pm.physents[0];
    pm.visents[1] = pm.physents[4];
    ctx.indices.visents[0] = 1;
    ctx.indices.visents[1] = 0;

    pm.nummoveent = 1;
    pm.moveents[0] = pm.physents[1];
    pm.moveents[0].info = 808;
    set_vec( pm.moveents[0].origin, Vec3{ 30, 0, 0 } );
    ctx.indices.moveents[0] = 0;

    ctx.pm = &pm;
}

using PmMoveSignature = void ( * )( abi::playermove_t *, int );

void synthetic_pm_move_probe( abi::playermove_t *pm, int role_marker )
{
    ProbeContext &ctx = *g_probe;
    CHECK_EQ( pm->server, role_marker );
    const Vec3 start { 200, 0, 0 };
    const Vec3 end { -100, 0, 0 };

    ++ctx.callback_ordinal;
    pm->usehull = 0;
    const auto world = phy::pm_player_trace_ext(
        ctx.env, *pm, start, end, 0,
        list_view( pm->physents, ctx.indices.physents.data(), 1 ), -1, nullptr );
    CHECK( world.fraction < 1.0f );
    add_trace( ctx, "world.allsolid", world, start, end,
               pm->physents[0].info, ctx.indices.physents[0] );

    ++ctx.callback_ordinal;
    const Vec3 box_start { 100, 0, 0 };
    const Vec3 box_end { 0, 0, 0 };
    const auto equal_boxes = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 1, ctx.indices.physents.data() + 1, 2 ),
        -1, nullptr );
    CHECK_EQ( equal_boxes.ent, 0 ); // strict '<': first equal-fraction hit wins
    add_trace( ctx, "equal.allsolid", equal_boxes, box_start, box_end,
               pm->physents[1].info, ctx.indices.physents[1] );

    ++ctx.callback_ordinal;
    pm->usehull = 2;
    const auto studio = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 3, ctx.indices.physents.data() + 3, 1 ),
        -1, nullptr );
    CHECK( studio.fraction < 1.0f );
    CHECK_EQ( studio.hitgroup, 7 );
    add_trace( ctx, "studio.allsolid", studio, box_start, box_end,
               pm->physents[3].info, ctx.indices.physents[3] );

    ++ctx.callback_ordinal;
    const auto glass_hit = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 4, ctx.indices.physents.data() + 4, 1 ),
        -1, nullptr );
    ++ctx.callback_ordinal;
    const auto glass_ignored = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, abi::k_pm_glass_ignore,
        list_view( pm->physents + 4, ctx.indices.physents.data() + 4, 1 ),
        -1, nullptr );
    CHECK( glass_hit.fraction < 1.0f );
    CHECK( glass_ignored.fraction == 1.0f );
    add_trace( ctx, "glass.hit.allsolid", glass_hit, box_start, box_end,
               pm->physents[4].info, ctx.indices.physents[4] );
    add_trace( ctx, "glass.ignore.allsolid", glass_ignored, box_start, box_end,
               pm->physents[4].info, ctx.indices.physents[4] );

    ++ctx.callback_ordinal;
    const auto filtered = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 6, ctx.indices.physents.data() + 6, 1 ),
        -1, &filter_marked_physent );
    CHECK( filtered.fraction == 1.0f );
    add_trace( ctx, "filter.allsolid", filtered, box_start, box_end,
               pm->physents[6].info, ctx.indices.physents[6] );

    ++ctx.callback_ordinal;
    const auto rotated = phy::pm_player_trace_ext(
        ctx.env, *pm, start, end, 0,
        list_view( pm->physents + 7, ctx.indices.physents.data() + 7, 1 ),
        -1, nullptr );
    // Q-18 owns legacy ULP parity; this witness asserts role identity only.
    add_trace( ctx, "rotated.allsolid", rotated, start, end,
               pm->physents[7].info, ctx.indices.physents[7] );

    ++ctx.callback_ordinal;
    pm->usehull = 0;
    const auto standing = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 1, ctx.indices.physents.data() + 1, 1 ),
        -1, nullptr );
    ++ctx.callback_ordinal;
    pm->usehull = 2;
    const auto point = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents + 1, ctx.indices.physents.data() + 1, 1 ),
        -1, nullptr );
    add_trace( ctx, "hull.standing.allsolid", standing, box_start, box_end,
               pm->physents[1].info, ctx.indices.physents[1] );
    add_trace( ctx, "hull.point.allsolid", point, box_start, box_end,
               pm->physents[1].info, ctx.indices.physents[1] );

    ++ctx.callback_ordinal;
    const auto phys_line = phy::pm_trace_line(
        ctx.env, *pm, start, end, abi::k_pm_traceline_physentsonly, 2, -1 );
    ++ctx.callback_ordinal;
    const auto vis_line = phy::pm_trace_line(
        ctx.env, *pm, start, end, abi::k_pm_traceline_anyvisible, 2, -1 );
    add_trace( ctx, "physents.line.allsolid", phys_line, start, end,
               pm->physents[phys_line.ent >= 0 ? phys_line.ent : 0].info,
               ctx.indices.physents[static_cast<std::size_t>(
                   phys_line.ent >= 0 ? phys_line.ent : 0 )] );
    add_trace( ctx, "visents.line.allsolid", vis_line, start, end,
               pm->visents[vis_line.ent >= 0 ? vis_line.ent : 0].info,
               ctx.indices.visents[static_cast<std::size_t>(
                   vis_line.ent >= 0 ? vis_line.ent : 0 )] );

    ++ctx.callback_ordinal;
    const auto move_line = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->moveents, ctx.indices.moveents.data(),
                   static_cast<std::size_t>( pm->nummoveent )), -1, nullptr );
    add_trace( ctx, "moveents.line.allsolid", move_line, box_start, box_end,
               pm->moveents[0].info, ctx.indices.moveents[0] );

    ++ctx.callback_ordinal;
    const int world_contents =
        phy::pm_true_point_contents( ctx.env, *pm, Vec3{ 0, 0, 0 } );
    const int merged_contents =
        phy::pm_point_contents( ctx.env, *pm, Vec3{ 0, 0, 0 } );
    int true_contents = 0;
    const int folded_contents = phy::pm_point_contents_pmove(
        ctx.env, *pm, Vec3{ 0, 0, 0 }, &true_contents );
    ctx.projection.integer( "contents.world", world_contents );
    ctx.projection.integer( "contents.merged", merged_contents );
    ctx.projection.integer( "contents.true", true_contents );
    ctx.projection.integer( "contents.folded", folded_contents );

    // Direct movevars injection: this is an input-identity claim, not a
    // transport/receiver claim.
    ctx.projection.floating( "movevars.gravity.bits", pm->movevars->gravity );
    ctx.projection.floating( "movevars.maxspeed.bits", pm->movevars->maxspeed );
    ctx.projection.floating( "movevars.stepsize.bits", pm->movevars->stepsize );
    ctx.projection.floating( "movevars.friction.bits", pm->movevars->friction );

    for ( int i = 0; i < pm->numphysent; ++i )
    {
        ctx.projection.integer( "sidecar.physent.info", pm->physents[i].info );
        ctx.projection.integer(
            "sidecar.physent.model",
            ctx.indices.physents[static_cast<std::size_t>( i )] );
    }
    for ( int i = 0; i < pm->numvisent; ++i )
        ctx.projection.integer(
            "sidecar.visent.model",
            ctx.indices.visents[static_cast<std::size_t>( i )] );
    for ( int i = 0; i < pm->nummoveent; ++i )
        ctx.projection.integer(
            "sidecar.moveent.model",
            ctx.indices.moveents[static_cast<std::size_t>( i )] );

    // Reuse the same frozen working set with a freshly replaced valid prefix.
    // Stale entries beyond numphysent remain deliberately untouched.
    pm->physents[0] = pm->moveents[0];
    ctx.indices.physents[0] = ctx.indices.moveents[0];
    pm->numphysent = 1;
    ++ctx.callback_ordinal;
    const auto reused = phy::pm_player_trace_ext(
        ctx.env, *pm, box_start, box_end, 0,
        list_view( pm->physents, ctx.indices.physents.data(), 1 ), -1, nullptr );
    add_trace( ctx, "reuse.line.allsolid", reused, box_start, box_end,
               pm->physents[0].info, ctx.indices.physents[0] );

    const int random_a = pm->RandomLong( -50, 50 );
    const int random_equal = pm->RandomLong( 7, 7 ); // still consumes a draw
    const float random_f = pm->RandomFloat( -1.0f, 1.0f );
    ctx.projection.integer( "rng.long", random_a );
    ctx.projection.integer( "rng.equal", random_equal );
    ctx.projection.floating( "rng.float.bits", random_f );
    ctx.projection.integer( "rng.draw.position", ctx.rng_draw_position );
    ctx.projection.integer( "callback.final.ordinal", ctx.callback_ordinal );
}

static_assert( std::is_same_v<decltype( &synthetic_pm_move_probe ),
                              PmMoveSignature> );

[[nodiscard]] bool compare_projection( const Projection &a,
                                       const Projection &b ) noexcept
{
    if ( a.count != b.count )
    {
        std::printf( "projection size mismatch: %zu vs %zu\n", a.count, b.count );
        return false;
    }
    for ( std::size_t i = 0; i < a.count; ++i )
    {
        if ( std::string_view( a.fields[i].name ) == b.fields[i].name &&
             a.fields[i].value == b.fields[i].value )
            continue;
        std::printf(
            "first divergent field #%zu %s/%s: 0x%016llX vs 0x%016llX "
            "(float fields are IEEE hex; projection also records callback "
            "ordinal, physent info/model index, trace inputs, RNG draw position)\n",
            i, a.fields[i].name, b.fields[i].name,
            static_cast<unsigned long long>( a.fields[i].value ),
            static_cast<unsigned long long>( b.fields[i].value ) );
        return false;
    }
    return true;
}

[[nodiscard]] ml::WorldData make_world()
{
    auto builder = test_bsp::make_minimal_world();
    std::vector<test_bsp::bsp::dmodel_t> models( 2 );
    models[0] = { { -64, -64, -64 }, { 64, 64, 64 }, {},
                  { 0, 0, -1, -1 }, 2, 0, 2 };
    models[1] = { { -16, -16, -16 }, { 16, 16, 16 }, {},
                  { 0, 0, 0, 0 }, 0, 0, 0 };
    builder.set_lump_records( test_bsp::bsp::k_lump_models, models );

    ml::WorldLoadOptions options;
    auto loaded = ml::load_world_data( builder.build(), "role-parity", options );
    REQUIRE( loaded.has_value() );
    return std::move( *loaded );
}

void test_role_parity_projection()
{
    ml::WorldData world = make_world();

    auto pm_server = std::make_unique<abi::playermove_t>();
    abi::movevars_t movevars_server {};
    ProbeContext server( world );
    initialize_fixture( server, *pm_server, movevars_server, 1 );
    g_probe = &server;
    synthetic_pm_move_probe( pm_server.get(), 1 );

    auto pm_client = std::make_unique<abi::playermove_t>();
    abi::movevars_t movevars_client {};
    ProbeContext client( world );
    initialize_fixture( client, *pm_client, movevars_client, 0 );
    g_probe = &client;
    synthetic_pm_move_probe( pm_client.get(), 0 );
    g_probe = nullptr;

    CHECK( compare_projection( server.projection, client.projection ) );
    CHECK_EQ( server.rng_draw_position, 3 );
    CHECK_EQ( client.rng_draw_position, 3 );
}

} // namespace

int main()
{
    ::xash::core::register_thread_role( ::xash::core::ThreadRole::Main );
    RUN_TEST( test_role_parity_projection );
    std::printf( "physics_role_parity: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
