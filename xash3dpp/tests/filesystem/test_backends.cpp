// xash3dpp — ISearchBackend unit tests
// Covers: DirBackend, PakBackend, ZipBackend, WadBackend
// Legacy reference: filesystem/pak.c, filesystem/zip.c, filesystem/wad.c,
//                   filesystem/filesystem.c  (dir_backend_t)
//
// Each backend is instantiated directly via its create() factory.
// Test archives are written programmatically in setup_testdir() so the test
// is fully self-contained.

#include <xash3dpp/private/filesystem/backends/dir_backend.hpp>
#include <xash3dpp/private/filesystem/backends/pak_backend.hpp>
#include <xash3dpp/private/filesystem/backends/zip_backend.hpp>
#include <xash3dpp/private/filesystem/backends/wad_backend.hpp>

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <miniz.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

using xash::filesystem::FsOffset;
using xash::filesystem::SearchPathFlags;
using namespace xash::filesystem::backends;

static int g_pass = 0, g_fail = 0;
static xash::memory::PoolHandle g_pool;

#include "../test_helpers.hpp"

// ===========================================================================
// Fixture globals
// ===========================================================================

static std::filesystem::path g_testdir;
static std::string           g_dir_root;   // DirBackend root
static std::string           g_pak_path;   // PakBackend archive
static std::string           g_zip_path;   // ZipBackend archive (stored entries)
static std::string           g_wad_path;   // WadBackend archive
static std::string           g_junk_path;  // non-archive file for create-invalid tests

// ===========================================================================
// Helpers
// ===========================================================================

// Drain all readable bytes from a File into a std::string.
static std::string read_all( xash::filesystem::File& f )
{
    std::array<std::byte, 4096> buf{};
    const FsOffset n = f.Read( std::span{buf} );
    if (n <= 0) return {};
    return std::string( reinterpret_cast<const char*>(buf.data()),
                        static_cast<std::size_t>(n) );
}

// ---------------------------------------------------------------------------
// write_pak — minimal PAK with two entries:
//   "scripts/test.txt"  → "hello world" (11 bytes)
//   "textures/logo.bin" → 0x01 0x02 0x03 0x04 (4 bytes)
// ---------------------------------------------------------------------------
static void write_pak( const std::filesystem::path& path )
{
    // Layout:
    //  [  0.. 11]  DiskHeader: "PACK" + dirofs(int32) + dirlen(int32)
    //  [ 12.. 22]  data0: "hello world"
    //  [ 23.. 26]  data1: 0x01 0x02 0x03 0x04
    //  [ 27..154]  Directory: 2 × DiskEntry (64 bytes each)
    //              DiskEntry = name[56] + filepos(int32) + filelen(int32)
    static constexpr std::int32_t k_d0_ofs  = 12;
    static constexpr std::int32_t k_d0_size = 11;
    static constexpr std::int32_t k_d1_ofs  = k_d0_ofs + k_d0_size;   // 23
    static constexpr std::int32_t k_d1_size = 4;
    static constexpr std::int32_t k_dirofs  = k_d1_ofs + k_d1_size;   // 27
    static constexpr std::int32_t k_dirlen  = 2 * 64;                  // 128

    std::ofstream f( path, std::ios::binary );
    // Header
    f.write( "PACK", 4 );
    f.write( reinterpret_cast<const char*>(&k_dirofs),  4 );
    f.write( reinterpret_cast<const char*>(&k_dirlen),  4 );
    // File data
    f.write( "hello world", k_d0_size );
    const char d1[4] = {'\x01', '\x02', '\x03', '\x04'};
    f.write( d1, k_d1_size );
    // Directory entry 0: "scripts/test.txt"
    char name0[56]{};
    std::strncpy( name0, "scripts/test.txt", sizeof(name0) - 1 );
    f.write( name0, 56 );
    f.write( reinterpret_cast<const char*>(&k_d0_ofs),  4 );
    f.write( reinterpret_cast<const char*>(&k_d0_size), 4 );
    // Directory entry 1: "textures/logo.bin"
    char name1[56]{};
    std::strncpy( name1, "textures/logo.bin", sizeof(name1) - 1 );
    f.write( name1, 56 );
    f.write( reinterpret_cast<const char*>(&k_d1_ofs),  4 );
    f.write( reinterpret_cast<const char*>(&k_d1_size), 4 );
}

// ---------------------------------------------------------------------------
// write_zip — requires full miniz archive APIs (disabled by legacy miniz.h).
// Guarded by MINIZ_NO_ARCHIVE_APIS; see xash3dpp CMakeLists.txt comment.
// ---------------------------------------------------------------------------
#ifndef MINIZ_NO_ARCHIVE_APIS
static void write_zip( const std::filesystem::path& path )
{
    mz_zip_archive za{};
    mz_zip_writer_init_file( &za, path.string().c_str(), 0 );
    mz_zip_writer_add_mem( &za, "scripts/test.txt",
                           "hello world", 11,
                           MZ_NO_COMPRESSION );
    const char d1[4] = {'\x01', '\x02', '\x03', '\x04'};
    mz_zip_writer_add_mem( &za, "textures/logo.bin",
                           d1, 4,
                           MZ_NO_COMPRESSION );
    mz_zip_writer_finalize_archive( &za );
    mz_zip_writer_end( &za );
}
#endif // !MINIZ_NO_ARCHIVE_APIS

// ---------------------------------------------------------------------------
// write_wad — minimal WAD3 with one lump:
//   "test" (type 68 = TYP_SCRIPT) → "wadlump" (7 bytes)
//
// Lookup path via WadBackend API: "test.txt"
//   (extension "txt" → TYP_SCRIPT=68; stem stripped → name="test")
// ---------------------------------------------------------------------------
static void write_wad( const std::filesystem::path& path )
{
    // On-disk layout:
    //  [  0.. 11]  DiskHeader: "WAD3" + numlumps(int32) + infotableofs(int32)
    //  [ 12.. 18]  lump data: "wadlump" (7 bytes)
    //  [ 19.. 50]  DiskLump (32 bytes):
    //              filepos(4) + disksize(4) + size(4) +
    //              type(1) + attribs(1) + pad(1) + pad(1) + name[16]
    static constexpr std::int32_t k_numlumps  = 1;
    static constexpr std::int32_t k_data_ofs  = 12;
    static constexpr std::int32_t k_data_size = 7;
    static constexpr std::int32_t k_lat_ofs   = k_data_ofs + k_data_size;  // 19

    std::ofstream f( path, std::ios::binary );
    // Header
    f.write( "WAD3", 4 );
    f.write( reinterpret_cast<const char*>(&k_numlumps), 4 );
    f.write( reinterpret_cast<const char*>(&k_lat_ofs),  4 );
    // Lump data
    f.write( "wadlump", k_data_size );
    // DiskLump
    f.write( reinterpret_cast<const char*>(&k_data_ofs),  4 );   // filepos
    f.write( reinterpret_cast<const char*>(&k_data_size), 4 );   // disksize
    f.write( reinterpret_cast<const char*>(&k_data_size), 4 );   // size (uncompressed)
    const char typ = 68, att = 0, pad = 0;
    f.write( &typ, 1 );
    f.write( &att, 1 );
    f.write( &pad, 1 );
    f.write( &pad, 1 );
    char name[16]{};
    std::strncpy( name, "test", sizeof(name) - 1 );
    f.write( name, 16 );
}

static void setup_testdir()
{
    g_testdir = std::filesystem::temp_directory_path() / "xash3dpp_backends_test";
    std::filesystem::remove_all( g_testdir );
    std::filesystem::create_directories( g_testdir );

    // DirBackend fixture
    const auto dir = g_testdir / "dir";
    std::filesystem::create_directories( dir / "sub" );
    {
        std::ofstream f( dir / "hello.txt", std::ios::binary );
        f.write( "hello world", 11 );
    }
    {
        std::ofstream f( dir / "data.bin", std::ios::binary );
        const char b[4] = {'\x01', '\x02', '\x03', '\x04'};
        f.write( b, 4 );
    }
    {
        std::ofstream f( dir / "sub" / "nested.txt", std::ios::binary );
        f.write( "nested", 6 );
    }
    g_dir_root = dir.string();

    // PakBackend fixture
    {
        const auto p = g_testdir / "test.pak";
        write_pak( p );
        g_pak_path = p.string();
    }

    // ZipBackend fixture
#ifndef MINIZ_NO_ARCHIVE_APIS
    {
        const auto z = g_testdir / "test.zip";
        write_zip( z );
        g_zip_path = z.string();
    }
#endif // !MINIZ_NO_ARCHIVE_APIS

    // WadBackend fixture
    {
        const auto w = g_testdir / "test.wad";
        write_wad( w );
        g_wad_path = w.string();
    }

    // Junk file — used for create-invalid tests across all archive backends
    {
        const auto j = g_testdir / "junk.bin";
        std::ofstream f( j, std::ios::binary );
        f.write( "not an archive", 14 );
        g_junk_path = j.string();
    }
}

// ===========================================================================
// DirBackend tests
// ===========================================================================

static void test_dir_backend_create()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    CHECK( b != nullptr );
}

static void test_dir_backend_info()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    // info() returns the root path — just check it's non-empty
    CHECK( !b->info().empty() );
}

static void test_dir_backend_find_file()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->find_file( "hello.txt" ).has_value() );
    CHECK( !b->find_file( "ghost.txt" ).has_value() );
}

static void test_dir_backend_load_file()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto data = b->load_file( "hello.txt" );
    CHECK( data.size() == 11u );
    CHECK( std::memcmp( data.data(), "hello world", 11 ) == 0 );
}

static void test_dir_backend_load_file_missing()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->load_file( "ghost.txt" ).empty() );
}

static void test_dir_backend_open_file_read()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    auto f = b->open_file( "hello.txt", "rb" );
    CHECK( f != nullptr );
    if (!f) return;
    CHECK( read_all( *f ) == "hello world" );
}

static void test_dir_backend_open_file_missing()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "ghost.txt", "rb" ) == nullptr );
}

static void test_dir_backend_write_creates_file()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    auto f = b->open_file( "new_write.txt", "wb" );
    CHECK( f != nullptr );
    if (!f) return;
    const char msg[] = "written";
    std::array<std::byte, 7> buf;
    std::memcpy( buf.data(), msg, 7 );
    CHECK( f->Write( std::span<const std::byte>{buf} ) == 7 );
    f.reset();
    // The OS file must now exist on disk.
    CHECK( std::filesystem::exists( g_testdir / "dir" / "new_write.txt" ) );
}

static void test_dir_backend_file_time()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->file_time( "hello.txt" ).has_value() );
    CHECK( !b->file_time( "ghost.txt" ).has_value() );
}

static void test_dir_backend_search()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto results = b->search( "*.txt", true );
    // "hello.txt" must appear; "data.bin" must not.
    const bool has_hello = std::find( results.begin(), results.end(),
                                      "hello.txt" ) != results.end();
    CHECK( has_hello );
    const bool has_bin   = std::find( results.begin(), results.end(),
                                      "data.bin" ) != results.end();
    CHECK( !has_bin );
}

static void test_dir_backend_nested_find()
{
    auto b = DirBackend::create( g_pool, g_dir_root, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->find_file( "sub/nested.txt" ).has_value() );
}

// ===========================================================================
// PakBackend tests
// ===========================================================================

static void test_pak_backend_create_invalid()
{
    // File with wrong magic → create returns nullptr.
    CHECK( PakBackend::create( g_pool, g_junk_path, SearchPathFlags::None ) == nullptr );
}

static void test_pak_backend_create_missing()
{
    const std::string absent = (g_testdir / "absent.pak").string();
    CHECK( PakBackend::create( g_pool, absent, SearchPathFlags::None ) == nullptr );
}

static void test_pak_backend_create_valid()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    CHECK( b != nullptr );
}

static void test_pak_backend_info()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    // info() = "path (2 files)" — must contain "2".
    CHECK( b->info().find("2") != std::string::npos );
}

static void test_pak_backend_find_file()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->find_file( "scripts/test.txt" ).has_value() );
    CHECK( !b->find_file( "scripts/missing.txt" ).has_value() );
}

static void test_pak_backend_find_file_case_insensitive()
{
    // PAK entries are binary-searched case-insensitively (mirrors Q_stricmp).
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->find_file( "SCRIPTS/TEST.TXT" ).has_value() );
    CHECK( b->find_file( "Scripts/Test.Txt" ).has_value() );
}

static void test_pak_backend_load_file()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto data = b->load_file( "scripts/test.txt" );
    CHECK( data.size() == 11u );
    CHECK( std::memcmp( data.data(), "hello world", 11 ) == 0 );
}

static void test_pak_backend_load_file_missing()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->load_file( "scripts/missing.txt" ).empty() );
}

static void test_pak_backend_open_file()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    auto f = b->open_file( "scripts/test.txt", "rb" );
    CHECK( f != nullptr );
    if (!f) return;
    CHECK( read_all( *f ) == "hello world" );
}

static void test_pak_backend_open_file_missing()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "scripts/missing.txt", "rb" ) == nullptr );
}

static void test_pak_backend_write_rejected()
{
    // PAK archives are read-only; write/append mode must return nullptr.
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "scripts/test.txt", "wb" ) == nullptr );
    CHECK( b->open_file( "scripts/test.txt", "ab" ) == nullptr );
}

static void test_pak_backend_file_time()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    // PAK returns the archive-level mtime for any found entry.
    CHECK(  b->file_time( "scripts/test.txt" ).has_value() );
    CHECK( !b->file_time( "scripts/missing.txt" ).has_value() );
}

static void test_pak_backend_search()
{
    auto b = PakBackend::create( g_pool, g_pak_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto results = b->search( "*.txt", true );
    // archive_search_by_name: "scripts/test.txt" matches "*.txt" (wildcard
    // spans '/') and then "scripts" is tried but does not match "*.txt".
    const bool found = std::find( results.begin(), results.end(),
                                  "scripts/test.txt" ) != results.end();
    CHECK( found );
    // "textures/logo.bin" must not appear.
    const bool has_bin = std::find( results.begin(), results.end(),
                                    "textures/logo.bin" ) != results.end();
    CHECK( !has_bin );
}

// ===========================================================================
// ZipBackend tests
// ===========================================================================
#ifndef MINIZ_NO_ARCHIVE_APIS

static void test_zip_backend_create_invalid()
{
    CHECK( ZipBackend::create( g_pool, g_junk_path, SearchPathFlags::None ) == nullptr );
}

static void test_zip_backend_create_missing()
{
    const std::string absent = (g_testdir / "absent.zip").string();
    CHECK( ZipBackend::create( g_pool, absent, SearchPathFlags::None ) == nullptr );
}

static void test_zip_backend_create_valid()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    CHECK( b != nullptr );
}

static void test_zip_backend_info()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->info().find("2") != std::string::npos );
}

static void test_zip_backend_find_file()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->find_file( "scripts/test.txt" ).has_value() );
    CHECK( !b->find_file( "scripts/missing.txt" ).has_value() );
}

static void test_zip_backend_find_file_case_insensitive()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->find_file( "SCRIPTS/TEST.TXT" ).has_value() );
}

static void test_zip_backend_load_file()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto data = b->load_file( "scripts/test.txt" );
    CHECK( data.size() == 11u );
    CHECK( std::memcmp( data.data(), "hello world", 11 ) == 0 );
}

static void test_zip_backend_load_file_missing()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->load_file( "scripts/missing.txt" ).empty() );
}

static void test_zip_backend_open_file()
{
    // Entries are stored (MZ_NO_COMPRESSION) so Read goes through the direct
    // (non-inflate) path — exercises make_os_file with deflated=false.
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    auto f = b->open_file( "scripts/test.txt", "rb" );
    CHECK( f != nullptr );
    if (!f) return;
    CHECK( read_all( *f ) == "hello world" );
}

static void test_zip_backend_open_file_missing()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "scripts/missing.txt", "rb" ) == nullptr );
}

static void test_zip_backend_write_rejected()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "scripts/test.txt", "wb" ) == nullptr );
    CHECK( b->open_file( "scripts/test.txt", "ab" ) == nullptr );
}

static void test_zip_backend_file_time()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->file_time( "scripts/test.txt" ).has_value() );
    CHECK( !b->file_time( "scripts/missing.txt" ).has_value() );
}

static void test_zip_backend_search()
{
    auto b = ZipBackend::create( g_pool, g_zip_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto results = b->search( "*.txt", true );
    const bool found = std::find( results.begin(), results.end(),
                                  "scripts/test.txt" ) != results.end();
    CHECK( found );
    const bool has_bin = std::find( results.begin(), results.end(),
                                    "textures/logo.bin" ) != results.end();
    CHECK( !has_bin );
}
#endif // !MINIZ_NO_ARCHIVE_APIS

// ===========================================================================
// WadBackend tests
// ===========================================================================

static void test_wad_backend_create_invalid()
{
    CHECK( WadBackend::create( g_pool, g_junk_path, SearchPathFlags::None ) == nullptr );
}

static void test_wad_backend_create_missing()
{
    const std::string absent = (g_testdir / "absent.wad").string();
    CHECK( WadBackend::create( g_pool, absent, SearchPathFlags::None ) == nullptr );
}

static void test_wad_backend_create_valid()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    CHECK( b != nullptr );
}

static void test_wad_backend_info()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( !b->info().empty() );
}

static void test_wad_backend_find_file()
{
    // "test.txt" → ext "txt" → TYP_SCRIPT=68; strip → lump name "test"
    // find_file returns e->name (the stored lowercase lump name, no extension).
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto result = b->find_file( "test.txt" );
    CHECK( result.has_value() );
    if (result) CHECK( *result == "test" );
    // Unknown extension → TYP_NONE → nullopt
    CHECK( !b->find_file( "test.xyz" ).has_value() );
    // Entry with wrong name → nullopt
    CHECK( !b->find_file( "other.txt" ).has_value() );
}

static void test_wad_backend_load_file()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    const auto data = b->load_file( "test.txt" );
    CHECK( data.size() == 7u );
    CHECK( std::memcmp( data.data(), "wadlump", 7 ) == 0 );
}

static void test_wad_backend_load_file_missing()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->load_file( "missing.txt" ).empty() );
}

static void test_wad_backend_open_file()
{
    // WAD::open_file returns a MemFile loaded with the full lump content.
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    auto f = b->open_file( "test.txt", "rb" );
    CHECK( f != nullptr );
    if (!f) return;
    CHECK( read_all( *f ) == "wadlump" );
}

static void test_wad_backend_open_file_missing()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "missing.txt", "rb" ) == nullptr );
}

static void test_wad_backend_write_rejected()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->open_file( "test.txt", "wb" ) == nullptr );
    CHECK( b->open_file( "test.txt", "ab" ) == nullptr );
}

static void test_wad_backend_file_time()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->file_time( "test.txt" ).has_value() );
    CHECK( !b->file_time( "missing.txt" ).has_value() );
}

static void test_wad_backend_search()
{
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    // "*.txt" → TYP_SCRIPT=68; entry "test" matches → result is "test.txt"
    const auto results = b->search( "*.txt", true );
    const bool found = std::find( results.begin(), results.end(),
                                  "test.txt" ) != results.end();
    CHECK( found );
    // "*.mip" → TYP_MIPTEX=67; our entry has type 68 → no results.
    CHECK( b->search( "*.mip", true ).empty() );
}

static void test_wad_backend_qualifier_match()
{
    // "test.wad/test.txt" — dir qualifier must match WAD stem ("test").
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK(  b->find_file( "test.wad/test.txt" ).has_value() );
    // Case-insensitive qualifier match
    CHECK(  b->find_file( "TEST.WAD/test.txt" ).has_value() );
    // Wrong WAD name → nullopt
    CHECK( !b->find_file( "other.wad/test.txt" ).has_value() );
}

static void test_wad_backend_case_insensitive_lookup()
{
    // WAD normalises all names to lowercase on load; lookups must be CI.
    // "TEST.TXT" → strip_extension → "test" (after to_lower in lookup).
    auto b = WadBackend::create( g_pool, g_wad_path, SearchPathFlags::None );
    if (!b) { ++g_fail; return; }
    CHECK( b->find_file( "TEST.TXT" ).has_value() );
    CHECK( b->find_file( "Test.Txt" ).has_value() );
}

// ===========================================================================
// main
// ===========================================================================

int main()
{
    setup_testdir();
    g_pool = xash::memory::create_pool("test_backends");

    // --- DirBackend ---------------------------------------------------------
    RUN_TEST( test_dir_backend_create );
    RUN_TEST( test_dir_backend_info );
    RUN_TEST( test_dir_backend_find_file );
    RUN_TEST( test_dir_backend_load_file );
    RUN_TEST( test_dir_backend_load_file_missing );
    RUN_TEST( test_dir_backend_open_file_read );
    RUN_TEST( test_dir_backend_open_file_missing );
    RUN_TEST( test_dir_backend_write_creates_file );
    RUN_TEST( test_dir_backend_file_time );
    RUN_TEST( test_dir_backend_search );
    RUN_TEST( test_dir_backend_nested_find );

    // --- PakBackend ---------------------------------------------------------
    RUN_TEST( test_pak_backend_create_invalid );
    RUN_TEST( test_pak_backend_create_missing );
    RUN_TEST( test_pak_backend_create_valid );
    RUN_TEST( test_pak_backend_info );
    RUN_TEST( test_pak_backend_find_file );
    RUN_TEST( test_pak_backend_find_file_case_insensitive );
    RUN_TEST( test_pak_backend_load_file );
    RUN_TEST( test_pak_backend_load_file_missing );
    RUN_TEST( test_pak_backend_open_file );
    RUN_TEST( test_pak_backend_open_file_missing );
    RUN_TEST( test_pak_backend_write_rejected );
    RUN_TEST( test_pak_backend_file_time );
    RUN_TEST( test_pak_backend_search );

    // --- ZipBackend ---------------------------------------------------------
#ifndef MINIZ_NO_ARCHIVE_APIS
    RUN_TEST( test_zip_backend_create_invalid );
    RUN_TEST( test_zip_backend_create_missing );
    RUN_TEST( test_zip_backend_create_valid );
    RUN_TEST( test_zip_backend_info );
    RUN_TEST( test_zip_backend_find_file );
    RUN_TEST( test_zip_backend_find_file_case_insensitive );
    RUN_TEST( test_zip_backend_load_file );
    RUN_TEST( test_zip_backend_load_file_missing );
    RUN_TEST( test_zip_backend_open_file );
    RUN_TEST( test_zip_backend_open_file_missing );
    RUN_TEST( test_zip_backend_write_rejected );
    RUN_TEST( test_zip_backend_file_time );
    RUN_TEST( test_zip_backend_search );
#else
    std::printf( "  ZipBackend tests skipped: legacy miniz has MINIZ_NO_ARCHIVE_APIS\n" );
#endif // !MINIZ_NO_ARCHIVE_APIS

    // --- WadBackend ---------------------------------------------------------
    RUN_TEST( test_wad_backend_create_invalid );
    RUN_TEST( test_wad_backend_create_missing );
    RUN_TEST( test_wad_backend_create_valid );
    RUN_TEST( test_wad_backend_info );
    RUN_TEST( test_wad_backend_find_file );
    RUN_TEST( test_wad_backend_load_file );
    RUN_TEST( test_wad_backend_load_file_missing );
    RUN_TEST( test_wad_backend_open_file );
    RUN_TEST( test_wad_backend_open_file_missing );
    RUN_TEST( test_wad_backend_write_rejected );
    RUN_TEST( test_wad_backend_file_time );
    RUN_TEST( test_wad_backend_search );
    RUN_TEST( test_wad_backend_qualifier_match );
    RUN_TEST( test_wad_backend_case_insensitive_lookup );

    xash::memory::destroy_pool(g_pool);
    std::printf( "backends: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
