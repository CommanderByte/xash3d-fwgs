// xash3dpp — imagelib tests
// Covers: init/shutdown lifecycle (idempotent). Codec/decode + WAD pack/unpack
// tests land with the O-2 implementation.

#include <xash3dpp/imagelib/imagelib.hpp>
#include <xash3dpp/imagelib/image.hpp>
#include <xash3dpp/imagelib/pixel_format.hpp>
#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/private/imagelib/palette.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "../test_helpers.hpp"

#include <miniz.h>   // build valid PNG IDAT (deflate) + CRCs for the codec tests

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
// BMP decode + save (chunk deliverable) — small hand-built in-memory bitmaps
// ---------------------------------------------------------------------------

static void bmp_put_u16( std::vector<std::byte> &v, std::uint16_t x )
{
    v.push_back( std::byte{ static_cast<std::uint8_t>( x & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 8 ) & 0xFF ) } );
}
static void bmp_put_u32( std::vector<std::byte> &v, std::uint32_t x )
{
    for( int i = 0; i < 4; ++i )
        v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> ( 8 * i ) ) & 0xFF ) } );
}
static void bmp_put_bytes( std::vector<std::byte> &v, std::initializer_list<std::uint8_t> bs )
{
    for( const std::uint8_t b : bs )
        v.push_back( std::byte{ b } );
}

// A 54-byte BITMAPFILEHEADER + BITMAPINFOHEADER. `file_size` is written verbatim
// into bfSize so callers can exercise the Sweet Half-Life mismatch tolerance.
static std::vector<std::byte> bmp_header( std::int32_t w, std::int32_t h, std::uint16_t bpp,
                                          std::uint32_t colors, std::uint32_t file_size )
{
    std::vector<std::byte> v;
    bmp_put_bytes( v, { 'B', 'M' } );
    bmp_put_u32( v, file_size );                         // bfSize
    bmp_put_u32( v, 0 );                                 // reserved
    bmp_put_u32( v, 54 );                                // bfOffBits (decoder ignores it)
    bmp_put_u32( v, 40 );                                // biSize
    bmp_put_u32( v, static_cast<std::uint32_t>( w ) );   // biWidth
    bmp_put_u32( v, static_cast<std::uint32_t>( h ) );   // biHeight
    bmp_put_u16( v, 1 );                                 // biPlanes
    bmp_put_u16( v, bpp );                               // biBitCount
    bmp_put_u32( v, 0 );                                 // biCompression = BI_RGB
    bmp_put_u32( v, 0 );                                 // biSizeImage
    bmp_put_u32( v, 0 );                                 // biXPelsPerMeter
    bmp_put_u32( v, 0 );                                 // biYPelsPerMeter
    bmp_put_u32( v, colors );                            // biClrUsed
    bmp_put_u32( v, 0 );                                 // biClrImportant
    return v;
}

struct Texel {
    std::uint8_t r, g, b, a;
    bool operator==( const Texel & ) const = default;
};

static Texel texel_at( const xash::imagelib::Image &img, std::size_t x, std::size_t y )
{
    const auto px = img.pixels();
    const std::size_t o = ( y * img.width() + x ) * 4;
    return { std::to_integer<std::uint8_t>( px[o + 0] ),
             std::to_integer<std::uint8_t>( px[o + 1] ),
             std::to_integer<std::uint8_t>( px[o + 2] ),
             std::to_integer<std::uint8_t>( px[o + 3] ) };
}

static void test_bmp_24bit_decode()
{
    using namespace xash::imagelib;

    // 2x2, 24-bpp. Row stride = 2*3 = 6 bytes -> +2 pad = 8 bytes/row. Desired
    // top-down image:  (0,0) red   (1,0) green  /  (0,1) blue  (1,1) white.
    // BMP is bottom-up and stores B,G,R, so file scanline 0 = the bottom row.
    // bfSize is deliberately bogus to prove the size-mismatch tolerance.
    std::vector<std::byte> bmp = bmp_header( 2, 2, 24, 0, /*bfSize*/ 12345 );
    bmp_put_bytes( bmp, { 0xFF, 0x00, 0x00,  0xFF, 0xFF, 0xFF,  0x00, 0x00 } ); // blue, white + pad
    bmp_put_bytes( bmp, { 0x00, 0x00, 0xFF,  0x00, 0xFF, 0x00,  0x00, 0x00 } ); // red,  green + pad

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "test.bmp", bmp );  // bfSize mismatch must be tolerated
    REQUIRE( out.has_value() );

    CHECK_EQ( out->width(), std::uint16_t{ 2 } );
    CHECK_EQ( out->height(), std::uint16_t{ 2 } );
    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( !out->has( ImageFlags::HasAlpha ) );   // 24-bit is always opaque
    CHECK( !out->palette().has_value() );          // always expands to Rgba8

    CHECK( texel_at( *out, 0, 0 ) == ( Texel{ 255, 0, 0, 255 } ) );       // red
    CHECK( texel_at( *out, 1, 0 ) == ( Texel{ 0, 255, 0, 255 } ) );       // green
    CHECK( texel_at( *out, 0, 1 ) == ( Texel{ 0, 0, 255, 255 } ) );       // blue
    CHECK( texel_at( *out, 1, 1 ) == ( Texel{ 255, 255, 255, 255 } ) );   // white
    dec.shutdown();
}

static void test_bmp_8bit_decode()
{
    using namespace xash::imagelib;

    // 2x2, 8-bpp, 4-colour palette. Row stride = 2 bytes -> +2 pad = 4 bytes/row.
    // Palette entries are on disk B,G,R,reserved and decode to R,G,B,alpha (the
    // reserved byte becomes alpha verbatim — a legacy quirk exercised here).
    std::vector<std::byte> bmp = bmp_header( 2, 2, 8, /*colors*/ 4, 0 );
    bmp_put_bytes( bmp, { 20, 10, 200, 255 } );  // idx0 -> R200 G10  B20  A255
    bmp_put_bytes( bmp, { 64, 128, 0, 200 } );   // idx1 -> R0   G128 B64  A200
    bmp_put_bytes( bmp, { 255, 255, 255, 0 } );  // idx2 -> R255 G255 B255 A0
    bmp_put_bytes( bmp, { 5, 5, 5, 255 } );      // idx3 -> R5   G5   B5   A255
    // Desired top-down: (0,0)=idx0 (1,0)=idx1 / (0,1)=idx2 (1,1)=idx3.
    // BMP bottom-up: file scanline 0 = bottom row (idx2,idx3).
    bmp_put_bytes( bmp, { 2, 3, 0, 0 } );        // scanline 0 + pad
    bmp_put_bytes( bmp, { 0, 1, 0, 0 } );        // scanline 1 + pad

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "font.bmp", bmp );
    REQUIRE( out.has_value() );

    CHECK_EQ( out->width(), std::uint16_t{ 2 } );
    CHECK_EQ( out->height(), std::uint16_t{ 2 } );
    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( !out->palette().has_value() );  // 8-bit expands to Rgba8 (no IL_KEEP_8BIT)

    CHECK( texel_at( *out, 0, 0 ) == ( Texel{ 200, 10, 20, 255 } ) );      // idx0
    CHECK( texel_at( *out, 1, 0 ) == ( Texel{ 0, 128, 64, 200 } ) );       // idx1
    CHECK( texel_at( *out, 0, 1 ) == ( Texel{ 255, 255, 255, 0 } ) );      // idx2
    CHECK( texel_at( *out, 1, 1 ) == ( Texel{ 5, 5, 5, 255 } ) );          // idx3
    CHECK( out->has( ImageFlags::HasColor ) );  // idx0/idx1/idx2 are non-grey
    dec.shutdown();
}

static void test_bmp_save_roundtrip()
{
    using namespace xash::imagelib;

    // 4x4 RGBA (width already a multiple of 4, so save adds no width padding and
    // the round-trip preserves dimensions). The (0,0) texel is semi-transparent.
    std::vector<std::byte> px( 4 * 4 * 4 );
    for( std::size_t y = 0; y < 4; ++y )
        for( std::size_t x = 0; x < 4; ++x )
        {
            const std::size_t o = ( y * 4 + x ) * 4;
            px[o + 0] = std::byte{ static_cast<std::uint8_t>( x * 40 ) };
            px[o + 1] = std::byte{ static_cast<std::uint8_t>( y * 40 ) };
            px[o + 2] = std::byte{ static_cast<std::uint8_t>( x * 16 + y ) };
            px[o + 3] = std::byte{ static_cast<std::uint8_t>( ( x == 0 && y == 0 ) ? 128 : 255 ) };
        }
    Image src( 4, 4, PixelFormat::Rgba8, std::move( px ), ImageFlags::HasColor );

    const auto saved = save_bmp( src );
    REQUIRE( saved.has_value() );
    CHECK( !saved->empty() );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "spray.bmp", *saved );
    REQUIRE( out.has_value() );
    CHECK_EQ( out->width(), std::uint16_t{ 4 } );
    CHECK_EQ( out->height(), std::uint16_t{ 4 } );
    CHECK( out->has( ImageFlags::HasAlpha ) );   // the (0,0) texel decoded alpha 128

    bool all_ok = true;
    for( std::size_t y = 0; y < 4; ++y )
        for( std::size_t x = 0; x < 4; ++x )
        {
            const std::uint8_t er = static_cast<std::uint8_t>( x * 40 );
            const std::uint8_t eg = static_cast<std::uint8_t>( y * 40 );
            const std::uint8_t eb = static_cast<std::uint8_t>( x * 16 + y );
            const std::uint8_t ea = ( x == 0 && y == 0 ) ? 128 : 255;
            all_ok = all_ok && texel_at( *out, x, y ) == ( Texel{ er, eg, eb, ea } );
        }
    CHECK( all_ok );
    dec.shutdown();
}

// ---------------------------------------------------------------------------
// DDS decode (kept-compressed; legacy Image_LoadDDS parity)
// ---------------------------------------------------------------------------

// Little-endian FourCC as it sits on disk (matches read_le<uint32>).
static constexpr std::uint32_t dds_fourcc( char a, char b, char c, char d )
{
    return   static_cast<std::uint32_t>( static_cast<std::uint8_t>( a ) )
         | ( static_cast<std::uint32_t>( static_cast<std::uint8_t>( b ) ) << 8 )
         | ( static_cast<std::uint32_t>( static_cast<std::uint8_t>( c ) ) << 16 )
         | ( static_cast<std::uint32_t>( static_cast<std::uint8_t>( d ) ) << 24 );
}

// Build a minimal single-surface DDS: 128-byte header + `payload` block bytes.
// dwFlags carries only CAPS|HEIGHT|WIDTH|PIXELFORMAT, so Image_DXTCalcSize takes
// the (one-level) mip path and the payload length must equal exactly one mip.
static std::vector<std::byte> make_dds( std::uint32_t w, std::uint32_t h,
                                        std::uint32_t fourcc,
                                        std::span<const std::byte> payload )
{
    namespace u = xash::utilities;
    std::vector<std::byte> f( 128, std::byte{ 0 } );
    auto w32 = [&]( std::size_t off, std::uint32_t v ) { u::write_le<std::uint32_t>( f.data() + off, v ); };
    w32( 0, 0x20534444u );                     // "DDS " magic
    w32( 4, 124u );                            // dwSize
    w32( 8, 0x1u | 0x2u | 0x4u | 0x1000u );    // CAPS|HEIGHT|WIDTH|PIXELFORMAT
    w32( 12, h );                              // dwHeight
    w32( 16, w );                              // dwWidth
    w32( 76, 32u );                            // pixelformat dwSize
    w32( 80, 0x4u );                           // pixelformat dwFlags = DDS_FOURCC
    w32( 84, fourcc );                         // dwFourCC
    w32( 108, 0x1000u );                       // dwCaps1 = DDS_TEXTURE
    for( const std::byte b : payload )
        f.push_back( b );
    return f;
}

static void test_dds_dxt1()
{
    using namespace xash::imagelib;

    // 4x4 => one 8-byte DXT1 block.
    std::array<std::byte, 8> block {};
    for( std::size_t i = 0; i < block.size(); ++i )
        block[i] = std::byte{ static_cast<std::uint8_t>( 0x10 + i ) };
    const auto dds = make_dds( 4, 4, dds_fourcc( 'D', 'X', 'T', '1' ), block );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "test.dds", dds );  // case-insensitive ext
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Dxt1 );
    CHECK( is_compressed( out->format() ) );
    CHECK_EQ( out->width(), std::uint16_t{ 4 } );
    CHECK_EQ( out->height(), std::uint16_t{ 4 } );
    CHECK_EQ( out->depth(), std::uint16_t{ 1 } );
    CHECK_EQ( out->mip_count(), std::uint8_t{ 1 } );
    CHECK( out->has( ImageFlags::DdsFormat ) );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( !out->has( ImageFlags::HasAlpha ) );
    CHECK( !out->has( ImageFlags::Cubemap ) );

    // Kept compressed: the raw block is copied verbatim (pixel size == 8).
    REQUIRE( out->pixels().size() == std::size_t{ 8 } );
    bool same = true;
    for( std::size_t i = 0; i < block.size(); ++i )
        same = same && out->pixels()[i] == block[i];
    CHECK( same );
    CHECK_EQ( dec.stats().images_decoded, std::uint64_t{ 1 } );
    dec.shutdown();
}

static void test_dds_dxt5_alpha()
{
    using namespace xash::imagelib;

    // 4x4 => one 16-byte DXT5 block. Image_CheckDXT5Alpha reads the block's
    // bytes [5..7] as the alpha-index bits; nonzero => HAS_ALPHA.
    std::array<std::byte, 16> block {};
    block[5] = std::byte{ 0xFF };
    block[6] = std::byte{ 0xFF };
    block[7] = std::byte{ 0xFF };
    const auto dds = make_dds( 4, 4, dds_fourcc( 'D', 'X', 'T', '5' ), block );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "a.dds", dds );
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Dxt5 );
    CHECK( out->has( ImageFlags::DdsFormat ) );
    CHECK( out->has( ImageFlags::HasAlpha ) );   // detected by the alpha scan
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK_EQ( out->pixels().size(), std::size_t{ 16 } );
    dec.shutdown();
}

static void test_dds_bad_header()
{
    using namespace xash::imagelib;

    ImageDecoder dec;
    REQUIRE( dec.init() );

    // Too short for the 128-byte header.
    std::vector<std::byte> tiny( 10, std::byte{ 0 } );
    const auto a = dec.decode( "x.dds", tiny );
    CHECK( !a.has_value() );
    CHECK( a.error() == ImageError::Truncated );

    // Full-size header, wrong magic.
    std::vector<std::byte> bad( 128, std::byte{ 0 } );
    const auto b = dec.decode( "x.dds", bad );
    CHECK( !b.has_value() );
    CHECK( b.error() == ImageError::BadHeader );

    dec.shutdown();
}

// ---------------------------------------------------------------------------
// KTX2 decode (kept-compressed BCn + Ktx2Raw passthrough; legacy Image_LoadKTX2)
// ---------------------------------------------------------------------------

static void ktx2_put_u32( std::vector<std::byte> &v, std::uint32_t x )
{
    for( int i = 0; i < 4; ++i )
        v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> ( 8 * i ) ) & 0xFF ) } );
}
static void ktx2_put_u64( std::vector<std::byte> &v, std::uint64_t x )
{
    for( int i = 0; i < 8; ++i )
        v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> ( 8 * i ) ) & 0xFF ) } );
}

// Structural header knobs (defaults describe a plain single-face/-layer 2D KTX2).
struct Ktx2Desc {
    std::uint32_t vk_format        = 0;
    std::uint32_t width            = 0;
    std::uint32_t height           = 0;
    std::uint32_t pixel_depth      = 0;
    std::uint32_t layer_count      = 0;
    std::uint32_t face_count       = 0;
    std::uint32_t supercompression = 0;
};

// Assemble a KTX2 file: 12-byte identifier + 36-byte header + 32-byte (zero)
// index + one 24-byte level record per payload + the payloads laid out
// contiguously right after the level array (so byteOffset/byteLength are honest).
static std::vector<std::byte> make_ktx2( const Ktx2Desc &d,
                                         std::span<const std::vector<std::byte>> levels )
{
    const std::uint32_t n = static_cast<std::uint32_t>( levels.size() );

    std::vector<std::byte> f;
    const std::uint8_t ident[12] = {
        0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n' };
    for( const std::uint8_t b : ident )
        f.push_back( std::byte{ b } );

    ktx2_put_u32( f, d.vk_format );         // vkFormat
    ktx2_put_u32( f, 0 );                   // typeSize
    ktx2_put_u32( f, d.width );             // pixelWidth
    ktx2_put_u32( f, d.height );            // pixelHeight
    ktx2_put_u32( f, d.pixel_depth );       // pixelDepth
    ktx2_put_u32( f, d.layer_count );       // layerCount
    ktx2_put_u32( f, d.face_count );        // faceCount
    ktx2_put_u32( f, n );                   // levelCount
    ktx2_put_u32( f, d.supercompression );  // supercompressionScheme

    for( int i = 0; i < 32; ++i )           // ktx2_index_t (unused by the decoder)
        f.push_back( std::byte{ 0 } );

    // Level records, payloads packed contiguously after the level array.
    const std::size_t payload_base = 80 + static_cast<std::size_t>( n ) * 24;
    std::size_t running = payload_base;
    for( const auto &lvl : levels )
    {
        ktx2_put_u64( f, running );           // byteOffset
        ktx2_put_u64( f, lvl.size() );        // byteLength
        ktx2_put_u64( f, lvl.size() );        // uncompressedByteLength
        running += lvl.size();
    }
    for( const auto &lvl : levels )
        for( const std::byte b : lvl )
            f.push_back( b );
    return f;
}

// Fill n bytes with a recognisable ramp so the concat order can be verified.
static std::vector<std::byte> ktx2_ramp( std::size_t n, std::uint8_t seed )
{
    std::vector<std::byte> v( n );
    for( std::size_t i = 0; i < n; ++i )
        v[i] = std::byte{ static_cast<std::uint8_t>( seed + i ) };
    return v;
}

// A single-level BC7 image is decoded kept-compressed, format+flags per the map.
static void test_ktx2_bc7()
{
    using namespace xash::imagelib;

    // 4x4 => exactly one 16-byte BC7 block.
    const auto block = ktx2_ramp( 16, 0x20 );
    std::vector<std::vector<std::byte>> levels{ block };

    // BC7_UNORM (145) -> Bc7Unorm, carries HasColor|HasAlpha.
    Ktx2Desc du; du.vk_format = 145; du.width = 4; du.height = 4;
    const auto ktx_u = make_ktx2( du, levels );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "test.KTX2", ktx_u );  // case-insensitive ext
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Bc7Unorm );
    CHECK( is_compressed( out->format() ) );
    CHECK_EQ( out->width(), std::uint16_t{ 4 } );
    CHECK_EQ( out->height(), std::uint16_t{ 4 } );
    CHECK_EQ( out->depth(), std::uint16_t{ 1 } );
    CHECK_EQ( out->mip_count(), std::uint8_t{ 1 } );
    CHECK( out->has( ImageFlags::DdsFormat ) );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( out->has( ImageFlags::HasAlpha ) );
    CHECK( !out->has( ImageFlags::Cubemap ) );

    // Kept compressed: the block bytes are copied verbatim (size == 16).
    REQUIRE( out->pixels().size() == std::size_t{ 16 } );
    bool same = true;
    for( std::size_t i = 0; i < block.size(); ++i )
        same = same && out->pixels()[i] == block[i];
    CHECK( same );
    CHECK_EQ( dec.stats().images_decoded, std::uint64_t{ 1 } );

    // BC7_SRGB (146) -> Bc7Srgb (same flags), proving the srgb map branch.
    Ktx2Desc ds; ds.vk_format = 146; ds.width = 4; ds.height = 4;
    const auto ktx_s = make_ktx2( ds, levels );
    const auto outs = dec.decode( "test.ktx2", ktx_s );
    REQUIRE( outs.has_value() );
    CHECK( outs->format() == PixelFormat::Bc7Srgb );
    CHECK( outs->has( ImageFlags::HasColor ) );
    CHECK( outs->has( ImageFlags::HasAlpha ) );
    dec.shutdown();
}

// A multi-level BC4 chain exercises the mip size cross-check + concat order.
static void test_ktx2_mipchain()
{
    using namespace xash::imagelib;

    // BC4_UNORM (139): 8 bytes/block. 8x8 => 2x2 blocks = 32 bytes; 4x4 => 8 bytes.
    const auto mip0 = ktx2_ramp( 32, 0x01 );
    const auto mip1 = ktx2_ramp( 8, 0xC0 );
    std::vector<std::vector<std::byte>> levels{ mip0, mip1 };

    Ktx2Desc d; d.vk_format = 139; d.width = 8; d.height = 8;
    const auto ktx = make_ktx2( d, levels );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "chain.ktx2", ktx );
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Bc4Unsigned );
    CHECK( is_compressed( out->format() ) );
    CHECK_EQ( out->width(), std::uint16_t{ 8 } );
    CHECK_EQ( out->height(), std::uint16_t{ 8 } );
    CHECK_EQ( out->mip_count(), std::uint8_t{ 2 } );
    CHECK( out->has( ImageFlags::DdsFormat ) );
    CHECK( !out->has( ImageFlags::HasColor ) );   // BC4 = 1 component, no colour
    CHECK( !out->has( ImageFlags::HasAlpha ) );

    // Levels concatenated in order: [mip0 (32) | mip1 (8)] == 40 bytes.
    REQUIRE( out->pixels().size() == std::size_t{ 40 } );
    bool ok = true;
    for( std::size_t i = 0; i < 32; ++i )
        ok = ok && out->pixels()[i] == mip0[i];
    for( std::size_t i = 0; i < 8; ++i )
        ok = ok && out->pixels()[32 + i] == mip1[i];
    CHECK( ok );
    dec.shutdown();
}

// A valid-but-unrecognised vkFormat falls back to Ktx2Raw (whole file verbatim).
static void test_ktx2_raw_fallback()
{
    using namespace xash::imagelib;

    // VK_FORMAT_R8G8B8A8_UNORM (37) is a legal KTX2 format we do not map to a
    // PixelFormat -> the whole file is handed through for ref_vk.
    std::vector<std::vector<std::byte>> levels{ ktx2_ramp( 16, 0x70 ) };
    Ktx2Desc d; d.vk_format = 37; d.width = 4; d.height = 4;
    const auto ktx = make_ktx2( d, levels );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "raw.ktx2", ktx );
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Ktx2Raw );
    CHECK( !is_compressed( out->format() ) );      // Ktx2Raw is a passthrough, not BCn
    CHECK( out->has( ImageFlags::DdsFormat ) );
    CHECK( !out->has( ImageFlags::HasColor ) );    // no per-format flags on the raw path
    CHECK( !out->has( ImageFlags::HasAlpha ) );
    CHECK_EQ( out->width(), std::uint16_t{ 4 } );
    CHECK_EQ( out->height(), std::uint16_t{ 4 } );

    // The stored payload is the entire file, byte for byte.
    REQUIRE( out->pixels().size() == ktx.size() );
    bool same = true;
    for( std::size_t i = 0; i < ktx.size(); ++i )
        same = same && out->pixels()[i] == ktx[i];
    CHECK( same );
    dec.shutdown();
}

// Bounds-safety + rejection paths: malformed / unsupported inputs must return an
// error, never fault.
static void test_ktx2_bad_input()
{
    using namespace xash::imagelib;

    ImageDecoder dec;
    REQUIRE( dec.init() );

    // Shorter than the 104-byte minimal prologue.
    {
        std::vector<std::byte> v( 50, std::byte{ 0 } );
        const auto img = dec.decode( "x.ktx2", v );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::Truncated );
    }
    // Full-size prologue, wrong identifier.
    {
        std::vector<std::byte> v( 104, std::byte{ 0 } );
        const auto img = dec.decode( "x.ktx2", v );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::BadHeader );
    }
    // Supercompression is rejected outright (never raw-passed).
    {
        std::vector<std::vector<std::byte>> levels{ ktx2_ramp( 16, 0 ) };
        Ktx2Desc d; d.vk_format = 145; d.width = 4; d.height = 4; d.supercompression = 1;
        const auto ktx = make_ktx2( d, levels );
        const auto img = dec.decode( "x.ktx2", ktx );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::UnsupportedFeature );
    }
    // 3D (pixelDepth > 1) is rejected.
    {
        std::vector<std::vector<std::byte>> levels{ ktx2_ramp( 16, 0 ) };
        Ktx2Desc d; d.vk_format = 145; d.width = 4; d.height = 4; d.pixel_depth = 2;
        const auto ktx = make_ktx2( d, levels );
        const auto img = dec.decode( "x.ktx2", ktx );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::UnsupportedFeature );
    }
    // Multi-face (faceCount > 1, e.g. a cubemap) is rejected.
    {
        std::vector<std::vector<std::byte>> levels{ ktx2_ramp( 16, 0 ) };
        Ktx2Desc d; d.vk_format = 145; d.width = 4; d.height = 4; d.face_count = 6;
        const auto ktx = make_ktx2( d, levels );
        const auto img = dec.decode( "x.ktx2", ktx );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::UnsupportedFeature );
    }
    // Recognised format but the level byteLength disagrees with the computed mip
    // size (4x4 BC7 must be 16 bytes; give it 15) -> BadHeader.
    {
        std::vector<std::vector<std::byte>> levels{ ktx2_ramp( 15, 0 ) };
        Ktx2Desc d; d.vk_format = 145; d.width = 4; d.height = 4;
        const auto ktx = make_ktx2( d, levels );
        const auto img = dec.decode( "x.ktx2", ktx );
        CHECK( !img.has_value() );
        CHECK( img.error() == ImageError::BadHeader );
    }
    dec.shutdown();
}

// ---------------------------------------------------------------------------
// MIP (miptex) decode — legacy Image_LoadMIP parity. Small hand-built lumps.
// ---------------------------------------------------------------------------

static void mip_put_u16( std::vector<std::byte> &v, std::uint16_t x )
{
    v.push_back( std::byte{ static_cast<std::uint8_t>( x & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 8 ) & 0xFF ) } );
}
static void mip_put_u32( std::vector<std::byte> &v, std::uint32_t x )
{
    for( int i = 0; i < 4; ++i )
        v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> ( 8 * i ) ) & 0xFF ) } );
}

// Build a miptex lump: mip_t header (name[16] + w + h + offsets[4]), four mip
// levels (level 0 = `indices`, levels 1..3 zero-filled), and — when `pal768` is
// non-empty — a 2-byte colour count (256) + the 256*RGB embedded palette (an HL
// mip). An empty `pal768` yields a Quake1 mip (no palette).
static std::vector<std::byte> build_mip( std::string_view mipname, std::uint32_t w, std::uint32_t h,
                                         const std::vector<std::uint8_t> &indices,
                                         const std::vector<std::uint8_t> &pal768 )
{
    const std::uint32_t m0 = w * h, m1 = m0 / 4, m2 = m0 / 16, m3 = m0 / 64;
    std::vector<std::byte> v;
    for( std::size_t i = 0; i < 16; ++i )
        v.push_back( std::byte{ i < mipname.size() ? static_cast<std::uint8_t>( mipname[i] ) : std::uint8_t{ 0 } } );
    mip_put_u32( v, w );
    mip_put_u32( v, h );
    const std::uint32_t o0 = 40, o1 = o0 + m0, o2 = o1 + m1, o3 = o2 + m2;
    mip_put_u32( v, o0 );
    mip_put_u32( v, o1 );
    mip_put_u32( v, o2 );
    mip_put_u32( v, o3 );
    for( std::uint32_t i = 0; i < m0; ++i )
        v.push_back( std::byte{ i < indices.size() ? indices[i] : std::uint8_t{ 0 } } );
    for( std::uint32_t i = 0; i < m1 + m2 + m3; ++i )
        v.push_back( std::byte{ 0 } );
    if( !pal768.empty() )
    {
        mip_put_u16( v, 256 );
        for( std::uint32_t i = 0; i < 768; ++i )
            v.push_back( std::byte{ i < pal768.size() ? pal768[i] : std::uint8_t{ 0 } } );
    }
    return v;
}

// The built-in Quake palette bytes (index-loop copy — no span-iterator warnings).
static std::vector<std::uint8_t> quake_pal_bytes()
{
    const auto s = xash::imagelib::quake_palette_rgb();
    std::vector<std::uint8_t> v;
    v.reserve( s.size() );
    for( std::size_t i = 0; i < s.size(); ++i )
        v.push_back( s[i] );
    return v;
}

// An HL mip whose embedded palette IS the Quake palette: classifies PAL_QUAKE1,
// so it decodes through LUMP_NORMAL (no texgamma) with the QUAKEPAL flag.
static void test_mip_hl_quake_palette()
{
    using namespace xash::imagelib;

    const auto qp = quake_pal_bytes();
    std::vector<std::uint8_t> idx( 64 );
    for( int i = 0; i < 64; ++i )
        idx[static_cast<std::size_t>( i )] = static_cast<std::uint8_t>( i );  // all < 225, none 255
    const auto mip = build_mip( "qtex", 8, 8, idx, qp );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "qtex.MIP", mip );  // case-insensitive ext
    REQUIRE( out.has_value() );

    CHECK_EQ( out->width(), std::uint16_t{ 8 } );
    CHECK_EQ( out->height(), std::uint16_t{ 8 } );
    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( out->has( ImageFlags::QuakePal ) );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( !out->has( ImageFlags::HasLuma ) );
    CHECK( !out->has( ImageFlags::HasAlpha ) );

    const auto px = out->pixels();
    REQUIRE( px.size() == 64 * 4 );
    bool ok = true;
    for( int i = 0; i < 64; ++i )
        ok = ok
          && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == qp[static_cast<std::size_t>( i * 3 + 0 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == qp[static_cast<std::size_t>( i * 3 + 1 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == qp[static_cast<std::size_t>( i * 3 + 2 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == 255;
    CHECK( ok );
    CHECK_EQ( dec.stats().images_decoded, std::uint64_t{ 1 } );
    dec.shutdown();
}

// A '{'-masked HL mip: index 255 decodes fully transparent, others opaque, and
// the OneBitAlpha + HasAlpha flags are set.
static void test_mip_masked()
{
    using namespace xash::imagelib;

    std::vector<std::uint8_t> pal( 768 );
    for( int i = 0; i < 256; ++i )
    {
        pal[static_cast<std::size_t>( i * 3 + 0 )] = static_cast<std::uint8_t>( i );
        pal[static_cast<std::size_t>( i * 3 + 1 )] = static_cast<std::uint8_t>( 255 - i );
        pal[static_cast<std::size_t>( i * 3 + 2 )] = static_cast<std::uint8_t>( i / 2 );
    }
    std::vector<std::uint8_t> idx( 64 );
    for( int i = 0; i < 64; ++i )
        idx[static_cast<std::size_t>( i )] = static_cast<std::uint8_t>( i );
    idx[10] = 255;  // one transparent-key pixel
    const auto mip = build_mip( "{decal", 8, 8, idx, pal );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "{decal.mip", mip );
    REQUIRE( out.has_value() );

    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( out->has( ImageFlags::OneBitAlpha ) );
    CHECK( out->has( ImageFlags::HasAlpha ) );

    const auto px = out->pixels();
    REQUIRE( px.size() == 64 * 4 );
    bool ok = true;
    for( int i = 0; i < 64; ++i )
    {
        const std::uint8_t id = idx[static_cast<std::size_t>( i )];
        std::uint8_t er, eg, eb, ea;
        if( id == 255 ) { er = 0; eg = 0; eb = 0; ea = 0; }
        else { er = id; eg = static_cast<std::uint8_t>( 255 - id ); eb = static_cast<std::uint8_t>( id / 2 ); ea = 255; }
        ok = ok
          && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == er
          && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == eg
          && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == eb
          && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == ea;
    }
    CHECK( ok );
    dec.shutdown();
}

// A Quake1 mip (no embedded palette) decodes against the built-in Q1 table.
static void test_mip_quake1_nopalette()
{
    using namespace xash::imagelib;

    const auto qp = quake_pal_bytes();
    std::vector<std::uint8_t> idx( 64 );
    for( int i = 0; i < 64; ++i )
        idx[static_cast<std::size_t>( i )] = static_cast<std::uint8_t>( i );  // < 225
    const std::vector<std::uint8_t> nopal;  // empty => Quake1 mip
    const auto mip = build_mip( "quake", 8, 8, idx, nopal );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "quake.mip", mip );
    REQUIRE( out.has_value() );

    CHECK( out->has( ImageFlags::QuakePal ) );
    CHECK( !out->has( ImageFlags::HasLuma ) );
    CHECK( !out->has( ImageFlags::HasAlpha ) );

    const auto px = out->pixels();
    REQUIRE( px.size() == 64 * 4 );
    bool ok = true;
    for( int i = 0; i < 64; ++i )
        ok = ok
          && std::to_integer<std::uint8_t>( px[i * 4 + 0] ) == qp[static_cast<std::size_t>( i * 3 + 0 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 1] ) == qp[static_cast<std::size_t>( i * 3 + 1 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 2] ) == qp[static_cast<std::size_t>( i * 3 + 2 )]
          && std::to_integer<std::uint8_t>( px[i * 4 + 3] ) == 255;
    CHECK( ok );
    dec.shutdown();
}

// A fullbright index ( > 224, != 255 ) in a Quake1 mip flags a luma layer.
static void test_mip_quake1_luma()
{
    using namespace xash::imagelib;

    std::vector<std::uint8_t> idx( 64, 10 );
    idx[5] = 230;  // fullbright
    const std::vector<std::uint8_t> nopal;
    const auto mip = build_mip( "torch", 8, 8, idx, nopal );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "torch.mip", mip );
    REQUIRE( out.has_value() );
    CHECK( out->has( ImageFlags::HasLuma ) );
    CHECK( out->has( ImageFlags::QuakePal ) );
    dec.shutdown();
}

// A 2:1 texture whose embedded name starts "sky" gets the QuakeSky flag.
static void test_mip_quakesky()
{
    using namespace xash::imagelib;

    std::vector<std::uint8_t> idx( 128 );  // 16x8
    for( int i = 0; i < 128; ++i )
        idx[static_cast<std::size_t>( i )] = static_cast<std::uint8_t>( i % 200 );  // avoid fullbright
    const std::vector<std::uint8_t> nopal;
    const auto mip = build_mip( "sky1", 16, 8, idx, nopal );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "sky1.mip", mip );
    REQUIRE( out.has_value() );
    CHECK( out->has( ImageFlags::QuakeSky ) );
    CHECK( out->has( ImageFlags::QuakePal ) );
    dec.shutdown();
}

// An HL water mip ("!..." embedded name) parses fog colour+density into
// fog_params (palette entry 3 RGB + entry 4 red).
static void test_mip_water_fog()
{
    using namespace xash::imagelib;

    std::vector<std::uint8_t> pal( 768, 0 );
    for( int i = 0; i < 256; ++i )  // greyscale base so it classifies Custom (not Q1/HL)
    {
        pal[static_cast<std::size_t>( i * 3 + 0 )] = static_cast<std::uint8_t>( i );
        pal[static_cast<std::size_t>( i * 3 + 1 )] = static_cast<std::uint8_t>( i );
        pal[static_cast<std::size_t>( i * 3 + 2 )] = static_cast<std::uint8_t>( i );
    }
    pal[3 * 3 + 0] = 200; pal[3 * 3 + 1] = 100; pal[3 * 3 + 2] = 50;  // fog colour
    pal[4 * 3 + 0] = 77;                                              // fog density
    const std::vector<std::uint8_t> idx( 64, 1 );
    const auto mip = build_mip( "!water", 8, 8, idx, pal );

    ImageDecoder dec;
    REQUIRE( dec.init() );
    const auto out = dec.decode( "!water.mip", mip );
    REQUIRE( out.has_value() );

    const Rgba fp = out->fog_params();
    CHECK_EQ( fp.r, std::uint8_t{ 200 } );
    CHECK_EQ( fp.g, std::uint8_t{ 100 } );
    CHECK_EQ( fp.b, std::uint8_t{ 50 } );
    CHECK_EQ( fp.a, std::uint8_t{ 77 } );
    dec.shutdown();
}

// Bounds safety: malformed mips return an error, never fault.
static void test_mip_bad_input()
{
    using namespace xash::imagelib;

    ImageDecoder dec;
    REQUIRE( dec.init() );

    // Shorter than the 40-byte mip_t header.
    {
        std::vector<std::byte> v( 10, std::byte{ 0 } );
        const auto r = dec.decode( "x.mip", v );
        CHECK( !r.has_value() );
        CHECK( r.error() == ImageError::Truncated );
    }
    // Valid header claiming 8x8 at offset0=40, but no pixel body at all.
    {
        std::vector<std::byte> v;
        for( int i = 0; i < 16; ++i ) v.push_back( std::byte{ 0 } );
        mip_put_u32( v, 8 ); mip_put_u32( v, 8 );
        mip_put_u32( v, 40 ); mip_put_u32( v, 0 ); mip_put_u32( v, 0 ); mip_put_u32( v, 0 );
        const auto r = dec.decode( "t.mip", v );
        CHECK( !r.has_value() );
        CHECK( r.error() == ImageError::Truncated );
    }
    // Zero width => bad header.
    {
        std::vector<std::byte> v;
        for( int i = 0; i < 16; ++i ) v.push_back( std::byte{ 0 } );
        mip_put_u32( v, 0 ); mip_put_u32( v, 8 );
        mip_put_u32( v, 40 ); mip_put_u32( v, 0 ); mip_put_u32( v, 0 ); mip_put_u32( v, 0 );
        const auto r = dec.decode( "t.mip", v );
        CHECK( !r.has_value() );
        CHECK( r.error() == ImageError::BadHeader );
    }
    dec.shutdown();
}

// ---------------------------------------------------------------------------
// PNG decode + save (chunk deliverable) — build valid PNGs in memory with miniz
// (deflate the filtered scanlines, CRC each chunk) then decode via the registry.
// ---------------------------------------------------------------------------

static void png_put_u32be( std::vector<std::byte> &v, std::uint32_t x )
{
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 24 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 16 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >>  8 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>(   x         & 0xFF ) } );
}

// Append a full PNG chunk (length + 4-char tag + data + CRC-32 over tag+data).
static void png_chunk( std::vector<std::byte> &v, const char ( &tag )[5],
                       std::span<const std::uint8_t> data )
{
    png_put_u32be( v, static_cast<std::uint32_t>( data.size() ) );
    std::vector<unsigned char> crcbuf;
    for( int i = 0; i < 4; ++i )
        crcbuf.push_back( static_cast<unsigned char>( tag[i] ) );
    for( const std::uint8_t b : data )
        crcbuf.push_back( b );
    for( const unsigned char c : crcbuf )
        v.push_back( std::byte{ c } );
    const mz_ulong crc = mz_crc32( MZ_CRC32_INIT, crcbuf.data(), crcbuf.size() );
    png_put_u32be( v, static_cast<std::uint32_t>( crc ) );
}

// Build a filter-None scanline stream (each row: 0 byte + rowsize raw samples).
static std::vector<std::uint8_t> png_none_stream( std::uint32_t w, std::uint32_t h,
                                                  std::size_t pixel_size,
                                                  std::span<const std::uint8_t> samples )
{
    const std::size_t rowsize = pixel_size * w;
    std::vector<std::uint8_t> s;
    for( std::uint32_t y = 0; y < h; ++y )
    {
        s.push_back( 0 );  // PNG_F_NONE
        for( std::size_t i = 0; i < rowsize; ++i )
            s.push_back( samples[y * rowsize + i] );
    }
    return s;
}

static std::uint8_t png_paeth_pred( int a, int b, int c )
{
    const int p = a + b - c;
    int pa = p - a; if( pa < 0 ) pa = -pa;
    int pb = p - b; if( pb < 0 ) pb = -pb;
    int pc = p - c; if( pc < 0 ) pc = -pc;
    return static_cast<std::uint8_t>( ( pc < pa && pc < pb ) ? c : ( pb < pa ) ? b : a );
}

// Forward-filter each row of `raw` (h*w*bpp bytes) with the per-row filter `ft`,
// producing the scanline stream the decoder must invert.
static std::vector<std::uint8_t> png_filtered_stream(
    std::uint32_t w, std::uint32_t h, std::size_t bpp,
    std::span<const std::uint8_t> raw, std::span<const std::uint8_t> ft )
{
    const std::size_t rowsize = bpp * w;
    std::vector<std::uint8_t> out;
    for( std::uint32_t y = 0; y < h; ++y )
    {
        const std::uint8_t f = ft[y];
        out.push_back( f );
        for( std::size_t i = 0; i < rowsize; ++i )
        {
            const int cur  = raw[y * rowsize + i];
            const int left = ( i >= bpp ) ? raw[y * rowsize + i - bpp] : 0;
            const int up   = ( y > 0 ) ? raw[( y - 1 ) * rowsize + i] : 0;
            const int ul   = ( y > 0 && i >= bpp ) ? raw[( y - 1 ) * rowsize + i - bpp] : 0;
            int fv = cur;
            switch( f )
            {
            case 1: fv = cur - left; break;                              // Sub
            case 2: fv = cur - up; break;                                // Up
            case 3: fv = cur - ( ( left + up ) >> 1 ); break;            // Average
            case 4: fv = cur - png_paeth_pred( left, up, ul ); break;    // Paeth
            default: fv = cur; break;                                    // None
            }
            out.push_back( static_cast<std::uint8_t>( fv & 0xFF ) );
        }
    }
    return out;
}

// Assemble a full PNG: signature + IHDR + optional PLTE/tRNS + IDAT(deflate) + IEND.
static std::vector<std::byte> png_assemble( std::uint32_t w, std::uint32_t h, std::uint8_t colortype,
                                            std::span<const std::uint8_t> filtered_stream,
                                            std::span<const std::uint8_t> plte = {},
                                            std::span<const std::uint8_t> trns = {} )
{
    auto u8 = []( auto x ) { return static_cast<std::uint8_t>( x ); };

    std::vector<std::byte> png;
    const std::uint8_t sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    for( const std::uint8_t sb : sig )
        png.push_back( std::byte{ sb } );

    std::vector<std::uint8_t> ihdr;
    ihdr.push_back( u8( w >> 24 ) ); ihdr.push_back( u8( w >> 16 ) );
    ihdr.push_back( u8( w >>  8 ) ); ihdr.push_back( u8( w ) );
    ihdr.push_back( u8( h >> 24 ) ); ihdr.push_back( u8( h >> 16 ) );
    ihdr.push_back( u8( h >>  8 ) ); ihdr.push_back( u8( h ) );
    ihdr.push_back( u8( 8 ) );        // bit depth
    ihdr.push_back( colortype );
    ihdr.push_back( u8( 0 ) );        // compression
    ihdr.push_back( u8( 0 ) );        // filter
    ihdr.push_back( u8( 0 ) );        // interlace
    png_chunk( png, "IHDR", ihdr );

    if( !plte.empty() ) png_chunk( png, "PLTE", plte );
    if( !trns.empty() ) png_chunk( png, "tRNS", trns );

    const mz_ulong bound = mz_compressBound( static_cast<mz_ulong>( filtered_stream.size() ) );
    std::vector<unsigned char> comp( bound != 0 ? bound : 1 );
    mz_ulong comp_len = static_cast<mz_ulong>( comp.size() );
    const int rc = mz_compress2( comp.data(), &comp_len, filtered_stream.data(),
                                 static_cast<mz_ulong>( filtered_stream.size() ), MZ_BEST_COMPRESSION );
    REQUIRE( rc == MZ_OK );
    std::vector<std::uint8_t> idat;
    for( mz_ulong i = 0; i < comp_len; ++i )
        idat.push_back( comp[i] );
    png_chunk( png, "IDAT", idat );

    png_chunk( png, "IEND", std::span<const std::uint8_t>{} );
    return png;
}

// Pixel accessor for the Rgba8 decode output.
static std::uint8_t png_px( const xash::imagelib::Image &img, std::size_t i, std::size_t c )
{
    return std::to_integer<std::uint8_t>( img.pixels()[i * 4 + c] );
}

static void test_png_rgb_decode()
{
    using namespace xash::imagelib;
    const std::uint8_t px[4][3] = { { 10, 20, 30 }, { 40, 50, 60 }, { 70, 80, 90 }, { 100, 110, 120 } };
    std::vector<std::uint8_t> samples;
    for( const auto &c : px ) { samples.push_back( c[0] ); samples.push_back( c[1] ); samples.push_back( c[2] ); }
    const auto stream = png_none_stream( 2, 2, 3, samples );
    const auto png    = png_assemble( 2, 2, 2 /*RGB*/, stream );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "a.png", png );
    REQUIRE( out.has_value() );
    CHECK_EQ( out->width(), std::uint16_t{ 2 } );
    CHECK_EQ( out->height(), std::uint16_t{ 2 } );
    CHECK( out->format() == PixelFormat::Rgba8 );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( !out->has( ImageFlags::HasAlpha ) );  // no tRNS, RGB has no alpha sample
    REQUIRE( out->pixels().size() == 2 * 2 * 4 );
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        ok = ok && png_px( *out, i, 0 ) == px[i][0] && png_px( *out, i, 1 ) == px[i][1]
                && png_px( *out, i, 2 ) == px[i][2] && png_px( *out, i, 3 ) == 255;
    CHECK( ok );
    CHECK_EQ( dec.stats().images_decoded, std::uint64_t{ 1 } );
    dec.shutdown();
}

static void test_png_rgba_decode()
{
    using namespace xash::imagelib;
    const std::uint8_t px[4][4] = {
        { 10, 20, 30, 255 }, { 40, 50, 60, 128 }, { 70, 80, 90, 200 }, { 100, 110, 120, 0 } };
    std::vector<std::uint8_t> samples;
    for( const auto &c : px ) for( int k = 0; k < 4; ++k ) samples.push_back( c[k] );
    const auto stream = png_none_stream( 2, 2, 4, samples );
    const auto png    = png_assemble( 2, 2, 6 /*RGBA*/, stream );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "a.png", png );
    REQUIRE( out.has_value() );
    CHECK( out->has( ImageFlags::HasColor ) );
    CHECK( out->has( ImageFlags::HasAlpha ) );
    REQUIRE( out->pixels().size() == 2 * 2 * 4 );
    bool ok = true;
    for( int i = 0; i < 4; ++i ) for( int k = 0; k < 4; ++k )
        ok = ok && png_px( *out, i, k ) == px[i][k];
    CHECK( ok );
    dec.shutdown();
}

static void test_png_grey_and_alpha()
{
    using namespace xash::imagelib;

    // Greyscale (colortype 0): 1 byte/pixel -> R=G=B=v, opaque; no colour bit.
    {
        const std::uint8_t g[4] = { 5, 128, 200, 255 };
        std::vector<std::uint8_t> samples( g, g + 4 );
        const auto stream = png_none_stream( 2, 2, 1, samples );
        const auto png    = png_assemble( 2, 2, 0 /*GREY*/, stream );
        ImageDecoder dec; REQUIRE( dec.init() );
        const auto out = dec.decode( "g.png", png );
        REQUIRE( out.has_value() );
        CHECK( !out->has( ImageFlags::HasColor ) );
        CHECK( !out->has( ImageFlags::HasAlpha ) );
        bool ok = true;
        for( int i = 0; i < 4; ++i )
            ok = ok && png_px( *out, i, 0 ) == g[i] && png_px( *out, i, 1 ) == g[i]
                    && png_px( *out, i, 2 ) == g[i] && png_px( *out, i, 3 ) == 255;
        CHECK( ok );
        dec.shutdown();
    }

    // Grey+alpha (colortype 4): 2 bytes/pixel -> R=G=B=grey, A=alpha; alpha bit set.
    {
        const std::uint8_t ga[4][2] = { { 5, 255 }, { 128, 64 }, { 200, 0 }, { 255, 200 } };
        std::vector<std::uint8_t> samples;
        for( const auto &c : ga ) { samples.push_back( c[0] ); samples.push_back( c[1] ); }
        const auto stream = png_none_stream( 2, 2, 2, samples );
        const auto png    = png_assemble( 2, 2, 4 /*GREY+ALPHA*/, stream );
        ImageDecoder dec; REQUIRE( dec.init() );
        const auto out = dec.decode( "ga.png", png );
        REQUIRE( out.has_value() );
        CHECK( !out->has( ImageFlags::HasColor ) );
        CHECK( out->has( ImageFlags::HasAlpha ) );
        bool ok = true;
        for( int i = 0; i < 4; ++i )
            ok = ok && png_px( *out, i, 0 ) == ga[i][0] && png_px( *out, i, 1 ) == ga[i][0]
                    && png_px( *out, i, 2 ) == ga[i][0] && png_px( *out, i, 3 ) == ga[i][1];
        CHECK( ok );
        dec.shutdown();
    }
}

static void test_png_palette()
{
    using namespace xash::imagelib;
    // 4-entry PLTE; tRNS gives per-index alpha for the first 3 (idx3 -> opaque).
    const std::uint8_t plte[12] = { 10, 20, 30,  40, 50, 60,  70, 80, 90,  100, 110, 120 };
    const std::uint8_t trns[3]  = { 255, 128, 0 };
    const std::uint8_t idx[4]   = { 0, 1, 2, 3 };
    std::vector<std::uint8_t> samples( idx, idx + 4 );
    std::vector<std::uint8_t> plte_v( plte, plte + 12 );
    std::vector<std::uint8_t> trns_v( trns, trns + 3 );
    const auto stream = png_none_stream( 2, 2, 1, samples );
    const auto png    = png_assemble( 2, 2, 3 /*PALETTE*/, stream, plte_v, trns_v );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "pal.png", png );
    REQUIRE( out.has_value() );
    CHECK( out->has( ImageFlags::HasColor ) );  // PALETTE carries the colour bit
    CHECK( out->has( ImageFlags::HasAlpha ) );  // tRNS present
    const std::uint8_t expa[4] = { 255, 128, 0, 255 };
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        ok = ok && png_px( *out, i, 0 ) == plte[i * 3 + 0] && png_px( *out, i, 1 ) == plte[i * 3 + 1]
                && png_px( *out, i, 2 ) == plte[i * 3 + 2] && png_px( *out, i, 3 ) == expa[i];
    CHECK( ok );
    dec.shutdown();
}

static void test_png_trns_colorkey()
{
    using namespace xash::imagelib;
    // RGB with a tRNS colour key (200,100,50) -> matching pixels decode to alpha 0.
    const std::uint8_t px[4][3] = { { 10, 20, 30 }, { 200, 100, 50 }, { 70, 80, 90 }, { 200, 100, 50 } };
    const std::uint8_t trns[6]  = { 0, 200, 0, 100, 0, 50 };  // 16-bit-per-sample, high byte 0
    std::vector<std::uint8_t> samples;
    for( const auto &c : px ) { samples.push_back( c[0] ); samples.push_back( c[1] ); samples.push_back( c[2] ); }
    std::vector<std::uint8_t> trns_v( trns, trns + 6 );
    const auto stream = png_none_stream( 2, 2, 3, samples );
    const auto png    = png_assemble( 2, 2, 2 /*RGB*/, stream, {}, trns_v );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "key.png", png );
    REQUIRE( out.has_value() );
    CHECK( out->has( ImageFlags::HasAlpha ) );  // tRNS forces the alpha bit on RGB
    const std::uint8_t expa[4] = { 255, 0, 255, 0 };
    bool ok = true;
    for( int i = 0; i < 4; ++i )
        ok = ok && png_px( *out, i, 0 ) == px[i][0] && png_px( *out, i, 1 ) == px[i][1]
                && png_px( *out, i, 2 ) == px[i][2] && png_px( *out, i, 3 ) == expa[i];
    CHECK( ok );
    dec.shutdown();
}

static void test_png_filters()
{
    using namespace xash::imagelib;
    // 4x4 RGB; each row uses a different filter so the decoder must invert Sub,
    // Up, Average and Paeth to reproduce the original samples.
    const std::uint32_t w = 4, h = 4;
    std::vector<std::uint8_t> raw;
    for( std::uint32_t y = 0; y < h; ++y )
        for( std::uint32_t x = 0; x < w; ++x )
        {
            raw.push_back( static_cast<std::uint8_t>( 16 * x + y ) );
            raw.push_back( static_cast<std::uint8_t>( 255 - 16 * x + 2 * y ) );
            raw.push_back( static_cast<std::uint8_t>( 8 * x + 32 * y ) );
        }
    const std::uint8_t ft[4] = { 1, 2, 3, 4 };  // Sub, Up, Average, Paeth
    std::vector<std::uint8_t> ftv( ft, ft + 4 );
    const auto stream = png_filtered_stream( w, h, 3, raw, ftv );
    const auto png    = png_assemble( w, h, 2 /*RGB*/, stream );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "f.png", png );
    REQUIRE( out.has_value() );
    CHECK_EQ( out->width(), std::uint16_t{ 4 } );
    CHECK_EQ( out->height(), std::uint16_t{ 4 } );
    REQUIRE( out->pixels().size() == w * h * 4 );
    bool ok = true;
    for( std::uint32_t i = 0; i < w * h; ++i )
        ok = ok && png_px( *out, i, 0 ) == raw[i * 3 + 0] && png_px( *out, i, 1 ) == raw[i * 3 + 1]
                && png_px( *out, i, 2 ) == raw[i * 3 + 2] && png_px( *out, i, 3 ) == 255;
    CHECK( ok );
    dec.shutdown();
}

static void test_png_bad_input()
{
    using namespace xash::imagelib;
    ImageDecoder dec; REQUIRE( dec.init() );

    // A valid 1x1 RGB baseline to corrupt.
    const std::uint8_t px[3] = { 100, 150, 200 };
    std::vector<std::uint8_t> samples( px, px + 3 );
    const auto stream = png_none_stream( 1, 1, 3, samples );
    const auto good   = png_assemble( 1, 1, 2, stream );

    // Baseline decodes.
    {
        const auto out = dec.decode( "ok.png", good );
        REQUIRE( out.has_value() );
        CHECK( png_px( *out, 0, 0 ) == 100 );
        CHECK( png_px( *out, 0, 3 ) == 255 );
    }
    // Shorter than the 33-byte header => Truncated.
    {
        std::vector<std::byte> v( good.begin(), good.begin() + 10 );
        const auto out = dec.decode( "t.png", v );
        CHECK( !out.has_value() );
        CHECK( out.error() == ImageError::Truncated );
    }
    // Corrupt the signature => BadHeader.
    {
        std::vector<std::byte> v = good;
        v[1] = std::byte{ 0x00 };
        const auto out = dec.decode( "s.png", v );
        CHECK( !out.has_value() );
        CHECK( out.error() == ImageError::BadHeader );
    }
    // Corrupt the IHDR CRC (byte 29) while leaving the fields valid => BadHeader.
    {
        std::vector<std::byte> v = good;
        v[29] = std::byte{ static_cast<std::uint8_t>( std::to_integer<std::uint8_t>( v[29] ) ^ 0xFF ) };
        const auto out = dec.decode( "c.png", v );
        CHECK( !out.has_value() );
        CHECK( out.error() == ImageError::BadHeader );
    }
    dec.shutdown();
}

static void test_png_save_roundtrip()
{
    using namespace xash::imagelib;
    // 3x2 RGBA with varied alpha; save_png -> decode must reproduce the pixels.
    const std::uint32_t w = 3, h = 2;
    std::vector<std::byte> srcpx( w * h * 4 );
    for( std::uint32_t i = 0; i < w * h; ++i )
    {
        srcpx[i * 4 + 0] = std::byte{ static_cast<std::uint8_t>( 20 * i + 1 ) };
        srcpx[i * 4 + 1] = std::byte{ static_cast<std::uint8_t>( 255 - 15 * i ) };
        srcpx[i * 4 + 2] = std::byte{ static_cast<std::uint8_t>( 7 * i + 3 ) };
        srcpx[i * 4 + 3] = std::byte{ static_cast<std::uint8_t>( ( i % 2 ) ? 128 : 255 ) };
    }
    Image src( static_cast<std::uint16_t>( w ), static_cast<std::uint16_t>( h ),
               PixelFormat::Rgba8, std::move( srcpx ), ImageFlags::HasColor | ImageFlags::HasAlpha );

    const auto saved = save_png( src );
    REQUIRE( saved.has_value() );
    CHECK( !saved->empty() );

    ImageDecoder dec; REQUIRE( dec.init() );
    const auto out = dec.decode( "rt.png", *saved );
    REQUIRE( out.has_value() );
    CHECK_EQ( out->width(), std::uint16_t{ 3 } );
    CHECK_EQ( out->height(), std::uint16_t{ 2 } );
    CHECK( out->has( ImageFlags::HasAlpha ) );
    REQUIRE( out->pixels().size() == w * h * 4 );
    bool ok = true;
    for( std::uint32_t i = 0; i < w * h; ++i )
    {
        const std::uint8_t er = static_cast<std::uint8_t>( 20 * i + 1 );
        const std::uint8_t eg = static_cast<std::uint8_t>( 255 - 15 * i );
        const std::uint8_t eb = static_cast<std::uint8_t>( 7 * i + 3 );
        const std::uint8_t ea = ( i % 2 ) ? std::uint8_t{ 128 } : std::uint8_t{ 255 };
        ok = ok && png_px( *out, i, 0 ) == er && png_px( *out, i, 1 ) == eg
                && png_px( *out, i, 2 ) == eb && png_px( *out, i, 3 ) == ea;
    }
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
    RUN_TEST( test_bmp_24bit_decode );
    RUN_TEST( test_bmp_8bit_decode );
    RUN_TEST( test_bmp_save_roundtrip );
    RUN_TEST( test_dds_dxt1 );
    RUN_TEST( test_dds_dxt5_alpha );
    RUN_TEST( test_dds_bad_header );
    RUN_TEST( test_ktx2_bc7 );
    RUN_TEST( test_ktx2_mipchain );
    RUN_TEST( test_ktx2_raw_fallback );
    RUN_TEST( test_ktx2_bad_input );
    RUN_TEST( test_mip_hl_quake_palette );
    RUN_TEST( test_mip_masked );
    RUN_TEST( test_mip_quake1_nopalette );
    RUN_TEST( test_mip_quake1_luma );
    RUN_TEST( test_mip_quakesky );
    RUN_TEST( test_mip_water_fog );
    RUN_TEST( test_mip_bad_input );
    RUN_TEST( test_png_rgb_decode );
    RUN_TEST( test_png_rgba_decode );
    RUN_TEST( test_png_grey_and_alpha );
    RUN_TEST( test_png_palette );
    RUN_TEST( test_png_trns_colorkey );
    RUN_TEST( test_png_filters );
    RUN_TEST( test_png_bad_input );
    RUN_TEST( test_png_save_roundtrip );

    std::printf( "test_imagelib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
