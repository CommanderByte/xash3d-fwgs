// xash3dpp — File streaming I/O tests
// Covers: File::Read, Write, Seek, Tell, Length, Eof, Flush,
//         File::Gets, Getc, UnGetc
//
// All tests obtain a File via Filesystem::open (OsFile is private).
// Legacy reference: filesystem/filesystem.c  (FS_Read, FS_Write, FS_Seek,
//                   FS_Tell, FS_Eof, FS_Gets, FS_Getc, FS_UnGetc, FS_Flush)

#include <xash3dpp/filesystem/file.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

static int g_pass = 0, g_fail = 0;

#include "../test_helpers.hpp"

// ===========================================================================
// Fixture
// ===========================================================================

static std::filesystem::path g_testdir;

// Create test files used across all groups:
//   simple.txt     — "abcdef"          (6 bytes, no newline)
//   multiline.txt  — "first\nsecond\nthird"  (no trailing newline)
//   crlf.txt       — "line1\r\nline2\r\n"    (Windows line endings)
//   empty.txt      — ""               (0 bytes)
//   newline.txt    — "\n"             (single newline)
static void setup_testdir()
{
    g_testdir = std::filesystem::temp_directory_path() / "xash3dpp_file_test";
    std::filesystem::remove_all( g_testdir );
    std::filesystem::create_directories( g_testdir );

    auto write = [](const std::filesystem::path& p, const char* data, std::size_t len)
    {
        std::ofstream f( p, std::ios::binary );
        f.write( data, static_cast<std::streamsize>( len ) );
    };

    write( g_testdir / "simple.txt",    "abcdef",                 6  );
    write( g_testdir / "multiline.txt", "first\nsecond\nthird",   18 );
    write( g_testdir / "crlf.txt",      "line1\r\nline2\r\n",     14 );
    write( g_testdir / "empty.txt",     "",                        0  );
    write( g_testdir / "newline.txt",   "\n",                      1  );
}

static void teardown_testdir()
{
    std::filesystem::remove_all( g_testdir );
}

// Each test builds its own Filesystem to keep state isolated.
static xash::filesystem::Filesystem make_fs()
{
    xash::filesystem::Filesystem fs;
    fs.init( g_testdir.string(), "valve", "game" );
    fs.add_game_directory( g_testdir.string(),
                         xash::filesystem::SearchPathFlags::None );
    return fs;
}

// ===========================================================================
// 1. Length
// ===========================================================================

static void test_file_length()
{
    // legacy: FS_FileLength / file_t::real_length
    auto fs = make_fs();

    auto f = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( f )
        CHECK( f->Length() == 6 );  // "abcdef" is 6 bytes

    auto e = fs.open( "empty.txt", "rb" );
    CHECK( e != nullptr );
    if ( e )
        CHECK( e->Length() == 0 );

    f.reset();
    e.reset();
    fs.shutdown();
}

// ===========================================================================
// 2. Read — exact, partial, and past-EOF
// ===========================================================================

static void test_file_read_exact()
{
    // legacy: FS_Read
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    std::byte buf[8]{};
    // Read exactly the file's 6 bytes.
    const xash::filesystem::FsOffset n = f->Read( std::span{ buf, 6 } );
    CHECK( n == 6 );
    CHECK( std::memcmp( buf, "abcdef", 6 ) == 0 );

    // A second Read at EOF returns 0 (C++ contract; not legacy's 1-for-empty).
    const xash::filesystem::FsOffset n2 = f->Read( std::span{ buf } );
    CHECK( n2 == 0 );

    f.reset();
    fs.shutdown();
}

static void test_file_read_partial()
{
    // legacy: FS_Read — partial reads leave the cursor mid-file
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    std::byte buf[3]{};
    const xash::filesystem::FsOffset n = f->Read( std::span{ buf } );
    CHECK( n == 3 );
    CHECK( std::memcmp( buf, "abc", 3 ) == 0 );

    // Tell reflects the bytes consumed.
    CHECK( f->Tell() == 3 );

    // Second partial read gets the remaining bytes.
    std::byte buf2[3]{};
    const xash::filesystem::FsOffset n2 = f->Read( std::span{ buf2 } );
    CHECK( n2 == 3 );
    CHECK( std::memcmp( buf2, "def", 3 ) == 0 );

    f.reset();
    fs.shutdown();
}

static void test_file_read_empty_buf()
{
    // C++ contract: Read with zero-length span returns 0 without advancing cursor.
    // (Legacy FS_Read returns 1 for buffersize==0 — that was a C quirk; the
    // C++ rewrite does not preserve it as the return type conveys no useful info.)
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const xash::filesystem::FsOffset n = f->Read( {} );
    CHECK( n == 0 );
    CHECK( f->Tell() == 0 );  // cursor must not have moved

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 3. Tell — advances with reads
// ===========================================================================

static void test_file_tell_advances_on_read()
{
    // legacy: FS_Tell
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    CHECK( f->Tell() == 0 );

    std::byte buf[2]{};
    f->Read( std::span{ buf } );
    CHECK( f->Tell() == 2 );

    f->Read( std::span{ buf } );
    CHECK( f->Tell() == 4 );

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 4. Eof — true only after all bytes consumed
// ===========================================================================

static void test_file_eof_after_full_read()
{
    // legacy: FS_Eof — (position == real_length)
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    CHECK( !f->Eof() );

    std::byte buf[6]{};
    f->Read( std::span{ buf } );

    CHECK( f->Eof() );

    f.reset();
    fs.shutdown();
}

static void test_file_eof_empty_file()
{
    // An empty file is at EOF immediately on open.
    auto fs = make_fs();
    auto f  = fs.open( "empty.txt", "rb" );
    CHECK( f != nullptr );
    if ( f )
        CHECK( f->Eof() );
    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 5. Seek
// ===========================================================================

static void test_file_seek_begin()
{
    // legacy: FS_Seek with SEEK_SET
    // NOTE: C++ Seek returns the target absolute position (not 0 like FS_Seek).
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const xash::filesystem::FsOffset r1 = f->Seek( 3, xash::filesystem::SeekOrigin::Begin );
    CHECK( r1 == 3 );
    CHECK( f->Tell() == 3 );

    const xash::filesystem::FsOffset r2 = f->Seek( 0, xash::filesystem::SeekOrigin::Begin );
    CHECK( r2 == 0 );
    CHECK( f->Tell() == 0 );

    f.reset();
    fs.shutdown();
}

static void test_file_seek_current()
{
    // legacy: FS_Seek with SEEK_CUR
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    f->Seek( 2, xash::filesystem::SeekOrigin::Begin );

    // Forward from current.
    const xash::filesystem::FsOffset r1 = f->Seek( 1, xash::filesystem::SeekOrigin::Current );
    CHECK( r1 == 3 );
    CHECK( f->Tell() == 3 );

    // Backward from current.
    const xash::filesystem::FsOffset r2 = f->Seek( -1, xash::filesystem::SeekOrigin::Current );
    CHECK( r2 == 2 );
    CHECK( f->Tell() == 2 );

    // Zero offset is a no-op.
    const xash::filesystem::FsOffset r3 = f->Seek( 0, xash::filesystem::SeekOrigin::Current );
    CHECK( r3 == 2 );

    f.reset();
    fs.shutdown();
}

static void test_file_seek_end()
{
    // legacy: FS_Seek with SEEK_END
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    // Seek to exact end.
    const xash::filesystem::FsOffset r1 = f->Seek( 0, xash::filesystem::SeekOrigin::End );
    CHECK( r1 == 6 );
    CHECK( f->Tell() == 6 );
    CHECK( f->Eof() );

    // Seek to 2 bytes before end.
    const xash::filesystem::FsOffset r2 = f->Seek( -2, xash::filesystem::SeekOrigin::End );
    CHECK( r2 == 4 );
    CHECK( f->Tell() == 4 );
    CHECK( !f->Eof() );

    f.reset();
    fs.shutdown();
}

static void test_file_seek_out_of_bounds()
{
    // legacy: FS_Seek returns -1 for offset < 0 or offset > real_length
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    // Negative absolute position.
    CHECK( f->Seek( -1, xash::filesystem::SeekOrigin::Begin ) == -1 );

    // Past end of file.
    CHECK( f->Seek( 7, xash::filesystem::SeekOrigin::Begin ) == -1 );

    // Cursor must not have moved after a failed seek.
    CHECK( f->Tell() == 0 );

    f.reset();
    fs.shutdown();
}

static void test_file_seek_followed_by_read()
{
    // Seek then Read must return the bytes at the new cursor position.
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    f->Seek( 3, xash::filesystem::SeekOrigin::Begin );

    std::byte buf[3]{};
    const xash::filesystem::FsOffset n = f->Read( std::span{ buf } );
    CHECK( n == 3 );
    CHECK( std::memcmp( buf, "def", 3 ) == 0 );

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 6. Write
// ===========================================================================

static void test_file_write()
{
    // legacy: FS_Write — returns bytes written
    auto fs = make_fs();

    {
        auto f = fs.open( "written.txt", "wb" );
        CHECK( f != nullptr );
        if ( !f ) { fs.shutdown(); return; }

        const std::string data = "hello world";
        const auto sp = std::as_bytes( std::span{ data.data(), data.size() } );
        const xash::filesystem::FsOffset n = f->Write( sp );
        CHECK( n == static_cast<xash::filesystem::FsOffset>( data.size() ) );
    }
    // Verify content persists (file is closed when f goes out of scope).
    const auto loaded = fs.load_file( "written.txt" );
    CHECK( loaded.size() == 11 );
    CHECK( std::memcmp( loaded.data(), "hello world", 11 ) == 0 );

    fs.remove( "written.txt" );
    fs.shutdown();
}

static void test_file_write_empty()
{
    // Writing zero bytes returns 0 without error.
    auto fs = make_fs();

    {
        auto f = fs.open( "zero.txt", "wb" );
        CHECK( f != nullptr );
        if ( f ) {
            const xash::filesystem::FsOffset n = f->Write( {} );
            CHECK( n == 0 );
        }
    }

    fs.remove( "zero.txt" );
    fs.shutdown();
}

// ===========================================================================
// 7. Flush — smoke test
// ===========================================================================

static void test_file_flush()
{
    // legacy: FS_Flush — fsync / _commit; just verify it doesn't crash
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( f )
        f->Flush();   // observable only as "no crash / no assert"
    CHECK( true );
    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 8. Gets
// ===========================================================================

static void test_file_gets_no_newline()
{
    // legacy: FS_Gets — reads until '\n' or EOF; here EOF is hit first
    // Expected: returns "abcdef", second call returns nullopt (EOF)
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const auto line = f->Gets();
    CHECK( line.has_value() );
    if ( line )
        CHECK( *line == "abcdef" );

    // EOF on next call.
    const auto eof_line = f->Gets();
    CHECK( !eof_line.has_value() );

    f.reset();
    fs.shutdown();
}

static void test_file_gets_multiline()
{
    // legacy: FS_Gets — each call returns one line, '\n' consumed and stripped
    // "first\nsecond\nthird" → "first", "second", "third", nullopt
    auto fs = make_fs();
    auto f  = fs.open( "multiline.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const auto l1 = f->Gets();
    CHECK( l1.has_value() && *l1 == "first" );

    const auto l2 = f->Gets();
    CHECK( l2.has_value() && *l2 == "second" );

    const auto l3 = f->Gets();
    CHECK( l3.has_value() && *l3 == "third" );  // no trailing newline → reads to EOF

    const auto l4 = f->Gets();
    CHECK( !l4.has_value() );  // EOF

    f.reset();
    fs.shutdown();
}

static void test_file_gets_crlf()
{
    // legacy: FS_Gets consumes '\r' inline and discards the following '\n'
    // "line1\r\nline2\r\n" → "line1", "line2"  (neither \r nor \n appear in output)
    auto fs = make_fs();
    auto f  = fs.open( "crlf.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const auto l1 = f->Gets();
    CHECK( l1.has_value() && *l1 == "line1" );

    const auto l2 = f->Gets();
    CHECK( l2.has_value() && *l2 == "line2" );

    const auto l3 = f->Gets();
    CHECK( !l3.has_value() );  // EOF

    f.reset();
    fs.shutdown();
}

static void test_file_gets_empty_line()
{
    // A bare '\n' → Gets reads it as an empty-but-present line: optional{""}.
    // (Contrast with nullopt which means EOF before any char was read.)
    auto fs = make_fs();
    auto f  = fs.open( "newline.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const auto line = f->Gets();
    CHECK( line.has_value() );
    if ( line )
        CHECK( line->empty() );  // stripped '\n', so ""

    CHECK( !f->Gets().has_value() );  // now at EOF

    f.reset();
    fs.shutdown();
}

static void test_file_gets_eof()
{
    // Gets on an already-exhausted file returns nullopt immediately.
    auto fs = make_fs();
    auto f  = fs.open( "empty.txt", "rb" );
    CHECK( f != nullptr );
    if ( f )
        CHECK( !f->Gets().has_value() );
    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 9. Getc
// ===========================================================================

static void test_file_getc_sequence()
{
    // legacy: FS_Getc — reads one byte at a time
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    CHECK( f->Getc() == static_cast<int>( 'a' ) );
    CHECK( f->Getc() == static_cast<int>( 'b' ) );
    CHECK( f->Getc() == static_cast<int>( 'c' ) );
    CHECK( f->Getc() == static_cast<int>( 'd' ) );
    CHECK( f->Getc() == static_cast<int>( 'e' ) );
    CHECK( f->Getc() == static_cast<int>( 'f' ) );

    f.reset();
    fs.shutdown();
}

static void test_file_getc_eof()
{
    // legacy: FS_Getc returns EOF when no bytes remain
    auto fs = make_fs();
    auto f  = fs.open( "empty.txt", "rb" );
    CHECK( f != nullptr );
    if ( f )
        CHECK( f->Getc() == EOF );
    f.reset();
    fs.shutdown();
}

// ===========================================================================
// 10. UnGetc
// ===========================================================================

static void test_file_ungetc_same_value()
{
    // legacy: FS_UnGetc — push-back one char; next Getc returns it
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    const int c = f->Getc();          // 'a'
    CHECK( c == static_cast<int>( 'a' ) );
    f->UnGetc( c );
    CHECK( f->Getc() == c );          // returns 'a' again
    CHECK( f->Getc() == static_cast<int>( 'b' ) );  // stream continues from 'b'

    f.reset();
    fs.shutdown();
}

static void test_file_ungetc_different_value()
{
    // UnGetc with a different value than what was read: the pushed-back value
    // is returned, then the stream resumes from its current position (not
    // re-replaying the original byte).
    auto fs = make_fs();
    auto f  = fs.open( "simple.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    f->Getc();                          // consume 'a', cursor now at 'b'
    f->UnGetc( static_cast<int>( 'z' ) );

    CHECK( f->Getc() == static_cast<int>( 'z' ) );  // pushed-back value
    CHECK( f->Getc() == static_cast<int>( 'b' ) );  // stream resumes at 'b'

    f.reset();
    fs.shutdown();
}

static void test_file_ungetc_after_gets()
{
    // Gets internally uses Getc; UnGetc works correctly after a Gets call.
    auto fs = make_fs();
    auto f  = fs.open( "multiline.txt", "rb" );
    CHECK( f != nullptr );
    if ( !f ) { fs.shutdown(); return; }

    f->Gets();  // consume "first\n"

    // Peek first char of next line, push it back, verify Gets still works.
    const int peek = f->Getc();           // 's' from "second"
    CHECK( peek == static_cast<int>( 's' ) );
    f->UnGetc( peek );

    const auto second = f->Gets();
    CHECK( second.has_value() && *second == "second" );

    f.reset();
    fs.shutdown();
}

// ===========================================================================
// main
// ===========================================================================

int main()
{
    setup_testdir();

    test_file_length();

    test_file_read_exact();
    test_file_read_partial();
    test_file_read_empty_buf();
    test_file_tell_advances_on_read();
    test_file_eof_after_full_read();
    test_file_eof_empty_file();

    test_file_seek_begin();
    test_file_seek_current();
    test_file_seek_end();
    test_file_seek_out_of_bounds();
    test_file_seek_followed_by_read();

    test_file_write();
    test_file_write_empty();

    test_file_flush();

    test_file_gets_no_newline();
    test_file_gets_multiline();
    test_file_gets_crlf();
    test_file_gets_empty_line();
    test_file_gets_eof();

    test_file_getc_sequence();
    test_file_getc_eof();

    test_file_ungetc_same_value();
    test_file_ungetc_different_value();
    test_file_ungetc_after_gets();

    teardown_testdir();

    std::printf( "file: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
