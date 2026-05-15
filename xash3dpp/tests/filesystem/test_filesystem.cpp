// xash3dpp — filesystem integration tests
// Covers: SearchPathFlags (|, &, ~, |=, &=, any), SeekOrigin,
//         Filesystem::init, shutdown, get_root_directory, gamedir, get_game_info,
//         Filesystem::add_game_directory, file_exists, open, load_file,
//         Filesystem::write_file, file_size, remove, rename,
//         Filesystem::search, clear_paths, allow_direct_paths,
//         Filesystem::load_direct_file, crc32_file, md5_file,
//         Filesystem::find_library, mount_archive, activate_game,
//         File::Read, Seek, Tell, Eof, Length, Gets, Getc, UnGetc

#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/utilities/hash.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <thread>
#include <vector>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"

// ===========================================================================
// Temp-directory fixture
// ===========================================================================

static std::filesystem::path g_testdir;

// create a minimal directory tree used by the integration tests:
//   <tmp>/xash3dpp_fs_test/
//       direct.txt           — used by test_load_direct_file
//       game/
//           hello.txt        — plain text file, "hello\n"
//           data.bin         — binary payload, "binary data"
static void setup_testdir()
{
    g_testdir = std::filesystem::temp_directory_path() / "xash3dpp_fs_test";
    std::filesystem::remove_all( g_testdir );
    std::filesystem::create_directories( g_testdir / "game" );

    // All files written in binary mode to avoid platform line-ending translation.
    {
        std::ofstream f( g_testdir / "game" / "hello.txt", std::ios::binary );
        f << "hello\n";
    }
    {
        std::ofstream f( g_testdir / "game" / "data.bin", std::ios::binary );
        f << "binary data";
    }
    {
        std::ofstream f( g_testdir / "direct.txt", std::ios::binary );
        f << "direct content";
    }
}

static void teardown_testdir()
{
    std::filesystem::remove_all( g_testdir );
}

// Convenience helpers that convert std::filesystem::path → std::string for xash API.
static std::string rootdir() { return g_testdir.string(); }
static std::string gamedir() { return ( g_testdir / "game" ).string(); }

// ===========================================================================
// 1. SearchPathFlags operators  (pure, no I/O)
// ===========================================================================

static void test_search_path_flags()
{
    using F = xash::filesystem::SearchPathFlags;

    // Bitwise OR combines flags.
    CHECK(  xash::filesystem::any( F::Static | F::NoWrite ) );

    // AND isolates a flag present in the combined value.
    CHECK(  xash::filesystem::any( ( F::Static | F::NoWrite ) & F::Static ) );

    // AND of a flag that is NOT present → zero.
    CHECK( !xash::filesystem::any( ( F::Static | F::NoWrite ) & F::GameDir ) );

    // NOT inverts; ANDing back cancels.
    CHECK( !xash::filesystem::any( F::Static & ~F::Static ) );

    // Compound-assign |=
    F f = F::None;
    f |= F::Static;
    CHECK(  xash::filesystem::any( f & F::Static ) );

    // Compound-assign &= (clear the flag).
    f &= ~F::Static;
    CHECK( !xash::filesystem::any( f & F::Static ) );

    // any() on None is false.
    CHECK( !xash::filesystem::any( F::None ) );
}

// ===========================================================================
// 2. SeekOrigin enum values  (pure)
// ===========================================================================

static void test_seek_origin_values()
{
    // Must match SEEK_SET / SEEK_CUR / SEEK_END so platform code can cast safely.
    CHECK( static_cast<int>( xash::filesystem::SeekOrigin::Begin   ) == 0 );
    CHECK( static_cast<int>( xash::filesystem::SeekOrigin::Current ) == 1 );
    CHECK( static_cast<int>( xash::filesystem::SeekOrigin::End     ) == 2 );
}

// ===========================================================================
// 3. init / GetRootDirectory / gamedir / get_game_info
// ===========================================================================

static void test_init_gamedir()
{
    xash::filesystem::Filesystem fs;
    CHECK( fs.init( rootdir(), "valve", "game" ) );
    CHECK( fs.get_root_directory() == rootdir() );
    // gamedir is stored verbatim from init().
    CHECK( fs.gamedir() == "game" );
    // get_game_info before activate_game → default-constructed (empty gamefolder).
    CHECK( fs.get_game_info().gamefolder.empty() );
    fs.shutdown();
}

// ===========================================================================
// 4. activate_game when no gameinfo.txt is present
// ===========================================================================

static void test_activate_game_not_found()
{
    // parse_gameinfo_txt / parse_liblist_gam are stubs → scan_game_directories
    // always returns an empty list → activate_game always returns false.
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    CHECK( !fs.activate_game( "game", xash::filesystem::SearchPathFlags::None ) );
    CHECK( !fs.get_game_info().gamefolder.empty() == false );  // still empty
    fs.shutdown();
}

// ===========================================================================
// 5. allow_direct_paths — smoke: toggle does not crash
// ===========================================================================

static void test_allow_direct_paths()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.allow_direct_paths( true );
    fs.allow_direct_paths( false );
    CHECK( true );  // reached here without crash or assert
    fs.shutdown();
}

// ===========================================================================
// 6. find_library without an active game
// ===========================================================================

static void test_find_library_no_game()
{
    // game_loaded == false → returns nullopt immediately.
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    CHECK( !fs.find_library( "hl.dll" ).has_value() );
    fs.shutdown();
}

// ===========================================================================
// 7. mount_archive with unknown extension
// ===========================================================================

static void test_mount_archive_bad_ext()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    // ".xyz" is not in k_archive_types.
    CHECK( !fs.mount_archive( "any/path/file.xyz",
                             xash::filesystem::SearchPathFlags::None ) );
    // ".pak" extension matches but the file doesn't exist → factory fails.
    CHECK( !fs.mount_archive( ( g_testdir / "nosuch.pak" ).string(),
                             xash::filesystem::SearchPathFlags::None ) );
    fs.shutdown();
}

// ===========================================================================
// 8. add_game_directory / file_exists / open / load_file
// ===========================================================================

static void test_add_game_directory()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    CHECK(  fs.file_exists( "hello.txt" ) );
    CHECK( !fs.file_exists( "nonexistent.xyz" ) );

    // open and read content (binary: "hello\n" = 6 bytes).
    auto f = fs.open( "hello.txt", "rb" );
    CHECK( f != nullptr );
    if ( f ) {
        std::byte buf[16]{};
        const xash::filesystem::FsOffset n = f->Read( std::span{ buf } );
        CHECK( n == 6 );
        CHECK( std::memcmp( buf, "hello\n", 6 ) == 0 );
    }

    // load_file returns the exact file bytes; size() == on-disk byte count.
    const auto data = fs.load_file( "hello.txt" );
    CHECK( !data.empty() );
    CHECK( std::memcmp( data.data(), "hello\n", 6 ) == 0 );

    // gamedironly=true with no GameDir-flagged path → file not found.
    CHECK( !fs.file_exists( "hello.txt", /*gamedironly=*/true ) );

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 9. write_file /file_existss file_sizeze
// ===========================================================================

static void test_write_file()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string content = "written content";
    const auto span = std::as_bytes( std::span{ content.data(), content.size() } );

    CHECK( fs.write_file( "written.txt", span ) );
    CHECK( fs.file_exists( "written.txt" ) );

    const auto sz = fs.file_size( "written.txt" );
    CHECK( sz.has_value() );
    if ( sz )
        CHECK( *sz == static_cast<xash::filesystem::FsOffset>( content.size() ) );

    // Clean up the test artefact.
    fs.remove( "written.txt" );
    fs.shutdown();
}

// ===========================================================================
// 10. remove
// ===========================================================================

static void test_delete()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string_view payload = "to be deleted";
    fs.write_file( "tmp_delete.txt",
                  std::as_bytes( std::span{ payload.data(), payload.size() } ) );

    CHECK(  fs.file_exists( "tmp_delete.txt" ) );
    CHECK(  fs.remove( "tmp_delete.txt" ) );
    CHECK( !fs.file_exists( "tmp_delete.txt" ) );

    // Deleting a nonexistent file returns false.
    CHECK( !fs.remove( "nonexistent.xyz" ) );

    fs.shutdown();
}

// ===========================================================================
// 11. rename
// ===========================================================================

static void test_rename()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string_view payload = "rename me";
    fs.write_file( "before.txt",
                  std::as_bytes( std::span{ payload.data(), payload.size() } ) );

    CHECK(  fs.file_exists( "before.txt" ) );
    CHECK(  fs.rename( "before.txt", "after.txt" ) );
    CHECK( !fs.file_exists( "before.txt" ) );
    CHECK(  fs.file_exists( "after.txt" ) );

    // Clean up.
    fs.remove( "after.txt" );
    fs.shutdown();
}

// ===========================================================================
// 12. search
// ===========================================================================

static void test_search()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto result = fs.search( "*.txt", /*case_insensitive=*/true );
    bool found_hello = false;
    for ( const auto& entry : result.files )
        if ( entry.find( "hello.txt" ) != std::string::npos )
            { found_hello = true; break; }
    CHECK( found_hello );

    // Pattern that matches nothing.
    const auto empty = fs.search( "*.zzz", true );
    CHECK( empty.files.empty() );

    // Results are de-duplicated (same file appears only once).
    // Add the same directory a second time.
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );
    const auto dedup = fs.search( "*.txt", true );
    int count_hello = 0;
    for ( const auto& entry : dedup.files )
        if ( entry.find( "hello.txt" ) != std::string::npos )
            ++count_hello;
    CHECK( count_hello == 1 );

    fs.shutdown();
}

// ===========================================================================
// 13. clear_paths — non-static removed, Static survives
// ===========================================================================

static void test_clear_paths()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );

    // Non-static path — visible before clear_paths, gone after.
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );
    CHECK(  fs.file_exists( "hello.txt" ) );
    fs.clear_paths();
    CHECK( !fs.file_exists( "hello.txt" ) );

    // Static path — survives clear_paths.
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::Static );
    CHECK(  fs.file_exists( "hello.txt" ) );
    fs.clear_paths();
    CHECK(  fs.file_exists( "hello.txt" ) );  // still there

    fs.shutdown();
}

// ===========================================================================
// 14. load_direct_file — bypass VFS
// ===========================================================================

static void test_load_direct_file()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );

    const std::string path = ( g_testdir / "direct.txt" ).string();
    const auto data = fs.load_direct_file( path );
    CHECK( !data.empty() );
    CHECK( std::memcmp( data.data(), "direct content", 14 ) == 0 );

    // Non-existent path → empty vector.
    const auto none = fs.load_direct_file( path + ".nonexistent" );
    CHECK( none.empty() );

    fs.shutdown();
}

// ===========================================================================
// 15. crc32_file — self-consistent with xash::utilities::crc32
// ===========================================================================

static void test_crc32_file()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto crc = fs.crc32_file( "hello.txt" );
    CHECK( crc.has_value() );
    if ( crc ) {
        // Must match the one-shot utility function on the same bytes.
        const char content[] = "hello\n";
        const auto expected  = xash::utilities::crc32( content, 6 );
        CHECK( *crc == expected );
    }

    // Non-existent file → nullopt.
    CHECK( !fs.crc32_file( "nonexistent.xyz" ).has_value() );

    fs.shutdown();
}

// ===========================================================================
// 16. md5_file — idempotent on identical content
// ===========================================================================

static void test_md5_file()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto d1 = fs.md5_file( "hello.txt" );
    CHECK( d1.has_value() );
    // Second call on same content must produce the same digest.
    const auto d2 = fs.md5_file( "hello.txt" );
    CHECK( d2.has_value() );
    if ( d1 && d2 )
        CHECK( *d1 == *d2 );

    // Different content → different digest (with overwhelming probability).
    const auto d3 = fs.md5_file( "data.bin" );
    CHECK( d3.has_value() );
    if ( d1 && d3 )
        CHECK( *d1 != *d3 );

    // Non-existent file → nullopt.
    CHECK( !fs.md5_file( "nonexistent.xyz" ).has_value() );

    fs.shutdown();
}

// ===========================================================================
// 17. shutdown resets search paths
// ===========================================================================

static void test_shutdown_resets_state()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );
    CHECK( fs.file_exists( "hello.txt" ) );

    fs.shutdown();
    // All paths (including non-static ones) are cleared.
    CHECK( !fs.file_exists( "hello.txt" ) );
}

// ===========================================================================
// 18. File::Seek / Tell / Eof / Length
// ===========================================================================

static void test_file_seek_tell_eof()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    auto f = fs.open( "hello.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    // Length is the uncompressed file size.
    CHECK( f->Length() == 6 );  // "hello\n"

    // At open: cursor is at 0, not EOF.
    CHECK( f->Tell() == 0 );
    CHECK( !f->Eof() );

    // Seek to end: Tell() == Length(), Eof().
    f->Seek( 0, xash::filesystem::SeekOrigin::End );
    CHECK( f->Tell() == f->Length() );
    CHECK( f->Eof() );

    // Seek back to beginning.
    f->Seek( 0, xash::filesystem::SeekOrigin::Begin );
    CHECK( f->Tell() == 0 );
    CHECK( !f->Eof() );

    // Seek forward by 2 bytes from current position.
    f->Seek( 2, xash::filesystem::SeekOrigin::Current );
    CHECK( f->Tell() == 2 );

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 19. File::Gets / Getc / UnGetc
// ===========================================================================

static void test_file_gets_getc()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // Gets reads one text line, strips the terminating '\n'.
    {
        auto f = fs.open( "hello.txt", "rb" );
        CHECK( f != nullptr );
        if ( f ) {
            const auto line = f->Gets();
            CHECK( line.has_value() );
            if ( line )
                CHECK( *line == "hello" );  // '\n' consumed and stripped
        }
    }

    // Getc + UnGetc round-trip.
    {
        auto f = fs.open( "data.bin", "rb" );
        CHECK( f != nullptr );
        if ( f ) {
            const int c = f->Getc();
            CHECK( c == static_cast<int>( 'b' ) );  // first byte of "binary data"
            f->UnGetc( c );
            CHECK( f->Getc() == c );  // UnGetc pushes back one character
        }
    }

    fs.shutdown();
}

// ===========================================================================
// main
// ===========================================================================
// 19. file_time — existing file has a valid time point
// ===========================================================================

static void test_file_time()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // Existing file → has a time point.
    const auto t = fs.file_time( "hello.txt" );
    CHECK( t.has_value() );

    // Non-existent file → nullopt.
    CHECK( !fs.file_time( "nonexistent.xyz" ).has_value() );

    fs.shutdown();
}

// ===========================================================================
// 20. disk_path — plain-directory file returns on-disk path; missing → nullopt
// ===========================================================================

static void test_disk_path()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // File that lives in a plain directory → path ending with the filename.
    const auto p = fs.disk_path( "hello.txt" );
    CHECK( p.has_value() );
    if ( p )
        CHECK( p->find( "hello.txt" ) != std::string::npos );

    // Non-existent file → nullopt.
    CHECK( !fs.disk_path( "nonexistent.xyz" ).has_value() );

    fs.shutdown();
}

// ===========================================================================
// 21. add_game_hierarchy — behaves like add_game_directory when no _hd/_lv dirs exist
// ===========================================================================

static void test_add_game_hierarchy()
{
    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );

    // No MountHD / MountLV flags → only the plain gamedir is mounted.
    // The _hd / _lv variants do not exist in the fixture, so any attempt to
    // scan them is silently skipped (collect_paths_for_dir on a missing dir
    // returns nothing).
    fs.add_game_hierarchy( gamedir(), xash::filesystem::SearchPathFlags::None );

    CHECK(  fs.file_exists( "hello.txt" ) );
    CHECK( !fs.file_exists( "nonexistent.xyz" ) );

    fs.shutdown();
}

// ===========================================================================
// 22. Concurrent reads — file_exists / open / load_file from multiple threads
// ===========================================================================

static void test_concurrent_reads()
{
    // All read operations take a shared_lock on paths_mutex, so N threads may
    // run them simultaneously without blocking each other.  This test checks
    // that no crash or data race occurs and that every thread gets the correct
    // result.
    constexpr int N = 8;

    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    std::atomic<int> found_count { 0 };
    std::atomic<int> load_ok     { 0 };

    std::vector<std::thread> threads;
    threads.reserve( N );
    for ( int i = 0; i < N; ++i )
    {
        threads.emplace_back( [&fs, &found_count, &load_ok] {
            if ( fs.file_exists( "hello.txt" ) )
                found_count.fetch_add( 1, std::memory_order_relaxed );

            auto f = fs.open( "hello.txt", "rb" );
            if ( f )
            {
                std::byte buf[6]{};
                if ( f->Read( std::span{ buf } ) == 6 &&
                     std::memcmp( buf, "hello\n", 6 ) == 0 )
                    load_ok.fetch_add( 1, std::memory_order_relaxed );
            }
        } );
    }
    for ( auto& t : threads ) t.join();

    CHECK( found_count.load() == N );
    CHECK( load_ok.load()     == N );

    fs.shutdown();
}

// ===========================================================================
// 23. Reads concurrent with add_game_directory (shared vs. exclusive lock)
// ===========================================================================

static void test_concurrent_read_write_paths()
{
    // A writer thread repeatedly adds and clears search paths (exclusive lock)
    // while reader threads call file_exists and load_file (shared lock).
    // The test verifies no crash occurs; correctness of individual results is
    // non-deterministic because of the intentional interleaving.
    constexpr int N_READERS = 4;
    constexpr int N_CYCLES  = 32;

    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    // Pre-load the directory so readers can find the file at least sometimes.
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::Static );

    std::atomic<bool> done { false };

    // Writer: add + clear (non-static) paths in a tight loop.
    std::thread writer( [&fs, &done] {
        for ( int i = 0; i < N_CYCLES; ++i )
        {
            fs.add_game_directory( gamedir(),
                                 xash::filesystem::SearchPathFlags::None );
            fs.clear_paths();
        }
        done.store( true, std::memory_order_relaxed );
    } );

    // Readers: query the filesystem continuously until the writer is done.
    std::vector<std::thread> readers;
    readers.reserve( N_READERS );
    for ( int i = 0; i < N_READERS; ++i )
    {
        readers.emplace_back( [&fs, &done] {
            while ( !done.load( std::memory_order_relaxed ) )
            {
                (void)fs.file_exists( "hello.txt" );
                (void)fs.load_file( "hello.txt" );
            }
        } );
    }

    writer.join();
    for ( auto& r : readers ) r.join();

    CHECK( true );  // reaching here without crash or deadlock is the assertion

    fs.shutdown();
}

// ===========================================================================
// 24. Concurrent open + Read — each thread owns its File allocation
// ===========================================================================

static void test_concurrent_open_read()
{
    // Each thread opens the same file and reads it independently.  Every File
    // is pool-allocated (mem_alloc) and freed (mem_free / File::operator delete)
    // from concurrent threads.  This exercises the pool's atomic counters under
    // parallel alloc+free load with objects that have non-trivial dtors.
    constexpr int         N_THREADS = 6;
    constexpr int         N_ITER    = 16;
    constexpr std::size_t FILE_SIZE = 6;  // "hello\n"

    xash::filesystem::Filesystem fs;
    fs.init( rootdir(), "valve", "game" );
    fs.add_game_directory( gamedir(), xash::filesystem::SearchPathFlags::None );

    std::atomic<int> correct { 0 };

    std::vector<std::thread> threads;
    threads.reserve( N_THREADS );
    for ( int i = 0; i < N_THREADS; ++i )
    {
        threads.emplace_back( [&fs, &correct] {
            for ( int iter = 0; iter < N_ITER; ++iter )
            {
                auto f = fs.open( "hello.txt", "rb" );
                if ( !f ) continue;

                std::byte buf[FILE_SIZE]{};
                const auto n = f->Read( std::span{ buf } );
                if ( n == static_cast<xash::filesystem::FsOffset>( FILE_SIZE ) &&
                     std::memcmp( buf, "hello\n", FILE_SIZE ) == 0 )
                    correct.fetch_add( 1, std::memory_order_relaxed );
                // unique_ptr dtor calls File::operator delete → mem_free
            }
        } );
    }
    for ( auto& t : threads ) t.join();

    CHECK( correct.load() == N_THREADS * N_ITER );

    fs.shutdown();
}

// ===========================================================================

int main()
{
    setup_testdir();

    // Pure tests (no filesystem I/O).
    test_search_path_flags();
    test_seek_origin_values();

    // Filesystem state tests (no file read/write).
    test_init_gamedir();
    test_activate_game_not_found();
    test_allow_direct_paths();
    test_find_library_no_game();
    test_mount_archive_bad_ext();

    // Integration tests (read from and write to the temp directory).
    test_add_game_directory();
    test_file_time();
    test_disk_path();
    test_add_game_hierarchy();
    test_write_file();
    test_delete();
    test_rename();
    test_search();
    test_clear_paths();
    test_load_direct_file();
    test_crc32_file();
    test_md5_file();
    test_shutdown_resets_state();
    test_file_seek_tell_eof();
    test_file_gets_getc();

    // Threading tests.
    test_concurrent_reads();
    test_concurrent_read_write_paths();
    test_concurrent_open_read();

    teardown_testdir();

    std::printf( "filesystem: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
