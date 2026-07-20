// xash3dpp — x64/x86 byte-identity harness (Chunk 8, slice S8.8).
//
// The on-disk `.sav`/`.HL1-3` format is offset/token-based with no raw
// pointers (save-boundary.md "Native-endian, little-endian only" + the
// format.hpp header comment: on-disk field offsets are never stored, only
// computed at build time for in-memory TYPEDESCRIPTION tables) — so the
// SAME logical save must be byte-identical whether it was produced by the
// x64 or the x86 build. An in-process cross-arch comparison isn't possible
// in one test binary (a single ctest executable is one arch), so this is
// implemented as a single-arch test with a fixed, hex-pinned golden: a
// canonical fixture save (fixed inputs, see below) is assembled via the
// SAME production code (LevelStateWriter + write_sav_container) that both
// the x64 and x86 test suites build and link, and the result is compared
// against a golden pinned as source text that compiles IDENTICALLY into
// both arch builds. If this test passes on BOTH `test.py --json` (x64) and
// `test.py --json --arch x86`, byte-identity is proven transitively: two
// independently-built binaries produced the exact same bytes for the exact
// same fixed inputs.
//
// Golden derivation (ONCE, 2026-07-20): the two constants below are not
// hand-typed — they were captured by running this test's own construction
// code (the "Canonical fixture" section) in a scratch instrumented pass
// (stderr markers after each step, since a std::printf-buffered scratch
// build loses its output if the harness aborts before the normal exit
// flush), reading off the printed byte length and the xash::utilities::
// Md5Hasher hex digest of the assembled image, then pinning both constants
// here. Md5Hasher is the project's existing hash utility (utilities/
// hash.hpp, already linked transitively via xash3dpp_save ->
// xash3dpp_utilities) — used purely as a compact drift-detector for this
// cross-arch byte-pin, not for any security property. No existing golden
// in this repo already covers the FULL container path with a non-empty
// embedded `.HL1` record (map_count == 1): test_sav_container.cpp's
// GAME_HEADER golden pins the 0-embedded-file case only (mapCount
// DataEmpty-skipped, so `mapCount`/`t0.HL1` never appear in that image) —
// this is a new golden, not a duplicate. To re-derive after an intentional
// format change: temporarily replace the two CHECK_EQ/CHECK assertions
// below with std::fprintf(stderr, ...) of `image.size()` and the hex
// digest (stderr is unbuffered, so it survives an assertion abort — see
// above), rebuild, run once, and re-pin.
//
// Pool-lifetime note: `g_pool` is a file-scope static, created/destroyed in
// main() around RUN_TEST — matching every sibling save test (test_sav_
// container.cpp, test_hl1_writer.cpp, ...). This matters here specifically:
// the SaveBuffer `unique_ptr`s below route their deallocation back through
// the pool (Q-22 pool-aware `operator delete`), so destroying the pool
// before those locals go out of scope is a use-after-free the memory
// subsystem asserts on. Scoping the pool to the whole test run rather than
// to one test function sidesteps the ordering hazard entirely.

#include <xash3dpp/private/save/container_codec.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_writer.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/hash.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;
namespace util = xash::utilities;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

// Renders a 16-byte MD5 digest as a 32-char lowercase hex string.
static std::string hex_digest( const std::array<std::uint8_t, 16> &d )
{
    static const char *hex = "0123456789abcdef";
    std::string        out( 32, '0' );
    for ( std::size_t i = 0; i < 16; ++i )
    {
        out[2 * i]     = hex[( d[i] >> 4 ) & 0xF];
        out[2 * i + 1] = hex[d[i] & 0xF];
    }
    return out;
}

// The pfnSave stand-in used by the S8.3 empty-image golden (test_hl1_writer
// .cpp's test_empty_full_image / test_hl1_loader.cpp's test_empty_golden_
// writer_roundtrip): one field record "x" = AABBCCDD per entity.
namespace
{
struct XSaver final : save::IEntitySaver
{
    save::Result<void> save_entity( std::size_t, const abi::edict_t *, save::IFieldSink &sink,
                                    save::TokenTable &tokens ) noexcept override
    {
        auto tok = tokens.insert( "x" );
        if ( !tok )
            return std::unexpected( tok.error() );
        const std::array<std::byte, 4> payload = { b( 0xAA ), b( 0xBB ), b( 0xCC ), b( 0xDD ) };
        return sink.write_field_record( *tok, payload );
    }
};
} // namespace

// ---------------------------------------------------------------------------
// Canonical fixture: the S8.3 minimal level (194-byte empty-image golden —
// skill=0, time=1.0f, mapName="t0", 0 connections, 0 lightstyles, 1 valid
// non-client entity classname "a", field "x"=AABBCCDD) embedded as `t0.HL1`
// inside a `.sav` container with a fixed GAME_HEADER (map_name="t0",
// comment="byte-identity", 1 embedded file).
// ---------------------------------------------------------------------------
static void test_canonical_save_byte_identity()
{
    // --- Step 1: the S8.3 minimal `.HL1` level image (LevelStateWriter). ---
    abi::edict_t e0{}; // free==0 -> valid; no FL_CLIENT
    std::array<abi::edict_t *, 1>   edicts  = { &e0 };
    std::array<std::string_view, 1> classes = { "a" };
    XSaver                          saver;

    save::LevelStateParams lp;
    lp.skill_level = 0;
    lp.time        = 1.0f;
    lp.map_name    = "t0";
    lp.edicts      = edicts;
    lp.classnames  = classes;
    lp.saver       = &saver;

    auto lwbuf = save::create_save_buffer( g_pool, 1024, 11, 0.0f );
    REQUIRE( lwbuf != nullptr );
    save::EntityTable      ltable;
    save::LevelStateWriter lwriter( *lwbuf, ltable );
    std::vector<std::byte> hl1_image;
    REQUIRE( lwriter.write( lp, hl1_image ).has_value() );
    REQUIRE( hl1_image.size() == 194u ); // the S8.3 empty golden (independently pinned elsewhere)

    // --- Step 2: embed it in a `.sav` container with a fixed GAME_HEADER. ---
    save::SavContainerParams cp;
    cp.map_name = "t0";
    cp.comment  = "byte-identity";

    std::array<save::EmbeddedFile, 1> files = { {
        { "t0.HL1", hl1_image },
    } };

    auto cwbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( cwbuf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( cp, /*global_state=*/nullptr, files, *cwbuf, image ).has_value() );

    // --- Step 3: pin the length + MD5 hex digest (derived once, see header). ---
    constexpr std::size_t      k_golden_size = 678;
    constexpr std::string_view k_golden_md5  = "a60d0052a8b8a8f2b40930870d59450e";

    CHECK_EQ( image.size(), k_golden_size );

    const auto        digest = util::Md5Hasher::hash( image.data(), image.size() );
    const std::string hex    = hex_digest( digest );
    if ( hex != k_golden_md5 )
        std::printf( "  [byte-identity] MISMATCH: computed size=%zu md5=%s\n", image.size(), hex.c_str() );
    CHECK( hex == std::string_view( k_golden_md5 ) );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "byte_identity_test" );

    RUN_TEST( test_canonical_save_byte_identity );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "byte_identity: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
