// xash3dpp — CIDirectory (case-insensitive directory resolver) tests
// Covers: CIDirectory::glob, CIDirectory::resolve, CIDirectory::invalidate
//
// Legacy reference: filesystem/dir.c  (FS_FixFileCase, FS_BuildTrie,
//                   listdirectory, FS_FindDirEntry)
//
// CIDirectory operates in two modes:
//   Native   — the underlying volume handles case natively (Windows, macOS,
//               and Linux CASEFOLD_FL dirs).  resolve returns the name as-is
//               without an existence check.
//   Emulated — case-sensitive volume (typical Linux ext4).  resolve populates
//               a per-subdirectory sorted cache and does a CI binary search.
//
// glob always uses the internal cache regardless of mode, so pattern-matching
// and cache-invalidation tests are portable.  Mode-specific resolve behaviours
// are guarded with #ifdef below.

#include <xash3dpp/private/filesystem/ci_directory.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"

// ===========================================================================
// Fixture
// ===========================================================================

static std::filesystem::path g_testdir;

// Root layout used by all tests:
//   <tmp>/xash3dpp_ci_test/
//       hello.txt          — lowercase name
//       World.txt          — mixed-case name
//       data.bin           — non-txt extension
//       sub/config.cfg     — file in a subdirectory
static void setup_testdir()
{
    g_testdir = std::filesystem::temp_directory_path() / "xash3dpp_ci_test";
    std::filesystem::remove_all( g_testdir );
    std::filesystem::create_directories( g_testdir / "sub" );

    auto touch = []( const std::filesystem::path& p ) {
        std::ofstream{ p, std::ios::binary } << "x";
    };

    touch( g_testdir / "hello.txt"    );
    touch( g_testdir / "World.txt"    );
    touch( g_testdir / "data.bin"     );
    touch( g_testdir / "sub" / "config.cfg" );
}

static void teardown_testdir()
{
    std::filesystem::remove_all( g_testdir );
}

// Factory: fresh CIDirectory rooted at g_testdir each time.
static xash::filesystem::CIDirectory make_ci_dir()
{
    return xash::filesystem::CIDirectory{ g_testdir.string() };
}

// ===========================================================================
// 1. glob — basic extension matching
// ===========================================================================

static void test_ci_directory_glob_basic()
{
    // legacy: listdirectory + FS_FindDirEntry — glob mirrors the result
    // of listing a directory and filtering by extension.
    auto d = make_ci_dir();

    // "*.txt" matches hello.txt and World.txt (2 results).
    const auto txts = d.glob( "", "*.txt", /*case_insensitive=*/true );
    CHECK( txts.size() == 2 );

    // "*.bin" matches only data.bin.
    const auto bins = d.glob( "", "*.bin", /*case_insensitive=*/true );
    CHECK( bins.size() == 1 );

    // "*.xyz" matches nothing.
    const auto none = d.glob( "", "*.xyz", /*case_insensitive=*/true );
    CHECK( none.empty() );
}

// ===========================================================================
// 2. glob — case-insensitive pattern matching
// ===========================================================================

static void test_ci_directory_glob_case_insensitive()
{
    // legacy: FS_FixFileCase uses Q_stricmp when probing entries.
    // With case_insensitive=true the pattern is matched without regard to case.
    auto d = make_ci_dir();

    // "*.TXT" case-insensitively matches both "hello.txt" and "World.txt".
    const auto txts = d.glob( "", "*.TXT", /*case_insensitive=*/true );
    CHECK( txts.size() == 2 );

    // "HELLO.*" case-insensitively matches "hello.txt".
    const auto hello = d.glob( "", "HELLO.*", /*case_insensitive=*/true );
    CHECK( hello.size() == 1 );
    if ( !hello.empty() )
        CHECK( hello[0] == "hello.txt" );

    // "WORLD.*" case-insensitively matches "World.txt".
    const auto world = d.glob( "", "WORLD.*", /*case_insensitive=*/true );
    CHECK( world.size() == 1 );
    if ( !world.empty() )
        CHECK( world[0] == "World.txt" );
}

// ===========================================================================
// 3. glob — case-sensitive pattern matching
// ===========================================================================

static void test_ci_directory_glob_case_sensitive()
{
    // With case_insensitive=false the pattern must match the on-disk case.
    auto d = make_ci_dir();

    // "*.TXT" does NOT match "hello.txt" or "World.txt" (extension is ".txt").
    const auto no_match = d.glob( "", "*.TXT", /*case_insensitive=*/false );
    CHECK( no_match.empty() );

    // "*.txt" DOES match both .txt files.
    const auto match = d.glob( "", "*.txt", /*case_insensitive=*/false );
    CHECK( match.size() == 2 );

    // Exact name + exact case matches.
    const auto exact = d.glob( "", "World.txt", /*case_insensitive=*/false );
    CHECK( exact.size() == 1 );

    // Exact name + wrong case does NOT match.
    const auto wrong = d.glob( "", "world.txt", /*case_insensitive=*/false );
    CHECK( wrong.empty() );
}

// ===========================================================================
// 4. glob — "*" matches all entries in root
// ===========================================================================

static void test_ci_directory_glob_star_all()
{
    // The root holds hello.txt, World.txt, data.bin, sub/ — 4 entries.
    auto d = make_ci_dir();
    const auto all = d.glob( "", "*", /*case_insensitive=*/true );
    CHECK( all.size() == 4 );
}

// ===========================================================================
// 5. glob — subdirectory
// ===========================================================================

static void test_ci_directory_glob_subdir()
{
    // glob in a named subdirectory lists only that directory's entries.
    auto d = make_ci_dir();

    const auto cfgs = d.glob( "sub", "*.cfg", /*case_insensitive=*/true );
    CHECK( cfgs.size() == 1 );
    if ( !cfgs.empty() )
        CHECK( cfgs[0] == "config.cfg" );

    // "*.txt" in "sub" → nothing.
    const auto empty = d.glob( "sub", "*.txt", /*case_insensitive=*/true );
    CHECK( empty.empty() );
}

// ===========================================================================
// 6. glob — nonexistent subdirectory → empty
// ===========================================================================

static void test_ci_directory_glob_missing_subdir()
{
    // platform::list_directory on a nonexistent path returns empty →
    // glob forwards that empty vector.
    auto d = make_ci_dir();
    const auto r = d.glob( "no_such_subdir", "*", /*case_insensitive=*/true );
    CHECK( r.empty() );
}

// ===========================================================================
// 7. resolve — platform-conditional behaviour
// ===========================================================================

static void test_ci_directory_resolve()
{
    auto d = make_ci_dir();

#ifdef _WIN32
    // Native mode: resolve is a pass-through — no disk lookup, always returns
    // the supplied name regardless of existence or case.
    // legacy: DIRENTRY_CASEINSENSITIVE early-exit in FS_FixFileCase.
    const auto r = d.resolve( "", "hello.txt" );
    CHECK( r.has_value() );
    if ( r ) CHECK( *r == "hello.txt" );

    // Uppercase query — returned as-is (no CI fold).
    const auto u = d.resolve( "", "HELLO.TXT" );
    CHECK( u.has_value() );
    if ( u ) CHECK( *u == "HELLO.TXT" );

    // Nonexistent name — still returned as-is in Native mode.
    const auto g = d.resolve( "", "ghost.xyz" );
    CHECK( g.has_value() );
#else
    // Emulated mode: CI binary-search against the cached directory listing.
    // legacy: FS_FindDirEntry with Q_stricmp.

    // Exact case — finds canonical name.
    const auto exact = d.resolve( "", "hello.txt" );
    CHECK( exact.has_value() );
    if ( exact ) CHECK( *exact == "hello.txt" );

    // Wrong case — CI fold returns canonical on-disk name.
    const auto fold = d.resolve( "", "HELLO.TXT" );
    CHECK( fold.has_value() );
    if ( fold ) CHECK( *fold == "hello.txt" );  // canonical, not "HELLO.TXT"

    // Mixed-case on-disk name resolved via CI.
    const auto world = d.resolve( "", "world.TXT" );
    CHECK( world.has_value() );
    if ( world ) CHECK( *world == "World.txt" );  // exact on-disk case

    // Nonexistent — nullopt.
    CHECK( !d.resolve( "", "ghost.xyz" ).has_value() );

    // Subdirectory resolution.
    const auto cfg = d.resolve( "sub", "CONFIG.CFG" );
    CHECK( cfg.has_value() );
    if ( cfg ) CHECK( *cfg == "config.cfg" );
#endif
}

// ===========================================================================
// 8. invalidate — stale cache is cleared; next glob re-scans
// ===========================================================================

static void test_ci_directory_invalidate()
{
    // legacy: FS_MaybeUpdateDirEntries — the legacy code rescans when the
    // cached count differs.  CIDirectory::invalidate is the explicit trigger.
    auto d = make_ci_dir();

    // Warm up the cache for the root subdirectory.
    const std::size_t initial_count = d.glob( "", "*", /*case_insensitive=*/true ).size();
    CHECK( initial_count == 4 );  // hello.txt, World.txt, data.bin, sub

    // create a new file AFTER the cache was populated.
    const auto new_file = g_testdir / "new_entry.tmp";
    { std::ofstream{ new_file, std::ios::binary } << "y"; }

    // Stale cache — the new file is NOT visible yet.
    const auto stale = d.glob( "", "*", /*case_insensitive=*/true );
    CHECK( stale.size() == initial_count );

    // invalidate the root directory cache.
    d.invalidate( "" );

    // Fresh scan — new file IS visible now.
    const auto fresh = d.glob( "", "*", /*case_insensitive=*/true );
    CHECK( fresh.size() == initial_count + 1 );

    std::filesystem::remove( new_file );
}

// ===========================================================================
// 9. invalidate — only the specified subdir is cleared
// ===========================================================================

static void test_ci_directory_invalidate_selective()
{
    auto d = make_ci_dir();

    // Warm caches for both root and "sub".
    d.glob( "",    "*",     /*case_insensitive=*/true );
    d.glob( "sub", "*.cfg", /*case_insensitive=*/true );

    // invalidate only "sub".
    d.invalidate( "sub" );

    // Adding a file to sub and re-globbing should see it after the invalidate.
    const auto extra = g_testdir / "sub" / "extra.cfg";
    { std::ofstream{ extra, std::ios::binary } << "z"; }

    const auto refreshed = d.glob( "sub", "*.cfg", /*case_insensitive=*/true );
    CHECK( refreshed.size() == 2 );  // config.cfg + extra.cfg

    // Root cache was NOT invalidated — still sees 4 original entries.
    const auto root = d.glob( "", "*", /*case_insensitive=*/true );
    CHECK( root.size() == 4 );

    std::filesystem::remove( extra );
}

// ===========================================================================
// main
// ===========================================================================

int main()
{
    setup_testdir();

    RUN_TEST( test_ci_directory_glob_basic );
    RUN_TEST( test_ci_directory_glob_case_insensitive );
    RUN_TEST( test_ci_directory_glob_case_sensitive );
    RUN_TEST( test_ci_directory_glob_star_all );
    RUN_TEST( test_ci_directory_glob_subdir );
    RUN_TEST( test_ci_directory_glob_missing_subdir );

    RUN_TEST( test_ci_directory_resolve );

    RUN_TEST( test_ci_directory_invalidate );
    RUN_TEST( test_ci_directory_invalidate_selective );

    teardown_testdir();

    std::printf( "ci_directory: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
