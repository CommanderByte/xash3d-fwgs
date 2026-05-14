// xash3dpp — path utility tests
// Covers: file_extension, filename, file_base, strip_extension,
//         fix_slashes, extract_dir, default_extension, replace_extension,
//         remove_line_feed, trim_space

#include <xash3dpp/utilities/path.hpp>

#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::puts("FAIL: " #expr " (" __FILE__ ")"); } } while(0)

// ---------------------------------------------------------------------------
// file_extension — returns string_view (includes dot)
// ---------------------------------------------------------------------------

static void test_file_extension()
{
    // legacy: COM_FileExtension
    CHECK( xash::utilities::file_extension( "file.bsp" )      == ".bsp" );
    CHECK( xash::utilities::file_extension( "path/to/map.bsp" ) == ".bsp" );
    CHECK( xash::utilities::file_extension( "file.ext.gz" )   == ".gz" );  // last dot wins

    // No extension → empty view.
    CHECK( xash::utilities::file_extension( "file" ).empty() );
    CHECK( xash::utilities::file_extension( "" ).empty() );

    // Dot in directory component only — no extension on the filename.
    CHECK( xash::utilities::file_extension( "dir.d/file" ).empty() );

    // Leading-dot filename: the dot-and-suffix is the extension.
    CHECK( xash::utilities::file_extension( ".hidden" ) == ".hidden" );
}

// ---------------------------------------------------------------------------
// filename — pointer into original string
// ---------------------------------------------------------------------------

static void test_filename()
{
    // legacy: COM_FileWithoutPath
    CHECK( xash::utilities::filename( "path/to/file.ext" ) == "file.ext" );
    CHECK( xash::utilities::filename( "file.ext" )         == "file.ext" );
    CHECK( xash::utilities::filename( "path/to/" )         == "" );
    CHECK( xash::utilities::filename( "" )                 == "" );
}

// ---------------------------------------------------------------------------
// file_base — strips directory + extension
// ---------------------------------------------------------------------------

static void test_file_base()
{
    // legacy: COM_FileBase (string overload)
    CHECK( xash::utilities::file_base( "path/to/file.bsp" ) == "file" );
    CHECK( xash::utilities::file_base( "file.bsp" )         == "file" );
    CHECK( xash::utilities::file_base( "noext" )            == "noext" );

    // char*/size overload.
    char buf[64]{};
    xash::utilities::file_base( "maps/de_dust2.bsp", buf, sizeof buf );
    CHECK( std::strcmp( buf, "de_dust2" ) == 0 );
}

// ---------------------------------------------------------------------------
// strip_extension
// ---------------------------------------------------------------------------

static void test_strip_extension()
{
    // string overload
    CHECK( xash::utilities::strip_extension( "file.bsp" )      == "file" );
    CHECK( xash::utilities::strip_extension( "file" )          == "file" );
    CHECK( xash::utilities::strip_extension( "path/to/f.bsp" ) == "path/to/f" );
    CHECK( xash::utilities::strip_extension( "file.ext.gz" )   == "file.ext" );  // only last ext

    // char* in-place overload.
    char path[32] = "model.mdl";
    xash::utilities::strip_extension( path );
    CHECK( std::strcmp( path, "model" ) == 0 );
}

// ---------------------------------------------------------------------------
// fix_slashes
// ---------------------------------------------------------------------------

static void test_fix_slashes()
{
    // string overload — legacy: COM_PathSlashFix
    CHECK( xash::utilities::fix_slashes( "a\\b\\c" )      == "a/b/c" );
    CHECK( xash::utilities::fix_slashes( "a/b/c" )        == "a/b/c" );
    CHECK( xash::utilities::fix_slashes( "no\\slashes\\" ) == "no/slashes/" );

    // char* in-place overload.
    char buf[32] = "dir\\sub\\file.ext";
    xash::utilities::fix_slashes( buf );
    CHECK( std::strcmp( buf, "dir/sub/file.ext" ) == 0 );
}

// ---------------------------------------------------------------------------
// extract_dir
// ---------------------------------------------------------------------------

static void test_extract_dir()
{
    // string overload — legacy: COM_ExtractFilePath
    CHECK( xash::utilities::extract_dir( "path/to/file.ext" ) == "path/to" );
    CHECK( xash::utilities::extract_dir( "file.ext" )         == "" );
    CHECK( xash::utilities::extract_dir( "" )                 == "" );
}

// ---------------------------------------------------------------------------
// default_extension
// ---------------------------------------------------------------------------

static void test_default_extension()
{
    // Only appends when no extension present — legacy: COM_DefaultExtension
    CHECK( xash::utilities::default_extension( "file",     ".bsp" ) == "file.bsp" );
    CHECK( xash::utilities::default_extension( "file.bsp", ".mdl" ) == "file.bsp" );  // already has ext
    CHECK( xash::utilities::default_extension( "file.bsp", ".bsp" ) == "file.bsp" );  // same ext
}

// ---------------------------------------------------------------------------
// replace_extension
// ---------------------------------------------------------------------------

static void test_replace_extension()
{
    // legacy: COM_ReplaceExtension (string overload)
    CHECK( xash::utilities::replace_extension( "file.bsp", ".mdl" ) == "file.mdl" );
    CHECK( xash::utilities::replace_extension( "file",     ".bsp" ) == "file.bsp" );
    CHECK( xash::utilities::replace_extension( "path/to/f.bsp", ".ent" ) == "path/to/f.ent" );
}

// ---------------------------------------------------------------------------
// remove_line_feed
// ---------------------------------------------------------------------------

static void test_remove_line_feed()
{
    // string overload — legacy: COM_RemoveLineFeed
    CHECK( xash::utilities::remove_line_feed( "hello\nworld" ) == "hello" );
    CHECK( xash::utilities::remove_line_feed( "hello\r\n" )    == "hello" );
    CHECK( xash::utilities::remove_line_feed( "hello" )        == "hello" );
    CHECK( xash::utilities::remove_line_feed( "\n" )           == "" );
    CHECK( xash::utilities::remove_line_feed( "" )             == "" );
}

// ---------------------------------------------------------------------------
// trim_space
// ---------------------------------------------------------------------------

static void test_trim_space()
{
    // string overload — legacy: COM_TrimSpace
    CHECK( xash::utilities::trim_space( "  hello  " ) == "hello" );
    CHECK( xash::utilities::trim_space( "hello" )     == "hello" );
    CHECK( xash::utilities::trim_space( "   " )       == "" );
    CHECK( xash::utilities::trim_space( "" )          == "" );
    CHECK( xash::utilities::trim_space( "a" )         == "a" );
}

int main()
{
    test_file_extension();
    test_filename();
    test_file_base();
    test_strip_extension();
    test_fix_slashes();
    test_extract_dir();
    test_default_extension();
    test_replace_extension();
    test_remove_line_feed();
    test_trim_space();

    std::printf( "path: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
