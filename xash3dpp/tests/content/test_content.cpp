// xash3dpp — content (model cache) tests
// Covers: init/shutdown lifecycle (idempotent). Model lookup, format dispatch,
// and studio header parse tests land with the O-1 implementation.

#include <xash3dpp/content/content.hpp>
#include <xash3dpp/content/model.hpp>
#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/content/formats.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    // Not activated — ModelCache only borrows the reference (P-3 context).
    xash::filesystem::Filesystem fs;
    xash::content::ModelCache cache;
    CHECK( cache.init( { fs } ) );
    CHECK_EQ( cache.stats().models_loaded, 0u );
    cache.shutdown();
    // Re-init must work (idempotent lifecycle).
    CHECK( cache.init( { fs } ) );
    cache.shutdown();
}

// ---------------------------------------------------------------------------
// Model registry — typed handle lookup (chunk deliverable)
// ---------------------------------------------------------------------------

static void test_model_registry()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    // World is pinned to slot 0.
    const ModelHandle world = cache.register_world( "maps/c0a0.bsp" );
    CHECK( world.valid() );
    CHECK_EQ( world.index, std::uint16_t{ 0 } );
    REQUIRE( cache.resolve( world ) != nullptr );
    CHECK( cache.resolve( world )->type() == ModelType::Brush );

    // find_or_alloc never returns slot 0 and is idempotent by name.
    const ModelHandle a = cache.find_or_alloc( "models/player.mdl" );
    CHECK( a.valid() );
    CHECK( a.index != 0 );
    const ModelHandle a2 = cache.find_or_alloc( "models/player.mdl" );
    CHECK( a == a2 );                          // same slot on re-request
    CHECK_EQ( cache.stats().cache_hits, std::uint64_t{ 1 } );

    // Distinct names get distinct slots.
    const ModelHandle b = cache.find_or_alloc( "sprites/explode.spr" );
    CHECK( b.valid() );
    CHECK( b.index != a.index );

    // find() locates a registered model, or the null handle if absent.
    CHECK( cache.find( "models/player.mdl" ) == a );
    CHECK( !cache.find( "does/not/exist.mdl" ).valid() );

    // resolve exposes the model; name round-trips.
    const Model *m = cache.resolve( a );
    REQUIRE( m != nullptr );
    CHECK( m->name() == "models/player.mdl" );
    CHECK( m->needload() == NeedLoad::NeedsLoaded );

    CHECK_EQ( cache.live_count(), std::size_t{ 3 } );  // world + a + b

    // Freeing invalidates the handle (generation bump); the slot is reusable.
    cache.free_model( a );
    CHECK( cache.resolve( a ) == nullptr );            // stale handle fails
    CHECK_EQ( cache.live_count(), std::size_t{ 2 } );

    const ModelHandle c = cache.find_or_alloc( "models/gauss.mdl" );
    CHECK( c.valid() );
    CHECK_EQ( c.index, a.index );                      // reused the freed slot
    CHECK( c.generation != a.generation );             // but a fresh generation
    CHECK( cache.resolve( a ) == nullptr );            // old handle still stale
    REQUIRE( cache.resolve( c ) != nullptr );
    CHECK( cache.resolve( c )->name() == "models/gauss.mdl" );

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// Studio header parse (chunk deliverable)
// ---------------------------------------------------------------------------

namespace {

// Build a minimal valid studiohdr byte image with the given field values.
std::vector<std::byte> make_studio_header()
{
    std::vector<std::byte> b( 300, std::byte{ 0 } );
    auto put32 = [&]( std::size_t off, std::int32_t v ) {
        xash::utilities::write_le<std::int32_t>( b.data() + off, v );
    };
    auto putf = [&]( std::size_t off, float v ) {
        xash::utilities::write_le<std::uint32_t>( b.data() + off, std::bit_cast<std::uint32_t>( v ) );
    };

    put32( 0, xash::content::k_studio_ident );      // ident "IDST"
    put32( 4, xash::content::k_studio_version );     // version 10
    std::memcpy( b.data() + 8, "player.mdl", 10 );   // name[64]
    put32( 72, 300 );                                // length
    putf( 76, 1.0f ); putf( 80, 2.0f ); putf( 84, 64.0f );   // eyeposition
    putf( 112, -16.0f ); putf( 116, -16.0f ); putf( 120, 0.0f ); // bbmin (clip_min)
    putf( 124, 16.0f ); putf( 128, 16.0f ); putf( 132, 72.0f );  // bbmax (clip_max)
    put32( 136, 0x0001 );                            // flags
    put32( 140, 30 );                                // numbones
    put32( 144, 244 );                               // boneindex
    put32( 156, 12 );                                // numhitboxes
    put32( 160, 250 );                               // hitboxindex
    put32( 164, 8 );                                 // numseq
    put32( 212, 3 );                                 // numattachments
    return b;
}

} // namespace

static void test_studio_parse()
{
    using namespace xash::content;

    const std::vector<std::byte> buf = make_studio_header();

    const auto parsed = parse_studio( buf );
    REQUIRE( parsed.has_value() );

    const StudioView v = parsed->view();
    REQUIRE( v.valid() );
    CHECK( v.name() == "player.mdl" );
    CHECK_EQ( v.length(), 300 );
    CHECK_EQ( v.flags(), 0x0001 );
    CHECK_EQ( v.num_bones(), 30 );
    CHECK_EQ( v.bone_index(), 244 );
    CHECK_EQ( v.num_hitboxes(), 12 );
    CHECK_EQ( v.hitbox_index(), 250 );
    CHECK_EQ( v.num_seq(), 8 );
    CHECK_EQ( v.num_attachments(), 3 );
    CHECK( v.eye_position().z == 64.0f );
    CHECK( v.clip_min().x == -16.0f );
    CHECK( v.clip_max().z == 72.0f );

    // Error paths.
    std::vector<std::byte> bad_magic = buf;
    xash::utilities::write_le<std::int32_t>( bad_magic.data(), 0x12345678 );
    CHECK( !parse_studio( bad_magic ).has_value() );
    CHECK( parse_studio( bad_magic ).error() == LoadError::BadMagic );

    std::vector<std::byte> bad_ver = buf;
    xash::utilities::write_le<std::int32_t>( bad_ver.data() + 4, 9 );
    CHECK( parse_studio( bad_ver ).error() == LoadError::BadVersion );

    const std::vector<std::byte> tiny( 100, std::byte{ 0 } );
    CHECK( parse_studio( tiny ).error() == LoadError::Truncated );

    // Out-of-range field reads on a header-only image return 0, not a fault.
    const StudioView empty;
    CHECK_EQ( empty.num_bones(), 0 );
    CHECK( empty.name().empty() );
}

// ---------------------------------------------------------------------------
// Load path — magic dispatch through the cache (O-3)
// ---------------------------------------------------------------------------

static void test_model_load()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    // A studio model loads and attaches its payload.
    const ModelHandle h = cache.find_or_alloc( "models/player.mdl" );
    const std::vector<std::byte> studio = make_studio_header();
    REQUIRE( cache.load_from_bytes( h, studio ).has_value() );

    const Model *m = cache.resolve( h );
    REQUIRE( m != nullptr );
    CHECK( m->type() == ModelType::Studio );
    CHECK( m->needload() == NeedLoad::Present );
    REQUIRE( m->studio() != nullptr );
    CHECK_EQ( m->studio()->view().num_bones(), 30 );
    CHECK_EQ( cache.stats().models_loaded, std::uint64_t{ 1 } );

    // A sprite (IDSP v2 / Half-Life) loads and normalises its header.
    std::vector<std::byte> spr( 40, std::byte{ 0 } );
    xash::utilities::write_le<std::int32_t>( spr.data() + 0, k_sprite_ident );
    xash::utilities::write_le<std::int32_t>( spr.data() + 4, 2 );    // version
    xash::utilities::write_le<std::int32_t>( spr.data() + 8, 1 );    // type
    xash::utilities::write_le<std::int32_t>( spr.data() + 12, 2 );   // texFormat
    xash::utilities::write_le<std::int32_t>( spr.data() + 20, 64 );  // bounds[0]
    xash::utilities::write_le<std::int32_t>( spr.data() + 24, 48 );  // bounds[1]
    xash::utilities::write_le<std::int32_t>( spr.data() + 28, 5 );   // numframes
    const ModelHandle sh = cache.find_or_alloc( "sprites/explode.spr" );
    REQUIRE( cache.load_from_bytes( sh, spr ).has_value() );
    const Model *sm = cache.resolve( sh );
    REQUIRE( sm != nullptr );
    CHECK( sm->type() == ModelType::Sprite );
    REQUIRE( sm->sprite() != nullptr );
    CHECK_EQ( sm->sprite()->info().num_frames, 5 );
    CHECK_EQ( sm->sprite()->info().max_width, 64 );
    CHECK_EQ( sm->sprite()->info().tex_format, 2 );

    // An alias (IDPO v6 / Quake MDL) loads (parse-minimal).
    std::vector<std::byte> ali( 12, std::byte{ 0 } );
    xash::utilities::write_le<std::int32_t>( ali.data() + 0, k_alias_ident );
    xash::utilities::write_le<std::int32_t>( ali.data() + 4, 6 );
    const ModelHandle ah = cache.find_or_alloc( "models/quake.mdl" );
    REQUIRE( cache.load_from_bytes( ah, ali ).has_value() );
    REQUIRE( cache.resolve( ah ) != nullptr );
    CHECK( cache.resolve( ah )->type() == ModelType::Alias );
    CHECK( cache.resolve( ah )->alias() != nullptr );

    CHECK_EQ( cache.stats().models_loaded, std::uint64_t{ 3 } );  // studio + sprite + alias

    // Unknown magic is rejected.
    const ModelHandle j = cache.find_or_alloc( "junk.dat" );
    const std::vector<std::byte> junk( 8, std::byte{ 0 } );
    const auto bad = cache.load_from_bytes( j, junk );
    CHECK( !bad.has_value() );
    CHECK( bad.error() == LoadError::BadMagic );

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// CRC cheat-detection surface (OQ-6)
// ---------------------------------------------------------------------------

static void test_model_crc()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    // Flag a model checksum-required, then load it.
    cache.need_crc( "models/player.mdl", true );
    const ModelHandle h = cache.find_or_alloc( "models/player.mdl" );
    std::vector<std::byte> a = make_studio_header();
    REQUIRE( cache.load_from_bytes( h, a ).has_value() );

    const std::uint32_t crc_a = xash::utilities::crc32( a.data(), a.size() );
    CHECK( cache.validate_crc( "models/player.mdl", crc_a ) );
    CHECK( !cache.validate_crc( "models/player.mdl", crc_a ^ 0x1u ) );
    CHECK( !cache.validate_crc( "unknown.mdl", crc_a ) );

    // A reload with changed bytes on a checksum-required model is rejected.
    std::vector<std::byte> b = make_studio_header();
    xash::utilities::write_le<std::int32_t>( b.data() + 140, 31 );  // numbones 30 -> 31
    const auto reload = cache.load_from_bytes( h, b );
    CHECK( !reload.has_value() );
    CHECK( reload.error() == LoadError::CrcMismatch );

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// P-4 typed introspection snapshot
// ---------------------------------------------------------------------------

static void test_model_introspection()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    CHECK( cache.register_world( "maps/c0a0.bsp" ).valid() );
    const ModelHandle h = cache.find_or_alloc( "models/player.mdl" );
    REQUIRE( cache.load_from_bytes( h, make_studio_header() ).has_value() );

    const auto infos = cache.model_infos();
    CHECK_EQ( infos.size(), std::size_t{ 2 } );   // world + player

    bool found_player = false;
    for ( const auto &mi : infos )
    {
        if ( mi.name == "models/player.mdl" )
        {
            found_player = true;
            CHECK( mi.type == ModelType::Studio );
            CHECK( mi.needload == NeedLoad::Present );
            CHECK( mi.crc != 0u );
        }
    }
    CHECK( found_player );

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// IModelPostProcess seam (OQ-4)
// ---------------------------------------------------------------------------

namespace {

struct RecordingPost final : xash::content::IModelPostProcess
{
    int  calls  = 0;
    bool result = true;

    bool on_model_loaded( xash::content::ModelHandle, xash::content::Model&,
                          std::span<const std::byte> ) noexcept override
    {
        ++calls;
        return result;
    }
};

} // namespace

static void test_model_postprocess()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    RecordingPost post;
    ModelCache cache;
    const InitParams params{ fs, &post };
    REQUIRE( cache.init( params ) );

    // The hook fires on a successful load.
    const ModelHandle h = cache.find_or_alloc( "models/player.mdl" );
    REQUIRE( cache.load_from_bytes( h, make_studio_header() ).has_value() );
    CHECK_EQ( post.calls, 1 );
    CHECK( cache.resolve( h ) != nullptr );

    // A rejecting hook fails the load and frees the model.
    post.result = false;
    const ModelHandle h2 = cache.find_or_alloc( "models/gauss.mdl" );
    const auto r = cache.load_from_bytes( h2, make_studio_header() );
    CHECK( !r.has_value() );
    CHECK( r.error() == LoadError::UnsupportedFeature );
    CHECK( cache.resolve( h2 ) == nullptr );   // freed on rejection
    CHECK_EQ( post.calls, 2 );

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// Level-transition purge FSM (OQ-8)
// ---------------------------------------------------------------------------

static void test_model_purge()
{
    using namespace xash::content;

    xash::filesystem::Filesystem fs;
    ModelCache cache;
    REQUIRE( cache.init( { fs } ) );

    CHECK( cache.register_world( "maps/c0a0.bsp" ).valid() );
    const ModelHandle a = cache.find_or_alloc( "models/a.mdl" );
    const ModelHandle b = cache.find_or_alloc( "models/b.mdl" );
    CHECK_EQ( cache.live_count(), std::size_t{ 3 } );  // world + a + b

    // Level change: flag every non-world model for reaping.
    cache.purge_for_level_change();

    // The new level re-references 'a' (rescues it) but never touches 'b'.
    const ModelHandle a2 = cache.find_or_alloc( "models/a.mdl" );
    CHECK( a2 == a );  // same slot, rescued from the purge

    cache.free_unused();

    CHECK( cache.resolve( a ) != nullptr );            // rescued, survives
    CHECK( cache.resolve( b ) == nullptr );            // reaped
    CHECK_EQ( cache.live_count(), std::size_t{ 2 } );  // world + a

    cache.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // The lifecycle entry points assert ThreadRole::Main; register it first.
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_model_registry );
    RUN_TEST( test_studio_parse );
    RUN_TEST( test_model_load );
    RUN_TEST( test_model_crc );
    RUN_TEST( test_model_introspection );
    RUN_TEST( test_model_postprocess );
    RUN_TEST( test_model_purge );

    std::printf( "test_content: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
