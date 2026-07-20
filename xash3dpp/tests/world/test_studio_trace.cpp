// xash3dpp — studio hitbox trace-loop pins (Chunk 7 OQ-2 close).
// Covers: SV_StudioPlayerBlend (window math vs hand-computed legacy values),
// the StudioHullCache legacy pooled ring (exact-key hits, ring eviction,
// pool-exhaustion wholesale clear), the SV_HullForStudioModel gating matrix
// (zero-size complex upgrade, FTRACE_SIMPLEBOX suppression, sv_clienttrace
// zero/scale, client pose override, CS shield flag), and the per-hitbox
// clip merge as a COMPOSITION ORACLE: the loop's result must equal manually
// merging per-hull kernel traces (the kernel + BoxHull::set_planes are
// already golden-verified; only the selection/merge logic is new).

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/trace.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/private/server/model_resolver.hpp> // StudioHullCache
#include <xash3dpp/world/trace.hpp>

#include "../test_helpers.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

namespace sv  = xash::server;
namespace wr  = ::xash::world;
namespace abi = xash::abi;
namespace ml  = xash::map_loader;
namespace ct  = xash::content;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

namespace {

// --- cmd_cvar stubs (mirrors tests/cmd_cvar/test_stubs) --------------------

struct Oracle final : xash::cmd_cvar::ITrustOracle
{
    bool stuffcmd_is_trusted() const noexcept override { return true; }
};

struct Policy final : xash::cmd_cvar::ICompatPolicy
{
    const char *redirect_cvar_name( std::string_view ) const noexcept override { return nullptr; }
    bool is_filterable_exempt( std::string_view ) const noexcept override { return false; }
    bool is_overridable_command( std::string_view ) const noexcept override { return false; }
};

// --- synthetic studio header: 244-byte header + one seqdesc ----------------

void wr_i32( std::vector<std::byte> &b, std::size_t off, std::int32_t v )
{
    std::memcpy( b.data() + off, &v, 4 );
}

void wr_f32( std::vector<std::byte> &b, std::size_t off, float v )
{
    std::memcpy( b.data() + off, &v, 4 );
}

[[nodiscard]] std::vector<std::byte>
make_seq_bytes( float blend_start, float blend_end )
{
    std::vector<std::byte> b( ct::k_studio_header_size +
                              ct::k_studio_seqdesc_stride );
    wr_i32( b, 0, ct::k_studio_ident );
    wr_i32( b, 4, ct::k_studio_version );
    wr_i32( b, 72, static_cast<std::int32_t>( b.size())); // length
    wr_i32( b, 164, 1 );   // numseq
    wr_i32( b, 168, 244 ); // seqindex
    wr_f32( b, 244 + 136, blend_start );
    wr_f32( b, 244 + 144, blend_end );
    return b;
}

// --- fixture resolvers -----------------------------------------------------

// Serves hand-made hulls (the merge-loop oracle) — bytes not involved.
struct HullResolver final : wr::IModelResolver
{
    std::vector<ct::StudioHitboxHull> hulls;

    std::optional<wr::BrushModel> brush_model( int ) noexcept override
    {
        return std::nullopt;
    }
    bool is_studio( int ) noexcept override { return true; }
    int studio_hulls( int, const wr::StudioHullPose &,
                      std::span<ct::StudioHitboxHull> out ) noexcept override
    {
        const std::size_t n = hulls.size() < out.size() ? hulls.size() : out.size();
        for ( std::size_t i = 0; i < n; ++i )
            out[i] = hulls[i];
        return static_cast<int>( n );
    }
};

// Serves a synthetic studio byte image (the gating/blend tests).
struct BytesResolver final : wr::IModelResolver
{
    std::vector<std::byte> bytes;

    std::optional<wr::BrushModel> brush_model( int ) noexcept override
    {
        return std::nullopt;
    }
    bool is_studio( int ) noexcept override { return true; }
    std::span<const std::byte> studio_bytes( int ) noexcept override
    {
        return bytes;
    }
};

// Axis-aligned oriented-box hull (the studio_hitbox_hulls plane pairing:
// per axis {+axis, center·axis + half} then {+axis, center·axis − half}).
[[nodiscard]] ct::StudioHitboxHull
make_box( const Vec3 &center, float half, int hitgroup )
{
    ct::StudioHitboxHull h;
    const Vec3 axes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    const float c[3]   = { center.x, center.y, center.z };
    for ( int k = 0; k < 3; ++k )
    {
        h.planes[2 * k].normal     = axes[k];
        h.planes[2 * k].dist       = c[k] + half;
        h.planes[2 * k + 1].normal = axes[k];
        h.planes[2 * k + 1].dist   = c[k] - half;
    }
    h.hitgroup = hitgroup;
    return h;
}

[[nodiscard]] ml::TraceResult manual_trace( const ct::StudioHitboxHull &h,
                                            const Vec3 &start, const Vec3 &end )
{
    ml::BoxHull box;
    ml::TracePlane planes[6];
    for ( int p = 0; p < 6; ++p )
    {
        planes[p].normal = h.planes[p].normal;
        planes[p].dist   = h.planes[p].dist;
    }
    return ml::trace_hull( box.set_planes( planes ), start, end );
}

[[nodiscard]] std::uint32_t bits( float f )
{
    std::uint32_t u;
    std::memcpy( &u, &f, 4 );
    return u;
}

[[nodiscard]] abi::edict_t make_studio_edict()
{
    abi::edict_t ed{};
    ed.v.solid      = abi::k_solid_bbox;
    ed.v.modelindex = 5;
    return ed;
}

// --- tests -----------------------------------------------------------------

void test_player_blend()
{
    const auto bytes = make_seq_bytes( 30.0f, 60.0f );
    const ct::StudioView hdr( bytes );
    const auto seq = hdr.seqdesc( 0 );

    // below the window: pitch*3 = 15 < 30 → blend 0, pitch -= start/3
    int blend = -1; float pitch = 5.0f;
    wr::studio_player_blend( seq, &blend, &pitch );
    CHECK( blend == 0 );
    CHECK( pitch == 5.0f - 30.0f / 3.0f );

    // above the window: 90 > 60 → blend 255, pitch -= end/3
    blend = -1; pitch = 30.0f;
    wr::studio_player_blend( seq, &blend, &pitch );
    CHECK( blend == 255 );
    CHECK( pitch == 30.0f - 60.0f / 3.0f );

    // inside: pitch 15 → 45; 255*(45-30)/(60-30) = 127 (int trunc), pitch 0
    blend = -1; pitch = 15.0f;
    wr::studio_player_blend( seq, &blend, &pitch );
    CHECK( blend == static_cast<int>( 255.0f * 15.0f / 30.0f ));
    CHECK( pitch == 0.0f );

    // degenerate window (< 0.1) → 127 (the qc-error catch)
    const auto degen = make_seq_bytes( 0.0f, 0.0f );
    const ct::StudioView dh( degen );
    blend = -1; pitch = 0.0f;
    wr::studio_player_blend( dh.seqdesc( 0 ), &blend, &pitch );
    CHECK( blend == 127 );
}

void test_hull_cache()
{
    sv::StudioHullCache cache;
    sv::StudioHullCache::Key key;
    key.modelindex = 5;
    key.frame      = 1.5f;
    key.sequence   = 2;

    const ct::StudioHitboxHull h = make_box( { 0, 0, 0 }, 8.0f, 3 );
    cache.add( key, std::span<const ct::StudioHitboxHull>( &h, 1 ));

    // exact hit
    auto hit = cache.find( key );
    CHECK( hit.size() == 1u );
    CHECK( hit[0].hitgroup == 3 );

    // near-miss: one float differs (legacy exact-compare)
    auto k2 = key;
    k2.frame = 1.5000001f;
    CHECK( cache.find( k2 ).empty() );

    // ring eviction: 16 more distinct adds push the original out
    for ( int i = 0; i < 16; ++i )
    {
        auto ki = key;
        ki.sequence = 100 + i;
        cache.add( ki, std::span<const ct::StudioHitboxHull>( &h, 1 ));
    }
    CHECK( cache.find( key ).empty() );

    // pool exhaustion clears EVERYTHING (legacy Mod_AddToStudioCache reset):
    // 100 + 100 hulls > 128 → the second add wipes the first.
    cache.clear();
    std::vector<ct::StudioHitboxHull> big( 100, h );
    cache.add( key, std::span<const ct::StudioHitboxHull>( big.data(), big.size()));
    CHECK( cache.find( key ).size() == 100u );
    auto k3 = key;
    k3.sequence = 9;
    cache.add( k3, std::span<const ct::StudioHitboxHull>( big.data(), big.size()));
    CHECK( cache.find( key ).empty() );      // wiped by the wholesale clear
    CHECK( cache.find( k3 ).size() == 100u ); // the trigger add landed after it
}

void test_pose_gating()
{
    Oracle oracle;
    Policy policy;
    xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ));

    BytesResolver resolver;
    resolver.bytes = make_seq_bytes( 0.0f, 0.0f ); // degenerate window → 127

    int trace_flags = 0;
    wr::MoveEnv env;
    env.models      = &resolver;
    env.cvars       = &ctx;
    env.trace_flags = &trace_flags;

    abi::edict_t ed = make_studio_edict();

    // (a) zero-size box, no simplebox → complex upgrade, scale 0.5
    auto pose = wr::studio_pose_for_entity( env, &ed, {}, {} );
    REQUIRE( pose.has_value() );
    CHECK( pose->force_complex );
    CHECK( !pose->skip_shield );
    CHECK( pose->use_cache ); // r_studiocache absent → default-on

    // (b) FTRACE_SIMPLEBOX suppresses the upgrade
    trace_flags = wr::k_ftrace_simplebox;
    pose = wr::studio_pose_for_entity( env, &ed, {}, {} );
    REQUIRE( pose.has_value() );
    CHECK( !pose->force_complex );
    trace_flags = 0;

    // (c) sized box → no upgrade, size scaled by 0.5
    pose = wr::studio_pose_for_entity( env, &ed, { -8, -8, -8 }, { 8, 8, 8 } );
    REQUIRE( pose.has_value() );
    CHECK( !pose->force_complex );
    CHECK( pose->size.x == 8.0f && pose->size.y == 8.0f && pose->size.z == 8.0f );

    // (d) client + sv_clienttrace 0 → hitbox trace disabled
    (void)ctx.cvar_get_or_create( "sv_clienttrace", "0", 0 );
    ed.v.flags = abi::k_fl_client;
    pose = wr::studio_pose_for_entity( env, &ed, {}, {} );
    REQUIRE( pose.has_value() );
    CHECK( !pose->force_complex );

    // (e) client + sv_clienttrace 0.8 → size (0.4,0.4,0.4), pose override
    ctx.cvar_set( "sv_clienttrace", "0.8" );
    pose = wr::studio_pose_for_entity( env, &ed, {}, {} );
    REQUIRE( pose.has_value() );
    CHECK( pose->force_complex );
    CHECK( pose->size.x == 0.8f * 0.5f );
    CHECK( pose->controllers[0] == 0x7F && pose->controllers[3] == 0x7F );
    CHECK( pose->blending[0] == 127 ); // degenerate blend window
    CHECK( pose->blending[1] == 0 );

    // (f) CS shield: gamestate 1 → skip flag
    ed.v.gamestate = 1;
    pose = wr::studio_pose_for_entity( env, &ed, {}, {} );
    REQUIRE( pose.has_value() );
    CHECK( pose->skip_shield );

    ctx.shutdown();
}

void test_clip_merge_oracle()
{
    HullResolver resolver;
    resolver.hulls.push_back( make_box( { 64, 0, 0 }, 16.0f, 3 ));  // A
    resolver.hulls.push_back( make_box( { 128, 0, 0 }, 16.0f, 5 )); // B

    wr::MoveEnv env;
    env.models = &resolver;

    abi::edict_t ed = make_studio_edict();

    // Ray from +x toward the origin: B (nearer the start) must win with the
    // smaller fraction and stamp ITS hitgroup.
    const Vec3 start{ 200, 0, 0 }, end{ 0, 0, 0 };
    const wr::SvTrace tr =
        wr::clip_move_to_entity( env, &ed, start, {}, {}, end );

    const ml::TraceResult ta = manual_trace( resolver.hulls[0], start, end );
    const ml::TraceResult tb = manual_trace( resolver.hulls[1], start, end );
    CHECK( tb.fraction < ta.fraction ); // sanity: B really is nearer
    CHECK_EQ( bits( tr.t.fraction ), bits( tb.fraction ));
    CHECK( tr.hitgroup == 5 );
    CHECK( tr.ent == &ed );
    // finalize (non-rotated): plane.dist recomputed as endpos·normal
    CHECK_EQ( bits( tr.t.plane.dist ),
              bits( xash::utilities::dot( tr.t.endpos, tr.t.plane.normal )));

    // start inside A: startsolid must survive the merge (sticky) + ent set
    const wr::SvTrace ts =
        wr::clip_move_to_entity( env, &ed, { 64, 0, 0 }, {}, {}, { 300, 0, 0 } );
    CHECK( ts.t.startsolid );
    CHECK( ts.ent == &ed );

    // clean miss: fraction 1, no ent. LEGACY QUIRK: sv_world.c stamps
    // `trace->hitgroup = Mod_HitgroupForStudioHull(last_hitgroup)`
    // UNCONDITIONALLY after the loop, so a full miss leaks hull 0's
    // hitgroup (3 here) — faithfully reproduced.
    const wr::SvTrace tm =
        wr::clip_move_to_entity( env, &ed, { 200, 100, 0 }, {}, {}, { 0, 100, 0 } );
    CHECK( tm.t.fraction == 1.0f );
    CHECK( tm.ent == nullptr );
    CHECK( tm.hitgroup == 3 );
}

} // namespace

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_player_blend );
    RUN_TEST( test_hull_cache );
    RUN_TEST( test_pose_gating );
    RUN_TEST( test_clip_merge_oracle );

    std::printf( "studio_trace: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
