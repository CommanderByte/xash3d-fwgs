// xash3dpp — filesystem integration tests
// Covers: SearchPathFlags (|, &, ~, |=, &=, any), SeekOrigin,
//         Filesystem::Init, Shutdown, GetRootDirectory, Gamedir, GetGameInfo,
//         Filesystem::AddGameDirectory, FileExists, Open, LoadFile,
//         Filesystem::WriteFile, FileSize, Delete, Rename,
//         Filesystem::Search, ClearPaths, AllowDirectPaths,
//         Filesystem::LoadDirectFile, CRC32File, MD5File,
//         Filesystem::FindLibrary, MountArchive, ActivateGame,
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

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

// ===========================================================================
// Temp-directory fixture
// ===========================================================================

static std::filesystem::path g_testdir;

// Create a minimal directory tree used by the integration tests:
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
// 3. Init / GetRootDirectory / Gamedir / GetGameInfo
// ===========================================================================

static void test_init_gamedir()
{
    xash::filesystem::Filesystem fs;
    CHECK( fs.Init( rootdir(), "valve", "game" ) );
    CHECK( fs.GetRootDirectory() == rootdir() );
    // gamedir is stored verbatim from Init().
    CHECK( fs.Gamedir() == "game" );
    // GetGameInfo before ActivateGame → default-constructed (empty gamefolder).
    CHECK( fs.GetGameInfo().gamefolder.empty() );
    fs.Shutdown();
}

// ===========================================================================
// 4. ActivateGame when no gameinfo.txt is present
// ===========================================================================

static void test_activate_game_not_found()
{
    // parse_gameinfo_txt / parse_liblist_gam are stubs → ScanGameDirectories
    // always returns an empty list → ActivateGame always returns false.
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    CHECK( !fs.ActivateGame( "game", xash::filesystem::SearchPathFlags::None ) );
    CHECK( !fs.GetGameInfo().gamefolder.empty() == false );  // still empty
    fs.Shutdown();
}

// ===========================================================================
// 5. AllowDirectPaths — smoke: toggle does not crash
// ===========================================================================

static void test_allow_direct_paths()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AllowDirectPaths( true );
    fs.AllowDirectPaths( false );
    CHECK( true );  // reached here without crash or assert
    fs.Shutdown();
}

// ===========================================================================
// 6. FindLibrary without an active game
// ===========================================================================

static void test_find_library_no_game()
{
    // game_loaded == false → returns nullopt immediately.
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    CHECK( !fs.FindLibrary( "hl.dll" ).has_value() );
    fs.Shutdown();
}

// ===========================================================================
// 7. MountArchive with unknown extension
// ===========================================================================

static void test_mount_archive_bad_ext()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    // ".xyz" is not in k_archive_types.
    CHECK( !fs.MountArchive( "any/path/file.xyz",
                             xash::filesystem::SearchPathFlags::None ) );
    // ".pak" extension matches but the file doesn't exist → factory fails.
    CHECK( !fs.MountArchive( ( g_testdir / "nosuch.pak" ).string(),
                             xash::filesystem::SearchPathFlags::None ) );
    fs.Shutdown();
}

// ===========================================================================
// 8. AddGameDirectory / FileExists / Open / LoadFile
// ===========================================================================

static void test_add_game_directory()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    CHECK(  fs.FileExists( "hello.txt" ) );
    CHECK( !fs.FileExists( "nonexistent.xyz" ) );

    // Open and read content (binary: "hello\n" = 6 bytes).
    auto f = fs.Open( "hello.txt", "rb" );
    CHECK( f != nullptr );
    if ( f ) {
        std::byte buf[16]{};
        const xash::filesystem::FsOffset n = f->Read( std::span{ buf } );
        CHECK( n == 6 );
        CHECK( std::memcmp( buf, "hello\n", 6 ) == 0 );
    }

    // LoadFile returns the exact file bytes; size() == on-disk byte count.
    const auto data = fs.LoadFile( "hello.txt" );
    CHECK( !data.empty() );
    CHECK( std::memcmp( data.data(), "hello\n", 6 ) == 0 );

    // gamedironly=true with no GameDir-flagged path → file not found.
    CHECK( !fs.FileExists( "hello.txt", /*gamedironly=*/true ) );

    fs.Shutdown();
}

// ===========================================================================
// 9. WriteFile / FileExists / FileSize
// ===========================================================================

static void test_write_file()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string content = "written content";
    const auto span = std::as_bytes( std::span{ content.data(), content.size() } );

    CHECK( fs.WriteFile( "written.txt", span ) );
    CHECK( fs.FileExists( "written.txt" ) );

    const auto sz = fs.FileSize( "written.txt" );
    CHECK( sz.has_value() );
    if ( sz )
        CHECK( *sz == static_cast<xash::filesystem::FsOffset>( content.size() ) );

    // Clean up the test artefact.
    fs.Delete( "written.txt" );
    fs.Shutdown();
}

// ===========================================================================
// 10. Delete
// ===========================================================================

static void test_delete()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string_view payload = "to be deleted";
    fs.WriteFile( "tmp_delete.txt",
                  std::as_bytes( std::span{ payload.data(), payload.size() } ) );

    CHECK(  fs.FileExists( "tmp_delete.txt" ) );
    CHECK(  fs.Delete( "tmp_delete.txt" ) );
    CHECK( !fs.FileExists( "tmp_delete.txt" ) );

    // Deleting a nonexistent file returns false.
    CHECK( !fs.Delete( "nonexistent.xyz" ) );

    fs.Shutdown();
}

// ===========================================================================
// 11. Rename
// ===========================================================================

static void test_rename()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const std::string_view payload = "rename me";
    fs.WriteFile( "before.txt",
                  std::as_bytes( std::span{ payload.data(), payload.size() } ) );

    CHECK(  fs.FileExists( "before.txt" ) );
    CHECK(  fs.Rename( "before.txt", "after.txt" ) );
    CHECK( !fs.FileExists( "before.txt" ) );
    CHECK(  fs.FileExists( "after.txt" ) );

    // Clean up.
    fs.Delete( "after.txt" );
    fs.Shutdown();
}

// ===========================================================================
// 12. Search
// ===========================================================================

static void test_search()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto result = fs.Search( "*.txt", /*case_insensitive=*/true );
    bool found_hello = false;
    for ( const auto& entry : result.files )
        if ( entry.find( "hello.txt" ) != std::string::npos )
            { found_hello = true; break; }
    CHECK( found_hello );

    // Pattern that matches nothing.
    const auto empty = fs.Search( "*.zzz", true );
    CHECK( empty.files.empty() );

    // Results are de-duplicated (same file appears only once).
    // Add the same directory a second time.
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );
    const auto dedup = fs.Search( "*.txt", true );
    int count_hello = 0;
    for ( const auto& entry : dedup.files )
        if ( entry.find( "hello.txt" ) != std::string::npos )
            ++count_hello;
    CHECK( count_hello == 1 );

    fs.Shutdown();
}

// ===========================================================================
// 13. ClearPaths — non-static removed, Static survives
// ===========================================================================

static void test_clear_paths()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );

    // Non-static path — visible before ClearPaths, gone after.
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );
    CHECK(  fs.FileExists( "hello.txt" ) );
    fs.ClearPaths();
    CHECK( !fs.FileExists( "hello.txt" ) );

    // Static path — survives ClearPaths.
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::Static );
    CHECK(  fs.FileExists( "hello.txt" ) );
    fs.ClearPaths();
    CHECK(  fs.FileExists( "hello.txt" ) );  // still there

    fs.Shutdown();
}

// ===========================================================================
// 14. LoadDirectFile — bypass VFS
// ===========================================================================

static void test_load_direct_file()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );

    const std::string path = ( g_testdir / "direct.txt" ).string();
    const auto data = fs.LoadDirectFile( path );
    CHECK( !data.empty() );
    CHECK( std::memcmp( data.data(), "direct content", 14 ) == 0 );

    // Non-existent path → empty vector.
    const auto none = fs.LoadDirectFile( path + ".nonexistent" );
    CHECK( none.empty() );

    fs.Shutdown();
}

// ===========================================================================
// 15. CRC32File — self-consistent with xash::utilities::crc32
// ===========================================================================

static void test_crc32_file()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto crc = fs.CRC32File( "hello.txt" );
    CHECK( crc.has_value() );
    if ( crc ) {
        // Must match the one-shot utility function on the same bytes.
        const char content[] = "hello\n";
        const auto expected  = xash::utilities::crc32( content, 6 );
        CHECK( *crc == expected );
    }

    // Non-existent file → nullopt.
    CHECK( !fs.CRC32File( "nonexistent.xyz" ).has_value() );

    fs.Shutdown();
}

// ===========================================================================
// 16. MD5File — idempotent on identical content
// ===========================================================================

static void test_md5_file()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    const auto d1 = fs.MD5File( "hello.txt" );
    CHECK( d1.has_value() );

    // Second call on same content must produce the same digest.
    const auto d2 = fs.MD5File( "hello.txt" );
    CHECK( d2.has_value() );
    if ( d1 && d2 )
        CHECK( *d1 == *d2 );

    // Different content → different digest (with overwhelming probability).
    const auto d3 = fs.MD5File( "data.bin" );
    CHECK( d3.has_value() );
    if ( d1 && d3 )
        CHECK( *d1 != *d3 );

    // Non-existent file → nullopt.
    CHECK( !fs.MD5File( "nonexistent.xyz" ).has_value() );

    fs.Shutdown();
}

// ===========================================================================
// 17. Shutdown resets search paths
// ===========================================================================

static void test_shutdown_resets_state()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );
    CHECK( fs.FileExists( "hello.txt" ) );

    fs.Shutdown();
    // All paths (including non-static ones) are cleared.
    CHECK( !fs.FileExists( "hello.txt" ) );
}

// ===========================================================================
// 18. File::Seek / Tell / Eof / Length
// ===========================================================================

static void test_file_seek_tell_eof()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    auto f = fs.Open( "hello.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.Shutdown(); return; }

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

    fs.Shutdown();
}

// ===========================================================================
// 19. File::Gets / Getc / UnGetc
// ===========================================================================

static void test_file_gets_getc()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // Gets reads one text line, strips the terminating '\n'.
    {
        auto f = fs.Open( "hello.txt", "rb" );
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
        auto f = fs.Open( "data.bin", "rb" );
        CHECK( f != nullptr );
        if ( f ) {
            const int c = f->Getc();
            CHECK( c == static_cast<int>( 'b' ) );  // first byte of "binary data"
            f->UnGetc( c );
            CHECK( f->Getc() == c );  // UnGetc pushes back one character
        }
    }

    fs.Shutdown();
}

// ===========================================================================
// main
// ===========================================================================
// 19. FileTime — existing file has a valid time point
// ===========================================================================

static void test_file_time()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // Existing file → has a time point.
    const auto t = fs.FileTime( "hello.txt" );
    CHECK( t.has_value() );

    // Non-existent file → nullopt.
    CHECK( !fs.FileTime( "nonexistent.xyz" ).has_value() );

    fs.Shutdown();
}

// ===========================================================================
// 20. DiskPath — plain-directory file returns on-disk path; missing → nullopt
// ===========================================================================

static void test_disk_path()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    // File that lives in a plain directory → path ending with the filename.
    const auto p = fs.DiskPath( "hello.txt" );
    CHECK( p.has_value() );
    if ( p )
        CHECK( p->find( "hello.txt" ) != std::string::npos );

    // Non-existent file → nullopt.
    CHECK( !fs.DiskPath( "nonexistent.xyz" ).has_value() );

    fs.Shutdown();
}

// ===========================================================================
// 21. AddGameHierarchy — behaves like AddGameDirectory when no _hd/_lv dirs exist
// ===========================================================================

static void test_add_game_hierarchy()
{
    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );

    // No MountHD / MountLV flags → only the plain gamedir is mounted.
    // The _hd / _lv variants do not exist in the fixture, so any attempt to
    // scan them is silently skipped (collect_paths_for_dir on a missing dir
    // returns nothing).
    fs.AddGameHierarchy( gamedir(), xash::filesystem::SearchPathFlags::None );

    CHECK(  fs.FileExists( "hello.txt" ) );
    CHECK( !fs.FileExists( "nonexistent.xyz" ) );

    fs.Shutdown();
}

// ===========================================================================
// 22. Concurrent reads — FileExists / Open / LoadFile from multiple threads
// ===========================================================================

static void test_concurrent_reads()
{
    // All read operations take a shared_lock on paths_mutex, so N threads may
    // run them simultaneously without blocking each other.  This test checks
    // that no crash or data race occurs and that every thread gets the correct
    // result.
    constexpr int N = 8;

    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    std::atomic<int> found_count { 0 };
    std::atomic<int> load_ok     { 0 };

    std::vector<std::thread> threads;
    threads.reserve( N );
    for ( int i = 0; i < N; ++i )
    {
        threads.emplace_back( [&fs, &found_count, &load_ok] {
            if ( fs.FileExists( "hello.txt" ) )
                found_count.fetch_add( 1, std::memory_order_relaxed );

            auto f = fs.Open( "hello.txt", "rb" );
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

    fs.Shutdown();
}

// ===========================================================================
// 23. Reads concurrent with AddGameDirectory (shared vs. exclusive lock)
// ===========================================================================

static void test_concurrent_read_write_paths()
{
    // A writer thread repeatedly adds and clears search paths (exclusive lock)
    // while reader threads call FileExists and LoadFile (shared lock).
    // The test verifies no crash occurs; correctness of individual results is
    // non-deterministic because of the intentional interleaving.
    constexpr int N_READERS = 4;
    constexpr int N_CYCLES  = 32;

    xash::filesystem::Filesystem fs;
    fs.Init( rootdir(), "valve", "game" );
    // Pre-load the directory so readers can find the file at least sometimes.
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::Static );

    std::atomic<bool> done { false };

    // Writer: add + clear (non-static) paths in a tight loop.
    std::thread writer( [&fs, &done] {
        for ( int i = 0; i < N_CYCLES; ++i )
        {
            fs.AddGameDirectory( gamedir(),
                                 xash::filesystem::SearchPathFlags::None );
            fs.ClearPaths();
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
                (void)fs.FileExists( "hello.txt" );
                (void)fs.LoadFile( "hello.txt" );
            }
        } );
    }

    writer.join();
    for ( auto& r : readers ) r.join();

    CHECK( true );  // reaching here without crash or deadlock is the assertion

    fs.Shutdown();
}

// ===========================================================================
// 24. Concurrent Open + Read — each thread owns its File allocation
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
    fs.Init( rootdir(), "valve", "game" );
    fs.AddGameDirectory( gamedir(), xash::filesystem::SearchPathFlags::None );

    std::atomic<int> correct { 0 };

    std::vector<std::thread> threads;
    threads.reserve( N_THREADS );
    for ( int i = 0; i < N_THREADS; ++i )
    {
        threads.emplace_back( [&fs, &correct] {
            for ( int iter = 0; iter < N_ITER; ++iter )
            {
                auto f = fs.Open( "hello.txt", "rb" );
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

    fs.Shutdown();
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
