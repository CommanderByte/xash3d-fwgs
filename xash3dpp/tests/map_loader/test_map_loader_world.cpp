// xash3dpp — MapLoader world integration (Chunk 5, C9)
// End-to-end through a REAL Filesystem: a synthetic BSP written to a temp
// game directory (tests/filesystem/test_backends.cpp precedent), loaded via
// the load_world_data Filesystem overload, the maps/<name>.ent entity-patch
// override (legacy Mod_LoadEntities:2356-2382), and the MapLoader FSM
// driving a synchronous LoadLevel with observer notifications + world()
// ownership.

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/map_loader/world.hpp>

#include "bsp/test_bsp_builder.hpp"

#include "../test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ml = xash::map_loader;
using test_bsp::make_minimal_world;
using xash::MapLoader;
using xash::MapLoaderInitParams;
using xash::MapLoadState;

static int g_pass = 0, g_fail = 0;

static std::filesystem::path g_root;

static void write_file( const std::filesystem::path &p, const void *data, std::size_t len )
{
    std::filesystem::create_directories( p.parent_path() );
    std::ofstream f( p, std::ios::binary );
    f.write( static_cast<const char *>( data ), static_cast<std::streamsize>( len ));
}

static void setup_tree()
{
    g_root = std::filesystem::temp_directory_path() / "xash3dpp_maploader_test";
    std::filesystem::remove_all( g_root );
    std::filesystem::create_directories( g_root / "valve" );
    std::filesystem::create_directories( g_root / "game" / "maps" );

    const auto bsp = make_minimal_world().build();
    write_file( g_root / "game" / "maps" / "test.bsp", bsp.data(), bsp.size() );

    // patched.bsp + a NEWER .ent override (bsp written first).
    write_file( g_root / "game" / "maps" / "patched.bsp", bsp.data(), bsp.size() );
    const std::string ent =
        "{\n\"classname\" \"worldspawn\"\n\"message\" \"Patched Map\"\n}\n";
    write_file( g_root / "game" / "maps" / "patched.ent", ent.data(), ent.size() );
}

// ---------------------------------------------------------------------------
// Filesystem overload
// ---------------------------------------------------------------------------

static void init_fs( xash::filesystem::Filesystem &fs )
{
    REQUIRE( fs.init( g_root.string(), "valve", "game" ) );
    // Search paths are added explicitly (tests/filesystem precedent); the
    // GameDir flag makes gamedironly lookups (the .ent probe) work.
    fs.add_game_directory(( g_root / "game" ).string(),
                          xash::filesystem::SearchPathFlags::GameDir );
}

static void test_load_through_filesystem()
{
    xash::filesystem::Filesystem fs;
    init_fs( fs );

    const auto w = ml::load_world_data( fs, "maps/test.bsp", ml::WorldLoadOptions{} );
    REQUIRE( w.has_value() );
    CHECK( w->message() == "Test Map" );
    CHECK_EQ( w->leafs().size(), std::size_t{ 3 } );

    // Missing file → MapNotFound.
    const auto missing = ml::load_world_data( fs, "maps/nope.bsp", ml::WorldLoadOptions{} );
    REQUIRE( !missing.has_value() );
    CHECK( missing.error() == xash::core::ErrorCode::MapNotFound );

    fs.shutdown();
}

static void test_ent_patch_override()
{
    xash::filesystem::Filesystem fs;
    init_fs( fs );

    const auto w = ml::load_world_data( fs, "maps/patched.bsp", ml::WorldLoadOptions{} );
    REQUIRE( w.has_value() );

    // The .ent patch replaced the entities lump wholesale.
    CHECK( w->message() == "Patched Map" );
    CHECK( w->entities().find( "Patched Map" ) != std::string_view::npos );
    CHECK( w->wadlist().empty() ); // patch has no "wad" key

    // Non-world loads never look for a patch.
    ml::WorldLoadOptions bm;
    bm.is_world = false;
    const auto raw = ml::load_world_data( fs, "maps/patched.bsp", bm );
    REQUIRE( raw.has_value() );
    CHECK( raw->entities().find( "Patched" ) == std::string_view::npos );

    fs.shutdown();
}

// ---------------------------------------------------------------------------
// MapLoader FSM end-to-end
// ---------------------------------------------------------------------------

namespace
{
struct Recorder final : xash::IMapLoaderObserver
{
    int begins = 0, ends = 0;
    bool last_success = false;
    void on_load_begin( std::string_view, MapLoadState ) noexcept override { ++begins; }
    void on_load_end( std::string_view, bool ok ) noexcept override
    {
        ++ends;
        last_success = ok;
    }
};
} // namespace

static void test_fsm_loads_world()
{
    xash::filesystem::Filesystem fs;
    init_fs( fs );

    MapLoader ml_;
    REQUIRE( ml_.init( MapLoaderInitParams{ .filesystem = &fs } ) );

    Recorder rec;
    ml_.attach_observer( &rec );

    // Bare map name → "maps/test.bsp".
    ml_.load_level( "test", /*background=*/false );
    CHECK( ml_.world() == nullptr );
    ml_.run_frame_step();

    CHECK( ml_.state() == MapLoadState::RunFrame );
    CHECK_EQ( rec.begins, 1 );
    CHECK_EQ( rec.ends, 1 );
    CHECK( rec.last_success );

    REQUIRE( ml_.world() != nullptr );
    CHECK( ml_.world()->message() == "Test Map" );

    // The activated world answers queries.
    CHECK_EQ( ml::point_leaf( *ml_.world(), { 200.0f, 0.0f, 0.0f } ), 1 );

    // A failing load notifies failure and clears the previous world.
    ml_.load_level( "does_not_exist", false );
    ml_.run_frame_step();
    CHECK_EQ( rec.ends, 2 );
    CHECK( !rec.last_success );
    CHECK( ml_.world() == nullptr );

    // Reload works; clear_world releases.
    ml_.load_level( "test", false );
    ml_.run_frame_step();
    REQUIRE( ml_.world() != nullptr );
    ml_.clear_world();
    CHECK( ml_.world() == nullptr );

    ml_.shutdown();
    fs.shutdown();
}

static void test_fsm_without_filesystem_fails()
{
    MapLoader ml_;
    REQUIRE( ml_.init( MapLoaderInitParams{} ) );
    CHECK( !ml_.load_world( "test", ml::WorldLoadOptions{} ));
    CHECK( ml_.world() == nullptr );
    ml_.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // MapLoader load-path entry points assert ThreadRole::Main; register it so
    // the bare asserts do not FATAL the harness (server test-main idiom).
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    setup_tree();

    RUN_TEST( test_load_through_filesystem );
    RUN_TEST( test_ent_patch_override );
    RUN_TEST( test_fsm_loads_world );
    RUN_TEST( test_fsm_without_filesystem_fails );

    std::filesystem::remove_all( g_root );

    std::printf( "map_loader_world: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
