// xash3dpp — imagelib tests
// Covers: init/shutdown lifecycle (idempotent). Codec/decode + WAD pack/unpack
// tests land with the O-2 implementation.

#include <xash3dpp/imagelib/imagelib.hpp>
#include <xash3dpp/imagelib/image.hpp>
#include <xash3dpp/imagelib/pixel_format.hpp>
#include <xash3dpp/core/thread_role.hpp>

#include <cstddef>
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

    std::printf( "test_imagelib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
