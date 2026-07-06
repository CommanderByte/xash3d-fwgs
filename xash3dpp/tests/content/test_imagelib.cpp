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
// TGA decode + save (chunk deliverable) — small in-memory buffers.
// ---------------------------------------------------------------------------

static void tga_put_u8( std::vector<std::byte> &v, std::uint8_t x )
{
    v.push_back( std::byte{ x } );
}

static void tga_put_u16le( std::vector<std::byte> &v, std::uint16_t x )
{
    v.push_back( std::byte{ static_cast<std::uint8_t>( x & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( x >> 8 ) } );
}

// Append the 18-byte tga_t header.
static void tga_header( std::vector<std::byte> &v, std::uint8_t image_type,
                        std::uint16_t w, std::uint16_t h, std::uint8_t pixel_size,
                        std::uint8_t attributes, std::uint8_t colormap_size = 0,
                        std::uint16_t colormap_length = 0 )
{
    tga_put_u8( v, 0 );                                     // id_length
    tga_put_u8( v, colormap_size != 0 ? 1 : 0 );            // colormap_type
    tga_put_u8( v, image_type );
    tga_put_u16le( v, 0 );                                  // colormap_index
    tga_put_u16le( v, colormap_length );
    tga_put_u8( v, colormap_size );
    tga_put_u16le( v, 0 );                                  // x_origin
    tga_put_u16le( v, 0 );                                  // y_origin
    tga_put_u16le( v, w );
    tga_put_u16le( v, h );
    tga_put_u8( v, pixel_size );
    tga_put_u8( v, attributes );
}

// 2x2 uncompressed 24-bit truecolour, exercising both orientations. Disk stores
// pixels B,G,R in raster order; attributes bit 0x20 flips top/bottom.
static void test_tga_uncompressed()
{
    using namespace xash::imagelib;

    const std::uint8_t rgb[4][3] = {
        { 10, 20, 30 }, { 40, 50, 60 }, { 70, 80, 90 }, { 100, 110, 120 } };

    auto build = [&]( std::uint8_t attributes ) {
        std::vector<std::byte> v;
        tga_header( v, 2, 2, 2, 24, attributes );
        for( const auto &c : rgb )  // BGR on disk
        {
            tga_put_u8( v, c[2] ); tga_put_u8( v, c[1] ); tga_put_u8( v, c[0] );
        }
        return v;
    };

    ImageDecoder dec;
    REQUIRE( dec.init() );

    // attributes 0x20 => stored top-to-bottom => output raster == disk raster.
    {
        const auto tga = build( 0x20 );
        const auto img = dec.decode( "a.tga", tga );
        REQUIRE( img.has_value() );
        CHECK_EQ( img->width(), std::uint16_t{ 2 } );
        CHECK_EQ( img->height(), std::uint16_t{ 2 } );
        CHECK( img->format() == PixelFormat::Rgba8 );
        CHECK( img->has( ImageFlags::HasColor ) );
        CHECK( !img->has( ImageFlags::HasAlpha ) );  // 24-bit => alpha forced 255
        const auto px = img->pixels();
        REQUIRE( px.size() == 2 * 2 * 4 );
        bool ok = true;
        for( int i = 0; i < 4; ++i )
            ok = ok
              && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == rgb[i][0]
              && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == rgb[i][1]
              && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == rgb[i][2]
              && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == 255;
        CHECK( ok );
    }

    // attributes 0x00 => stored bottom-to-top => output rows swap (2x2 flip).
    {
        const auto tga = build( 0x00 );
        const auto img = dec.decode( "b.tga", tga );
        REQUIRE( img.has_value() );
        const auto px = img->pixels();
        REQUIRE( px.size() == 2 * 2 * 4 );
        const int map[4] = { 2, 3, 0, 1 };  // out row0 = disk row1, out row1 = disk row0
        bool ok = true;
        for( int i = 0; i < 4; ++i )
        {
            const int s = map[i];
            ok = ok
              && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == rgb[s][0]
              && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == rgb[s][1]
              && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == rgb[s][2];
        }
        CHECK( ok );
    }

    dec.shutdown();
}

// 2x2 RLE (type 10) 32-bit: a run packet (2 identical) + a raw packet (2 fresh).
static void test_tga_rle()
{
    using namespace xash::imagelib;

    std::vector<std::byte> v;
    tga_header( v, 10, 2, 2, 32, 0x20 );  // RLE truecolour, top-to-bottom
    // run packet: 2x A=(R200,G100,B50,A255) — control 0x80|1, then one BGRA texel
    tga_put_u8( v, 0x81 );
    tga_put_u8( v, 50 ); tga_put_u8( v, 100 ); tga_put_u8( v, 200 ); tga_put_u8( v, 255 );
    // raw packet: B=(1,2,3,a4), C=(5,6,7,a255) — control 0x00|1, then two BGRA texels
    tga_put_u8( v, 0x01 );
    tga_put_u8( v, 3 ); tga_put_u8( v, 2 ); tga_put_u8( v, 1 ); tga_put_u8( v, 4 );
    tga_put_u8( v, 7 ); tga_put_u8( v, 6 ); tga_put_u8( v, 5 ); tga_put_u8( v, 255 );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto img = dec.decode( "c.tga", v );
    REQUIRE( img.has_value() );
    CHECK_EQ( img->width(), std::uint16_t{ 2 } );
    CHECK_EQ( img->height(), std::uint16_t{ 2 } );
    CHECK( img->has( ImageFlags::HasColor ) );
    CHECK( img->has( ImageFlags::HasAlpha ) );  // B carries alpha 4

    const std::uint8_t expect[4][4] = {
        { 200, 100, 50, 255 }, { 200, 100, 50, 255 },
        { 1, 2, 3, 4 }, { 5, 6, 7, 255 } };
    const auto px = img->pixels();
    REQUIRE( px.size() == 2 * 2 * 4 );
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        for( int c = 0; c < 4; ++c )
            ok = ok && std::to_integer<std::uint8_t>( px[i * 4 + c] ) == expect[i][c];
    CHECK( ok );
    dec.shutdown();
}

// 2x2 colormapped (type 1) 8-bit indices into a 256-entry 24-bit palette.
static void test_tga_colormapped()
{
    using namespace xash::imagelib;

    const std::uint8_t pal[4][3] = {
        { 10, 20, 30 }, { 40, 50, 60 }, { 70, 80, 90 }, { 100, 110, 120 } };

    std::vector<std::byte> v;
    tga_header( v, 1, 2, 2, 8, 0x20, /*colormap_size*/ 24, /*colormap_length*/ 256 );
    for( int i = 0; i < 256; ++i )  // palette stored B,G,R; only first four used
    {
        if( i < 4 ) { tga_put_u8( v, pal[i][2] ); tga_put_u8( v, pal[i][1] ); tga_put_u8( v, pal[i][0] ); }
        else        { tga_put_u8( v, 0 ); tga_put_u8( v, 0 ); tga_put_u8( v, 0 ); }
    }
    tga_put_u8( v, 0 ); tga_put_u8( v, 1 ); tga_put_u8( v, 2 ); tga_put_u8( v, 3 );  // indices

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto img = dec.decode( "d.tga", v );
    REQUIRE( img.has_value() );
    CHECK( img->has( ImageFlags::HasColor ) );
    CHECK( !img->has( ImageFlags::HasAlpha ) );  // 24-bit colormap => alpha 255
    const auto px = img->pixels();
    REQUIRE( px.size() == 2 * 2 * 4 );
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        ok = ok
          && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == pal[i][0]
          && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == pal[i][1]
          && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == pal[i][2]
          && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == 255;
    CHECK( ok );
    dec.shutdown();
}

// Bounds-safety: malformed inputs must return an error, never fault.
static void test_tga_bad_input()
{
    using namespace xash::imagelib;

    ImageDecoder dec;
    REQUIRE( dec.init() );

    // Shorter than the 18-byte header.
    {
        std::vector<std::byte> v( 5, std::byte{ 0 } );
        const auto img = dec.decode( "t.tga", v );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::Truncated );
    }
    // Valid header claiming 4x4 24-bit, but no pixel body at all.
    {
        std::vector<std::byte> v;
        tga_header( v, 2, 4, 4, 24, 0x20 );
        const auto img = dec.decode( "t.tga", v );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::Truncated );
    }
    // Zero width => bad header.
    {
        std::vector<std::byte> v;
        tga_header( v, 2, 0, 4, 24, 0x20 );
        const auto img = dec.decode( "t.tga", v );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::BadHeader );
    }
    dec.shutdown();
}

// save_tga -> decode round-trip: the save flip + BGRA and the decode flip must
// compose to identity for an RGBA image with alpha.
static void test_tga_save_roundtrip()
{
    using namespace xash::imagelib;

    const std::uint8_t rgba[4][4] = {
        { 10, 20, 30, 255 }, { 40, 50, 60, 128 },
        { 70, 80, 90, 200 }, { 100, 110, 120, 255 } };
    std::vector<std::byte> srcpx( 2 * 2 * 4 );
    for( int i = 0; i < 4; ++i )
        for( int c = 0; c < 4; ++c )
            srcpx[static_cast<std::size_t>( i * 4 + c )] = std::byte{ rgba[i][c] };

    Image src( 2, 2, PixelFormat::Rgba8, std::move( srcpx ),
               ImageFlags::HasColor | ImageFlags::HasAlpha );

    const auto saved = save_tga( src );
    REQUIRE( saved.has_value() );
    CHECK( !saved->empty() );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "rt.tga", *saved );
    REQUIRE( out.has_value() );
    CHECK_EQ( out->width(), std::uint16_t{ 2 } );
    CHECK_EQ( out->height(), std::uint16_t{ 2 } );
    CHECK( out->has( ImageFlags::HasAlpha ) );
    const auto px = out->pixels();
    REQUIRE( px.size() == 2 * 2 * 4 );
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        for( int c = 0; c < 4; ++c )
            ok = ok && std::to_integer<std::uint8_t>( px[i * 4 + c] ) == rgba[i][c];
    CHECK( ok );
    dec.shutdown();
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
    RUN_TEST( test_tga_uncompressed );
    RUN_TEST( test_tga_rle );
    RUN_TEST( test_tga_colormapped );
    RUN_TEST( test_tga_bad_input );
    RUN_TEST( test_tga_save_roundtrip );

    std::printf( "test_imagelib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
