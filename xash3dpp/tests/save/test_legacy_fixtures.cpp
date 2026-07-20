// xash3dpp — env-gated real-retail-save fixture parse (Chunk 8, slice S8.8).
//
// Exercises load_sav_file + save_comment + LevelStateLoader::load against
// REAL `.sav` files written by the legacy 32-bit engine (retail Half-Life or
// any mod).  This repo's other save tests (test_sav_container / test_hl1_
// writer / test_hl1_loader) are synthetic goldens hand-derived from the
// documented wire rules; this test is the independent cross-check against
// files this codebase never produced — real token orderings, real entity
// counts, whatever quirk branches an actual game session happened to
// exercise (save-boundary.md's S8.3 parity-audit note: "Writer goldens pin
// small token tables, not the 4095-slot production image — the production
// byte-parity witness is the S8.8 legacy-fixture tier").
//
// PARSE-ONLY: no game DLL is loaded here, so entity RESTORE (pfnRestore) is
// out of scope — this proves the container and `.HL1` level parse accept
// the file cleanly (header plausible, ETABLE consistent, no SaveError),
// nothing more.
//
// Gating (mirrors tests/server/lifecycle/test_hl_smoke.cpp's SKIP-GREEN
// pattern exactly): set env XASH3DPP_LEGACY_SAVE_DIR to a directory
// containing one or more real `*.sav` files written by the legacy engine.
// Unset, nonexistent, or holding no `.sav` files -> SKIP (exit 0) so the
// suite stays green on CI and on any machine without fixtures.  The env var
// is absent in this repo's own build/test runs, so this test is expected to
// SKIP there.

#include <xash3dpp/private/save/container_codec.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_loader.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/save_comment.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace fs_  = xash::filesystem;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

// ---------------------------------------------------------------------------
// Small local helpers.
// ---------------------------------------------------------------------------

static bool ends_with_ci( std::string_view s, std::string_view suffix ) noexcept
{
    if ( s.size() < suffix.size() )
        return false;
    const std::string_view tail = s.substr( s.size() - suffix.size() );
    for ( std::size_t i = 0; i < suffix.size(); ++i )
        if ( std::tolower( static_cast<unsigned char>( tail[i] ) ) !=
             std::tolower( static_cast<unsigned char>( suffix[i] ) ) )
            return false;
    return true;
}

// Raw file bytes — used both for the standalone version-tag spot check (a
// direct read of the on-disk int, since CommentInfo intentionally exposes
// only the derived Ok/OldVersion/... status, never the raw scalar) and to
// feed save_comment()'s pure (no-filesystem) overload.
static std::vector<std::byte> read_raw_file( const std::filesystem::path &p )
{
    std::ifstream      f( p, std::ios::binary );
    std::vector<char>  raw( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
    std::vector<std::byte> out( raw.size() );
    if ( !raw.empty() )
        std::memcpy( out.data(), raw.data(), raw.size() );
    return out;
}

static std::int32_t read_le_i32( std::span<const std::byte> bytes, std::size_t offset ) noexcept
{
    if ( bytes.size() < offset + 4 )
        return 0;
    const auto b0 = std::to_integer<std::uint32_t>( bytes[offset + 0] );
    const auto b1 = std::to_integer<std::uint32_t>( bytes[offset + 1] );
    const auto b2 = std::to_integer<std::uint32_t>( bytes[offset + 2] );
    const auto b3 = std::to_integer<std::uint32_t>( bytes[offset + 3] );
    return static_cast<std::int32_t>( b0 | ( b1 << 8 ) | ( b2 << 16 ) | ( b3 << 24 ) );
}

// ---------------------------------------------------------------------------
// One fixture: load_sav_file (container + embedded .HL1) -> save_comment ->
// LevelStateLoader::load.  `scratch_root` is a shared temp tree this
// function mounts a Filesystem over (load_sav_file requires a mounted
// Filesystem — mirrors test_sav_container.cpp's temp-directory precedent).
// ---------------------------------------------------------------------------
static void run_one_fixture( const std::filesystem::path &fixture, const std::filesystem::path &scratch_root )
{
    std::printf( "  [legacy-fixture] %s\n", fixture.filename().string().c_str() );

    const std::vector<std::byte> raw = read_raw_file( fixture );
    REQUIRE( raw.size() >= 8u ); // at least the id+version prefix

    // --- version-tag spot check (raw bytes @ offset 4) ---
    const std::int32_t version = read_le_i32( raw, 4 );
    CHECK_EQ( version, save::k_savegame_version ); // 0x0071

    // --- mount a scratch Filesystem and copy the fixture into <root>/game/save/ ---
    const std::string stem = fixture.stem().string();
    std::error_code    ec;
    std::filesystem::create_directories( scratch_root / "game" / "save", ec );
    std::filesystem::copy_file( fixture, scratch_root / "game" / "save" / ( stem + ".sav" ),
                                std::filesystem::copy_options::overwrite_existing, ec );
    REQUIRE( !ec );

    fs_::Filesystem fs;
    REQUIRE( fs.init( scratch_root.string(), "valve", "game" ) );
    fs.add_game_directory( ( scratch_root / "game" ).string(), fs_::SearchPathFlags::GameDir );

    auto buf = save::create_save_buffer( g_pool ); // defaults: SAVE_HEAPSIZE/SAVE_HASHSTRINGS
    REQUIRE( buf != nullptr );

    auto loaded = save::load_sav_file( fs, stem, /*global_state=*/nullptr, *buf );
    REQUIRE( loaded.has_value() );  // "no SaveError"
    REQUIRE( loaded->has_value() ); // the file exists (we just copied it)
    const save::SavContainerResult &container = **loaded;

    // --- container plausibility ---
    CHECK( container.header.map_name[0] != '\0' );
    CHECK( container.header.comment[0] != '\0' ); // "comment non-empty"
    CHECK( container.header.map_count >= 0 );
    CHECK( !container.records.empty() );

    // --- save_comment: full parse, must succeed with plausible values ---
    auto comment = save::save_comment( raw, fixture.filename().string(), std::nullopt );
    REQUIRE( comment.has_value() );
    CHECK( comment->status == save::SaveCommentStatus::Ok ); // implies the 0x0071 version gate passed
    CHECK( !comment->description.empty() );
    CHECK( !comment->map_name.empty() );

    // --- locate the embedded `.HL1` level record and parse it (parse-only —
    //     no game DLL, so entity RESTORE is out of scope here). ---
    const save::ExtractedRecord *hl1 = nullptr;
    for ( const auto &rec : container.records )
        if ( ends_with_ci( rec.name, ".hl1" ) )
        {
            hl1 = &rec;
            break;
        }
    REQUIRE( hl1 != nullptr );

    auto lbuf = save::create_save_buffer( g_pool );
    REQUIRE( lbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateLoader loader( *lbuf, ltable );
    save::LevelState       state;
    auto                   r = loader.load( hl1->data, state );
    REQUIRE( r.has_value() ); // "no SaveError"

    // --- header plausible + ETABLE consistent ---
    CHECK( state.header.entity_count > 0 );
    CHECK( state.header.map_name[0] != '\0' );
    CHECK_EQ( static_cast<std::int32_t>( ltable.count() ), state.header.entity_count );

    std::printf( "    map=%s entities=%d records=%zu comment=\"%s\"\n", state.header.map_name,
                state.header.entity_count, container.records.size(), comment->description.c_str() );

    fs.shutdown();
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    const char *dir_env = std::getenv( "XASH3DPP_LEGACY_SAVE_DIR" );
    if ( dir_env == nullptr || dir_env[0] == '\0' )
    {
        std::printf( "SKIP: set XASH3DPP_LEGACY_SAVE_DIR to a directory of real "
                     "legacy-engine-written `.sav` files to run the retail-save "
                     "fixture parse\n" );
        std::printf( "legacy_fixtures: skipped (no XASH3DPP_LEGACY_SAVE_DIR)\n" );
        return 0;
    }

    const std::filesystem::path dir( dir_env );
    std::error_code             ec;
    if ( !std::filesystem::is_directory( dir, ec ) )
    {
        std::printf( "SKIP: %s is not a directory\n", dir_env );
        std::printf( "legacy_fixtures: skipped (no such directory)\n" );
        return 0;
    }

    std::vector<std::filesystem::path> fixtures;
    for ( const auto &entry : std::filesystem::directory_iterator( dir, ec ) )
        if ( entry.is_regular_file() && ends_with_ci( entry.path().extension().string(), ".sav" ) )
            fixtures.push_back( entry.path() );

    if ( fixtures.empty() )
    {
        std::printf( "SKIP: %s contains no *.sav files\n", dir_env );
        std::printf( "legacy_fixtures: skipped (empty dir)\n" );
        return 0;
    }

    std::printf( "  fixture dir: %s (%zu file(s))\n", dir_env, fixtures.size() );

    g_pool = mem::create_pool( "legacy_fixtures_test" );

    const std::filesystem::path scratch_root =
        std::filesystem::temp_directory_path() / "xash3dpp_legacy_fixtures_test";
    std::filesystem::remove_all( scratch_root, ec );

    for ( const auto &f : fixtures )
        run_one_fixture( f, scratch_root );

    std::filesystem::remove_all( scratch_root, ec );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "legacy_fixtures: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
