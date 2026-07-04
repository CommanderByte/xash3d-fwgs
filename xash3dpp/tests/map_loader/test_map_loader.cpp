// xash3dpp — MapLoader FSM tests (Chunk 5, C1)
// Covers the pre-BSP scaffold surface: init/shutdown/re-init, transition
// queueing (new_game/load_level/load_game/change_level), run_frame_step
// dispatch + observer notifications, observer attach/detach and the fixed
// 4-slot table, and MAX_QPATH name truncation (limits::map_qpath_max).
//
// The LoadLevel state gains real world loading in C9; the notify sequence
// pinned here (begin/end around every state change, including the return to
// RunFrame) is the scaffold contract and is revisited there.

#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>

#include "../test_helpers.hpp"

#include <string>
#include <string_view>
#include <vector>

using xash::IMapLoaderObserver;
using xash::MapLoader;
using xash::MapLoaderInitParams;
using xash::MapLoadState;

static int g_pass = 0, g_fail = 0;

namespace
{

struct RecordingObserver final : IMapLoaderObserver
{
    struct Event
    {
        bool         begin;   // true = on_load_begin, false = on_load_end
        std::string  map;
        MapLoadState reason;  // begin only
        bool         success; // end only
    };

    std::vector<Event> events;

    void on_load_begin( std::string_view map, MapLoadState reason ) noexcept override
    {
        events.push_back( { true, std::string( map ), reason, false } );
    }

    void on_load_end( std::string_view map, bool success ) noexcept override
    {
        events.push_back( { false, std::string( map ), MapLoadState::RunFrame, success } );
    }
};

} // namespace

// ---------------------------------------------------------------------------
// init / shutdown
// ---------------------------------------------------------------------------

static void test_init_shutdown_reinit()
{
    MapLoader ml;
    REQUIRE( ml.init( MapLoaderInitParams{} ) );
    CHECK( ml.state() == MapLoadState::RunFrame );
    CHECK( ml.current_map().empty() );

    // Idempotent re-init while initialised.
    CHECK( ml.init( MapLoaderInitParams{} ) );

    ml.shutdown();
    ml.shutdown(); // idempotent

    // Re-init after shutdown works.
    REQUIRE( ml.init( MapLoaderInitParams{} ) );
    CHECK( ml.state() == MapLoadState::RunFrame );
    ml.shutdown();
}

// ---------------------------------------------------------------------------
// transitions queue until run_frame_step
// ---------------------------------------------------------------------------

static void test_transitions_queue_until_step()
{
    MapLoader ml;
    REQUIRE( ml.init( MapLoaderInitParams{} ) );

    ml.new_game( "c1a0" );
    CHECK( ml.state() == MapLoadState::RunFrame ); // not applied yet
    CHECK_STREQ( ml.current_map().data(), "c1a0" );

    ml.run_frame_step();
    CHECK( ml.state() == MapLoadState::LoadLevel );

    // Second step returns the FSM to RunFrame (scaffold behaviour).
    ml.run_frame_step();
    CHECK( ml.state() == MapLoadState::RunFrame );

    // No pending transition → step is a no-op.
    ml.run_frame_step();
    CHECK( ml.state() == MapLoadState::RunFrame );

    ml.load_game( "save01" );
    ml.run_frame_step();
    CHECK( ml.state() == MapLoadState::LoadGame );
    CHECK_STREQ( ml.current_map().data(), "save01" );

    ml.change_level( "c1a1", "landmark_a", /*background=*/false );
    ml.run_frame_step();
    CHECK( ml.state() == MapLoadState::ChangeLevel );
    CHECK_STREQ( ml.current_map().data(), "c1a1" );

    ml.shutdown();
}

// ---------------------------------------------------------------------------
// observers
// ---------------------------------------------------------------------------

static void test_observer_notifications()
{
    MapLoader ml;
    REQUIRE( ml.init( MapLoaderInitParams{} ) );

    RecordingObserver obs;
    ml.attach_observer( &obs );

    ml.load_level( "de_dust", /*background=*/false );
    CHECK_EQ( obs.events.size(), std::size_t{ 0 } ); // nothing until the step

    ml.run_frame_step();
    REQUIRE( obs.events.size() == 2 );
    CHECK( obs.events[0].begin );
    CHECK( obs.events[0].reason == MapLoadState::LoadLevel );
    CHECK_STREQ( obs.events[0].map.c_str(), "de_dust" );
    CHECK( !obs.events[1].begin );
    CHECK( obs.events[1].success );

    // Detached observers stop receiving events.
    ml.detach_observer( &obs );
    ml.load_level( "de_aztec", false );
    ml.run_frame_step();
    CHECK_EQ( obs.events.size(), std::size_t{ 2 } );

    ml.shutdown();
}

static void test_observer_slot_table()
{
    MapLoader ml;
    REQUIRE( ml.init( MapLoaderInitParams{} ) );

    // The slot table holds 4 observers; a 5th attach is ignored.
    RecordingObserver obs[5];
    for ( auto &o : obs )
        ml.attach_observer( &o );

    ml.new_game( "c2a5" );
    ml.run_frame_step();

    for ( int i = 0; i < 4; ++i )
        CHECK_EQ( obs[i].events.size(), std::size_t{ 2 } );
    CHECK_EQ( obs[4].events.size(), std::size_t{ 0 } );

    // Drain the FSM back to RunFrame so the next transition is observable.
    ml.run_frame_step();
    for ( int i = 0; i < 4; ++i )
        CHECK_EQ( obs[i].events.size(), std::size_t{ 4 } );

    // Detaching frees a slot for a new attach.
    ml.detach_observer( &obs[1] );
    ml.attach_observer( &obs[4] );
    ml.new_game( "c2a5" );
    ml.run_frame_step();
    CHECK_EQ( obs[4].events.size(), std::size_t{ 2 } );
    CHECK_EQ( obs[1].events.size(), std::size_t{ 4 } ); // unchanged after detach

    // Null attach is ignored (no crash, no slot consumed).
    ml.attach_observer( nullptr );

    ml.shutdown();
}

// ---------------------------------------------------------------------------
// name truncation (limits::map_qpath_max)
// ---------------------------------------------------------------------------

static void test_map_name_truncation()
{
    MapLoader ml;
    REQUIRE( ml.init( MapLoaderInitParams{} ) );

    const std::string long_name( 3 * xash::limits::map_qpath_max, 'x' );
    ml.load_level( long_name, false );

    // Buffer keeps map_qpath_max-1 chars + null terminator.
    CHECK_EQ( ml.current_map().size(), xash::limits::map_qpath_max - 1 );

    ml.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_init_shutdown_reinit );
    RUN_TEST( test_transitions_queue_until_step );
    RUN_TEST( test_observer_notifications );
    RUN_TEST( test_observer_slot_table );
    RUN_TEST( test_map_name_truncation );

    std::printf( "map_loader: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
