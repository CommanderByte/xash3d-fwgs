// xash3dpp — areanode linking behaviour pins (Chunk 6 S5a)
// Covers: tree construction (depth-4, longer-of-X/Y split, node budget),
// link/unlink list membership by solid class, the SetAbsBox hook contract,
// world/freed-entity skips, the SOLID_NOT skin gate (water bmodels DO
// link), leaf-cluster fill + headnode overflow fallback over a real
// WorldData, aiment-follow leaf copy, trigger touching (filters, group
// AND/NAND, AABB reject, playersonly suppression) and the touch-links
// recursion semaphore.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/world/links.hpp>

#include "../map_loader/bsp/test_bsp_builder.hpp"

#include "../test_helpers.hpp"

#include <optional>
#include <vector>

namespace sv  = xash::server;
namespace wr  = ::xash::world;
namespace abi = xash::abi;
namespace ml  = xash::map_loader;
using xash::utilities::Vec3;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// fixtures
// ---------------------------------------------------------------------------

namespace {

// Test hooks: SetAbsBox = origin + mins/maxs (no HLSDK expansion); records
// touch dispatches.
struct TestHooks : wr::IWorldLinkHooks
{
    int  abs_box_calls = 0;
    bool reject_brush  = false;
    std::vector<std::pair<abi::edict_t *, abi::edict_t *>> touches;

    void set_abs_box( abi::edict_t *ent ) noexcept override
    {
        ++abs_box_calls;
        sv::EntityView v( ent );
        const Vec3 org = v.origin();
        const Vec3 mn = v.mins(), mx = v.maxs();
        v.set_absmin( { org.x + mn.x, org.y + mn.y, org.z + mn.z } );
        v.set_absmax( { org.x + mx.x, org.y + mx.y, org.z + mx.z } );
    }

    void dispatch_touch( abi::edict_t *trigger,
                         abi::edict_t *other ) noexcept override
    {
        touches.emplace_back( trigger, other );
    }

    [[nodiscard]] bool
    brush_trigger_intersects( abi::edict_t *, abi::edict_t * ) noexcept override
    {
        return !reject_brush;
    }
};

struct WorldFixture
{
    xash::memory::PoolHandle       pool;
    sv::EdictArena                 arena;
    wr::WorldLinks                 links;
    TestHooks                      hooks;
    std::optional<ml::WorldData>   world;
    wr::LinkEnv                    env;

    WorldFixture()
    {
        pool = xash::memory::create_pool( "test_world_links" );
        REQUIRE( static_cast<bool>( pool ));
        REQUIRE( arena.init( pool, 32, 2 ));

        ml::WorldLoadOptions opts;
        opts.is_world = true;
        auto w = ml::load_world_data( test_bsp::make_minimal_world().build(),
                                      "t", opts );
        REQUIRE( w.has_value() );
        world.emplace( std::move( *w ));

        links.set_hooks( &hooks );
        links.clear_world( { -256, -256, -256 }, { 256, 256, 256 } );

        env.world       = &*world;
        env.worldspawn  = arena.edict_num( 0 );
        env.playersonly = false;
    }

    ~WorldFixture()
    {
        arena.shutdown();
        xash::memory::destroy_pool( pool );
    }

    abi::edict_t *spawn( Vec3 origin, Vec3 mins, Vec3 maxs, int solid )
    {
        abi::edict_t *e = arena.alloc_edict( 1.0 );
        REQUIRE( e != nullptr );
        sv::store_vec3( e->v.origin, origin );
        sv::store_vec3( e->v.mins, mins );
        sv::store_vec3( e->v.maxs, maxs );
        e->v.solid = solid;
        return e;
    }
};

[[nodiscard]] std::size_t list_length( const abi::link_t &sentinel ) noexcept
{
    std::size_t n = 0;
    for ( const abi::link_t *l = sentinel.next; l != &sentinel; l = l->next )
        ++n;
    return n;
}

[[nodiscard]] std::size_t count_lists( const wr::AreaNode *node, int which )
{
    if ( node == nullptr )
        return 0;
    const abi::link_t &s = which == 0 ? node->trigger_edicts
                         : which == 1 ? node->solid_edicts
                                      : node->portal_edicts;
    return list_length( s ) + count_lists( node->children[0], which ) +
           count_lists( node->children[1], which );
}

} // namespace

// ---------------------------------------------------------------------------
// tree construction
// ---------------------------------------------------------------------------

static void test_tree_shape()
{
    WorldFixture f;

    // Depth-4 full binary tree = 31 nodes, within the 32-node pool.
    CHECK_EQ( f.links.node_count(), 31u );

    // Root splits the longer of X/Y; the fixture box is a cube → Y (tie
    // goes to Y: size.x > size.y is false).
    CHECK_EQ( f.links.root()->axis, 1 );
    CHECK_EQ( f.links.root()->dist, 0.0f );
}

// ---------------------------------------------------------------------------
// link / unlink membership
// ---------------------------------------------------------------------------

static void test_link_membership_by_solid()
{
    WorldFixture f;

    abi::edict_t *solid = f.spawn( { 10, 10, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_bbox );
    abi::edict_t *trig  = f.spawn( { -10, -10, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_trigger );
    abi::edict_t *portal = f.spawn( { 0, 40, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                    abi::k_solid_portal );

    f.links.link_edict( solid, false, f.env );
    f.links.link_edict( trig, false, f.env );
    f.links.link_edict( portal, false, f.env );

    CHECK_EQ( f.hooks.abs_box_calls, 3 );
    CHECK_EQ( count_lists( f.links.root(), 1 ), 1u ); // solids
    CHECK_EQ( count_lists( f.links.root(), 0 ), 1u ); // triggers
    CHECK_EQ( count_lists( f.links.root(), 2 ), 1u ); // portals
    CHECK( f.links.linked( solid ));

    // Relink is idempotent (unlinks from the old position first).
    f.links.link_edict( solid, false, f.env );
    CHECK_EQ( count_lists( f.links.root(), 1 ), 1u );

    wr::WorldLinks::unlink_edict( solid );
    CHECK( !f.links.linked( solid ));
    CHECK_EQ( count_lists( f.links.root(), 1 ), 0u );

    // Unlink when not linked is a no-op.
    wr::WorldLinks::unlink_edict( solid );
    CHECK( !f.links.linked( solid ));
}

static void test_link_skips_world_and_freed()
{
    WorldFixture f;

    // Worldspawn is never linked.
    f.links.link_edict( f.env.worldspawn, false, f.env );
    CHECK( !f.links.linked( f.env.worldspawn ));

    // Freed entities are never linked.
    abi::edict_t *e = f.spawn( { 0, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                               abi::k_solid_bbox );
    f.arena.free_edict( e, 1.0 );
    e->v.solid = abi::k_solid_bbox; // scrubbed by free — restore for the test
    f.links.link_edict( e, false, f.env );
    CHECK( !f.links.linked( e ));
}

static void test_solid_not_skin_gate()
{
    WorldFixture f;

    // SOLID_NOT with ordinary skin: not linked at all.
    abi::edict_t *deco = f.spawn( { 0, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                  abi::k_solid_not );
    deco->v.skin = 0;
    f.links.link_edict( deco, false, f.env );
    CHECK( !f.links.linked( deco ));

    // SOLID_NOT water bmodel (skin < CONTENTS_EMPTY) links into solids.
    abi::edict_t *water = f.spawn( { 0, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_not );
    water->v.skin = -3; // CONTENTS_WATER
    f.links.link_edict( water, false, f.env );
    CHECK( f.links.linked( water ));
    CHECK_EQ( count_lists( f.links.root(), 1 ), 1u );
}

// ---------------------------------------------------------------------------
// leaf fill
// ---------------------------------------------------------------------------

static void test_leaf_fill_clusters()
{
    WorldFixture f;

    // Fixture world: plane x=128 splits leaf 1 (cluster 0) / leaf 2
    // (cluster 1).  A box straddling it touches both clusters.
    abi::edict_t *e = f.spawn( { 128, 0, 0 }, { -16, -16, -16 },
                               { 16, 16, 16 }, abi::k_solid_bbox );
    e->v.modelindex = 1; // leaf walk requires a model
    f.links.link_edict( e, false, f.env );

    CHECK_EQ( e->num_leafs, 2 );
    CHECK_EQ( e->leafnums16[0], 0 );
    CHECK_EQ( e->leafnums16[1], 1 );
    CHECK_EQ( e->headnode, -1 );

    // No modelindex → no leaf walk.
    abi::edict_t *point = f.spawn( { 200, 0, 0 }, {}, {}, abi::k_solid_bbox );
    f.links.link_edict( point, false, f.env );
    CHECK_EQ( point->num_leafs, 0 );
    CHECK_EQ( point->headnode, -1 );
}

static void test_aiment_follow_copies_leafs()
{
    WorldFixture f;

    abi::edict_t *host = f.spawn( { 128, 0, 0 }, { -16, -16, -16 },
                                  { 16, 16, 16 }, abi::k_solid_bbox );
    host->v.modelindex = 1;
    f.links.link_edict( host, false, f.env );
    REQUIRE( host->num_leafs == 2 );

    abi::edict_t *follower = f.spawn( { 0, 0, 0 }, { -4, -4, -4 },
                                      { 4, 4, 4 }, abi::k_solid_bbox );
    follower->v.movetype = abi::k_movetype_follow;
    follower->v.aiment   = host;
    f.links.link_edict( follower, false, f.env );

    CHECK_EQ( follower->num_leafs, 2 );
    CHECK_EQ( follower->leafnums16[0], 0 );
    CHECK_EQ( follower->leafnums16[1], 1 );
}

// ---------------------------------------------------------------------------
// trigger touching
// ---------------------------------------------------------------------------

static void test_touch_filters_and_dispatch()
{
    WorldFixture f;

    abi::edict_t *trig = f.spawn( { 0, 0, 0 }, { -16, -16, -16 },
                                  { 16, 16, 16 }, abi::k_solid_trigger );
    f.links.link_edict( trig, false, f.env );

    // Overlapping mover → dispatched.
    abi::edict_t *mover = f.spawn( { 8, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_slidebox );
    f.links.link_edict( mover, true, f.env );
    REQUIRE( f.hooks.touches.size() == 1u );
    CHECK( f.hooks.touches[0].first == trig );
    CHECK( f.hooks.touches[0].second == mover );

    // AABB reject: distant mover → no dispatch.
    abi::edict_t *far_e = f.spawn( { 200, 200, 200 }, { -8, -8, -8 },
                                   { 8, 8, 8 }, abi::k_solid_slidebox );
    f.links.link_edict( far_e, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );

    // playersonly suppresses dispatch.
    f.env.playersonly = true;
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );
    f.env.playersonly = false;

    // Brush refinement hook can reject the AABB hit.
    f.hooks.reject_brush = true;
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );
    f.hooks.reject_brush = false;

    // Disabled trigger (solid changed) → skipped.
    trig->v.solid = abi::k_solid_not;
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );
}

static void test_touch_group_policy()
{
    WorldFixture f;

    abi::edict_t *trig = f.spawn( { 0, 0, 0 }, { -16, -16, -16 },
                                  { 16, 16, 16 }, abi::k_solid_trigger );
    trig->v.groupinfo = 0x2;
    f.links.link_edict( trig, false, f.env );

    abi::edict_t *mover = f.spawn( { 0, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_slidebox );
    mover->v.groupinfo = 0x1;

    // AND policy with disjoint groups → skipped.
    f.links.set_group_op( wr::GroupOp::And );
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 0u );

    // Overlapping groups → dispatched.
    mover->v.groupinfo = 0x3;
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );

    // NAND policy inverts: overlap → skipped.
    f.links.set_group_op( wr::GroupOp::Nand );
    f.links.link_edict( mover, true, f.env );
    CHECK_EQ( f.hooks.touches.size(), 1u );
}

namespace {

// Hooks whose touch dispatch relinks the toucher — exercises the
// iTouchLinkSemaphore recursion guard.
struct RecursiveHooks final : TestHooks
{
    wr::WorldLinks *links = nullptr;
    wr::LinkEnv    *env   = nullptr;
    int             depth = 0;

    void dispatch_touch( abi::edict_t *trigger,
                         abi::edict_t *other ) noexcept override
    {
        TestHooks::dispatch_touch( trigger, other );
        REQUIRE( depth == 0 ); // semaphore must prevent nesting
        ++depth;
        links->link_edict( other, true, *env ); // would recurse unguarded
        --depth;
    }
};

} // namespace

static void test_touch_semaphore_blocks_recursion()
{
    WorldFixture f;
    RecursiveHooks hooks;
    hooks.links = &f.links;
    hooks.env   = &f.env;
    f.links.set_hooks( &hooks );

    abi::edict_t *trig = f.spawn( { 0, 0, 0 }, { -16, -16, -16 },
                                  { 16, 16, 16 }, abi::k_solid_trigger );
    f.links.link_edict( trig, false, f.env );

    abi::edict_t *mover = f.spawn( { 0, 0, 0 }, { -8, -8, -8 }, { 8, 8, 8 },
                                   abi::k_solid_slidebox );
    f.links.link_edict( mover, true, f.env );

    // Exactly one dispatch: the nested link ran with the semaphore held,
    // so its touch pass was skipped.
    CHECK_EQ( hooks.touches.size(), 1u );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_tree_shape );
    RUN_TEST( test_link_membership_by_solid );
    RUN_TEST( test_link_skips_world_and_freed );
    RUN_TEST( test_solid_not_skin_gate );
    RUN_TEST( test_leaf_fill_clusters );
    RUN_TEST( test_aiment_follow_copies_leafs );
    RUN_TEST( test_touch_filters_and_dispatch );
    RUN_TEST( test_touch_group_policy );
    RUN_TEST( test_touch_semaphore_blocks_recursion );

    std::printf( "world_links: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
