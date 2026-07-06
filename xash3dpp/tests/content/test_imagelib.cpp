// xash3dpp — imagelib tests
// Covers: init/shutdown lifecycle (idempotent). Codec/decode + WAD pack/unpack
// tests land with the O-2 implementation.

#include <xash3dpp/imagelib/imagelib.hpp>
#include <xash3dpp/imagelib/image.hpp>
#include <xash3dpp/imagelib/pixel_format.hpp>
#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    xash::imagelib::ImageDecoder dec;
    CHECK( dec.init() );
    CHECK_EQ( dec.stats().images_decoded, 0u );
    dec.shutdown();
    // Re-init must work (idempotent lifecycle).
    CHECK( dec.init() );
    dec.shutdown();
}

// ---------------------------------------------------------------------------
// Pixel-format helpers
// ---------------------------------------------------------------------------

static void test_pixel_format()
{
    using namespace xash::imagelib;

    CHECK( !is_compressed( PixelFormat::Rgba8 ) );
    CHECK( is_compressed( PixelFormat::Dxt1 ) );
    CHECK( is_compressed( PixelFormat::Bc7Srgb ) );
    CHECK( !is_compressed( PixelFormat::Ktx2Raw ) );

    CHECK_EQ( bytes_per_pixel( PixelFormat::Rgba8 ), std::size_t{ 4 } );
    CHECK_EQ( bytes_per_pixel( PixelFormat::Rgb8 ), std::size_t{ 3 } );
    CHECK_EQ( bytes_per_pixel( PixelFormat::Indexed8 ), std::size_t{ 1 } );
    CHECK_EQ( bytes_per_pixel( PixelFormat::Dxt1 ), std::size_t{ 0 } );

    CHECK_EQ( block_bytes( PixelFormat::Dxt1 ), std::size_t{ 8 } );
    CHECK_EQ( block_bytes( PixelFormat::Dxt5 ), std::size_t{ 16 } );

    // 16x16 RGBA = 1024 bytes; 16x16 DXT1 = 16 blocks * 8 = 128 bytes.
    CHECK_EQ( level_bytes( PixelFormat::Rgba8, 16, 16 ), std::size_t{ 1024 } );
    CHECK_EQ( level_bytes( PixelFormat::Dxt1, 16, 16 ), std::size_t{ 128 } );
    // Non-multiple-of-4 rounds up to whole blocks: 5x5 DXT1 = 2x2 blocks * 8.
    CHECK_EQ( level_bytes( PixelFormat::Dxt1, 5, 5 ), std::size_t{ 32 } );
}

static void test_image_flags()
{
    using namespace xash::imagelib;

    ImageFlags f = ImageFlags::HasAlpha | ImageFlags::HasColor;
    CHECK( has_flag( f, ImageFlags::HasAlpha ) );
    CHECK( has_flag( f, ImageFlags::HasColor ) );
    CHECK( !has_flag( f, ImageFlags::Cubemap ) );

    f |= ImageFlags::Cubemap;
    CHECK( has_flag( f, ImageFlags::Cubemap ) );
    f &= ~ImageFlags::HasAlpha;
    CHECK( !has_flag( f, ImageFlags::HasAlpha ) );
    CHECK( has_flag( f, ImageFlags::HasColor ) );
}

static void test_image_value()
{
    using namespace xash::imagelib;

    std::vector<std::byte> px( 4 * 2 * 2, std::byte{ 0x40 } );  // 2x2 RGBA
    Image img( 2, 2, PixelFormat::Rgba8, std::move( px ), ImageFlags::HasColor );

    CHECK_EQ( img.width(), std::uint16_t{ 2 } );
    CHECK_EQ( img.height(), std::uint16_t{ 2 } );
    CHECK_EQ( img.depth(), std::uint16_t{ 1 } );
    CHECK( img.format() == PixelFormat::Rgba8 );
    CHECK( img.has( ImageFlags::HasColor ) );
    CHECK( !img.empty() );
    CHECK_EQ( img.pixels().size(), std::size_t{ 16 } );
    CHECK( !img.palette().has_value() );

    Palette pal;
    pal[1] = Rgba{ 10, 20, 30, 255 };
    img.set_palette( pal );
    CHECK( img.palette().has_value() );
    CHECK( img.palette()->operator[]( 1 ) == ( Rgba{ 10, 20, 30, 255 } ) );
}

// ---------------------------------------------------------------------------
// WAD3 pack/unpack round-trip (chunk deliverable)
// ---------------------------------------------------------------------------

static void test_wad_roundtrip()
{
    using namespace xash::imagelib;

    // A 16x16 indexed image: pixel i carries index i, one of every index.
    Palette pal;
    for( int i = 0; i < 256; ++i )
        pal[static_cast<std::size_t>( i )] = Rgba{
            static_cast<std::uint8_t>( i ),
            static_cast<std::uint8_t>( 255 - i ),
            static_cast<std::uint8_t>( i / 2 ),
            255 };

    std::vector<std::byte> idx( 16 * 16 );
    for( int i = 0; i < 256; ++i )
        idx[static_cast<std::size_t>( i )] = std::byte{ static_cast<std::uint8_t>( i ) };

    Image src( 16, 16, PixelFormat::Indexed8, std::move( idx ) );
    src.set_palette( pal );

    // pack -> WAD3 bytes
    const auto packed = save_wad( src );
    REQUIRE( packed.has_value() );
    CHECK( !packed->empty() );

    // unpack via the decoder registry (dispatch on ".wad")
    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "logo.WAD", *packed );  // case-insensitive ext
    REQUIRE( out.has_value() );

    CHECK_EQ( out->width(), std::uint16_t{ 16 } );
    CHECK_EQ( out->height(), std::uint16_t{ 16 } );
    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( out->has( ImageFlags::HasAlpha ) );

    // Each pixel expands to its palette RGB; index 255 is transparent (classic).
    const auto px = out->pixels();
    REQUIRE( px.size() == 16 * 16 * 4 );
    bool all_ok = true;
    for( int i = 0; i < 256; ++i )
    {
        const Rgba e = pal[static_cast<std::size_t>( i )];
        const std::uint8_t ea = ( i == 255 ) ? 0 : 255;
        all_ok = all_ok
            && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == e.r
            && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == e.g
            && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == e.b
            && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == ea;
    }
    CHECK( all_ok );
    CHECK_EQ( dec.stats().images_decoded, std::uint64_t{ 1 } );
    dec.shutdown();

    // An unknown extension must not resolve to a codec.
    ImageDecoder dec2;
    REQUIRE( dec2.init() );
    const auto bad = dec2.decode( "x.xyz", *packed );
    CHECK( !bad.has_value() );
    CHECK( bad.error() == ImageError::UnknownFormat );
    dec2.shutdown();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // The lifecycle entry points assert ThreadRole::Main; register it first.
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_init_shutdown );
    RUN_TEST( test_pixel_format );
    RUN_TEST( test_image_flags );
    RUN_TEST( test_image_value );
    RUN_TEST( test_wad_roundtrip );

    std::printf( "test_imagelib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
