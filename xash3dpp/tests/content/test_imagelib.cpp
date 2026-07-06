// xash3dpp — imagelib tests
// Covers: init/shutdown lifecycle (idempotent). Codec/decode + WAD pack/unpack
// tests land with the O-2 implementation.

#include <xash3dpp/imagelib/imagelib.hpp>
#include <xash3dpp/imagelib/image.hpp>
#include <xash3dpp/imagelib/pixel_format.hpp>
#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
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

    std::printf( "test_imagelib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
