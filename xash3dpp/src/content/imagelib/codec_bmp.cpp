// xash3dpp — imagelib BMP codec (Windows .BMP decode + save)
// Legacy reference: engine/common/imagelib/img_bmp.c
//   Image_LoadBMP (unpack: BMP file -> RGBA), Image_SaveBMP (pack: image ->
//   24/32-bit BMP). Byte-format frozen: engine/common/imagelib/img_bmp.h (bmp_t,
//   the 14-byte BITMAPFILEHEADER + 40-byte BITMAPINFOHEADER).
//
// Codecs are stateless (no global `image` scratch — boundary H-1) and parse
// through bounds-checked reads (modernization H-3/H-4): every file access is
// range-checked, so a malformed BMP yields an ImageError, never a fault (the
// legacy pixel loop can over-read; this one cannot). The legacy filename tricks
// (#XASH_SYSTEMFONT_001 qfont, #logo decal gradient, IL_OVERVIEW green-key,
// IL_KEEP_8BIT indexed passthrough) are intentionally dropped — this path always
// expands to Rgba8 (see the PARITY notes on the porting PR).
//
// @thread-safety: the codec instance is const/stateless — safe to share.

#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/private/imagelib/codec.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace xash::imagelib {
namespace {

// ---- frozen BMP format constants (engine/common/imagelib/img_bmp.h) --------
constexpr std::size_t   k_file_header  = 14;    // BI_FILE_HEADER_SIZE (BITMAPFILEHEADER)
constexpr std::size_t   k_bmp_struct   = 54;    // sizeof(bmp_t): 14 + 40-byte DIB header
constexpr std::size_t   k_info_header  = 40;    // BI_SIZE (BITMAPINFOHEADER) — save writes this
constexpr std::uint32_t k_bi_rgb       = 0;     // uncompressed RGB
constexpr std::uint32_t k_bi_bitfields = 3;     // 32-bpp channel masks (accepted; decoded as RGB)
constexpr std::int64_t  k_max_dim      = 8192;  // IMAGE_MAXWIDTH/HEIGHT (legacy Image_ValidSize)

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// ---- unpack ---------------------------------------------------------------
// Parity with Image_LoadBMP: decode a Windows BMP (1/4/8/16/24/32-bpp, BI_RGB or
// the 32-bpp BI_BITFIELDS special case) to a top-down Rgba8 image. 8- and 4-bit
// index through the BMP palette (stored B,G,R,reserved — reversed to R,G,B and
// alpha=reserved, matching legacy); 1-bit is black/white with alpha 0; 16-bit is
// X1R5G5B5 (red low, blue high); 24/32-bit are BGR(A) reversed to RGBA.
[[nodiscard]] Result<Image> decode_bmp( std::string_view /*name*/, std::span<const std::byte> file )
{
    namespace u = xash::utilities;

    if( file.size() < k_bmp_struct )
        return std::unexpected( ImageError::Truncated );

    // ---- fixed 54-byte header (bmp_t), little-endian ----
    if( byte_at( file, 0 ) != std::uint8_t{ 'B' } || byte_at( file, 1 ) != std::uint8_t{ 'M' } )
        return std::unexpected( ImageError::BadHeader );

    const auto reserved0      = u::read_le<std::uint32_t>( file.data() + 6 );
    const auto header_size    = u::read_le<std::uint32_t>( file.data() + 14 );
    const auto width_signed   = u::read_le<std::int32_t>( file.data() + 18 );
    const auto height_signed  = u::read_le<std::int32_t>( file.data() + 22 );
    const auto planes         = u::read_le<std::uint16_t>( file.data() + 26 );
    const auto bits_per_pixel = u::read_le<std::uint16_t>( file.data() + 28 );
    const auto compression    = u::read_le<std::uint32_t>( file.data() + 30 );
    const auto colors_field   = u::read_le<std::uint32_t>( file.data() + 46 );

    // Bogus-header checks (legacy order): reserved words and plane count first,
    // then the "BM" magic (already checked), then the DIB header size.
    if( reserved0 != 0 )
        return std::unexpected( ImageError::BadHeader );
    if( planes != 1 )
        return std::unexpected( ImageError::BadHeader );
    if( !( header_size == 40 || header_size == 108 || header_size == 124 ) )
        return std::unexpected( ImageError::BadHeader );

    // Sweet Half-Life quirk: the bfSize field (file[2..6]) may disagree with the
    // real byte count (splash.bmp). Legacy only warns, so we never read/validate
    // it — a size mismatch must NOT reject the file.

    // Only uncompressed BI_RGB, plus the 32-bpp BI_BITFIELDS special case.
    if( compression != k_bi_rgb )
    {
        if( bits_per_pixel != 32 || compression != k_bi_bitfields )
            return std::unexpected( ImageError::UnsupportedFeature );
    }

    switch( bits_per_pixel )
    {
    case 1: case 4: case 8: case 16: case 24: case 32: break;
    default: return std::unexpected( ImageError::BadHeader );
    }

    // width is used verbatim; height's sign selects top-down storage but legacy
    // only takes abs() and always flips (see the row loop) — reproduced here.
    const std::int64_t columns = static_cast<std::int64_t>( width_signed );
    const std::int64_t rows    = ( height_signed < 0 )
        ? -static_cast<std::int64_t>( height_signed )
        :  static_cast<std::int64_t>( height_signed );
    if( columns <= 0 || rows <= 0 || columns > k_max_dim || rows > k_max_dim )
        return std::unexpected( ImageError::BadHeader );  // legacy Image_ValidSize

    // ---- palette (<= 8-bpp): cbPalBytes RGBQUAD entries (B,G,R,reserved) ----
    std::array<std::array<std::uint8_t, 4>, 256> palette{};  // zero-initialised
    std::size_t cb_pal_bytes = 0;
    if( bits_per_pixel <= 8 )
    {
        std::uint32_t colors = colors_field;
        if( colors == 0 )
            cb_pal_bytes = ( std::size_t{ 1 } << bits_per_pixel ) * 4u;  // full table
        else
        {
            if( colors > 256 ) colors = 256;                            // clamp (legacy warns)
            cb_pal_bytes = static_cast<std::size_t>( colors ) * 4u;
        }
    }

    // Pixel/palette data begins right after the DIB header. Legacy computes the
    // start from 14 + bitmapHeaderSize (it ignores bfOffBits) — reproduced.
    const std::size_t data_start = k_file_header + header_size;
    if( file.size() < data_start + cb_pal_bytes )
        return std::unexpected( ImageError::Truncated );  // palette OOB

    for( std::size_t i = 0; i < cb_pal_bytes / 4; ++i )
    {
        const std::size_t o = data_start + i * 4;
        palette[i][0] = byte_at( file, o + 0 );  // blue
        palette[i][1] = byte_at( file, o + 1 );  // green
        palette[i][2] = byte_at( file, o + 2 );  // red
        palette[i][3] = byte_at( file, o + 3 );  // reserved -> alpha (legacy verbatim)
    }

    std::size_t pos = data_start + cb_pal_bytes;  // first pixel byte

    // Row padding to a 4-byte boundary — legacy per-bpp formulas (img_bmp.c).
    const std::int64_t w = columns;
    std::size_t pad = 0;
    switch( bits_per_pixel )
    {
    case 1:  pad = static_cast<std::size_t>( ( ( 32 - ( w % 32 ) ) / 8 ) % 4 ); break;
    case 4:  pad = static_cast<std::size_t>( ( ( 8 - ( w % 8 ) ) / 2 ) % 4 ); break;
    case 16: pad = static_cast<std::size_t>( ( 4 - ( ( w * 2 ) % 4 ) ) % 4 ); break;
    case 8:
    case 24: pad = static_cast<std::size_t>( ( 4 - ( ( w * ( bits_per_pixel / 8 ) ) % 4 ) ) % 4 ); break;
    default: pad = 0; break;  // 32-bpp rows are already 4-aligned
    }

    // Legacy pixel-size pre-check. NOTE: (bpp>>3) is 0 for 1/4-bpp, so this only
    // guards 8/16/24/32; the per-read bounds checks below cover every format.
    const std::int64_t bytes_floor = bits_per_pixel / 8;  // 0 for 1/4-bpp
    if( file.size() < pos + static_cast<std::size_t>( columns * rows * bytes_floor ) )
        return std::unexpected( ImageError::Truncated );  // pixels OOB

    const std::size_t out_w = static_cast<std::size_t>( columns );
    const std::size_t out_h = static_cast<std::size_t>( rows );
    std::vector<std::byte> rgba( out_w * out_h * 4 );

    ImageFlags    flags = ImageFlags::None;
    std::uint64_t refl[3] = { 0, 0, 0 };

    // BMP scanlines are stored bottom-up; write them into output rows (rows-1..0)
    // so the result is top-down. Legacy flips unconditionally — even for the
    // negative-height "top-down" form — and this reproduces that exactly.
    for( std::int64_t row = rows - 1; row >= 0; --row )
    {
        std::size_t di = static_cast<std::size_t>( row ) * out_w * 4u;

        for( std::int64_t column = 0; column < columns; ++column )
        {
            std::uint8_t red = 0, green = 0, blue = 0;

            switch( bits_per_pixel )
            {
            case 1:
            {
                if( pos >= file.size() ) return std::unexpected( ImageError::Truncated );
                const std::uint8_t bits = byte_at( file, pos++ );
                --column;  // the inner bit loop drives `column`
                for( int c = 0, k = 128; c < 8; ++c, k >>= 1 )
                {
                    if( ++column >= columns ) break;
                    red = green = blue = ( ( bits & k ) != 0 ) ? 0xFF : 0x00;
                    rgba[di++] = std::byte{ red };
                    rgba[di++] = std::byte{ green };
                    rgba[di++] = std::byte{ blue };
                    rgba[di++] = std::byte{ 0x00 };
                }
                break;
            }
            case 4:
            {
                if( pos >= file.size() ) return std::unexpected( ImageError::Truncated );
                const std::uint8_t twain = byte_at( file, pos++ );
                std::size_t idx = static_cast<std::size_t>( twain >> 4 );
                red   = palette[idx][2];
                green = palette[idx][1];
                blue  = palette[idx][0];
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ palette[idx][3] };
                if( ++column == columns ) break;
                idx   = static_cast<std::size_t>( twain & 0x0F );
                red   = palette[idx][2];
                green = palette[idx][1];
                blue  = palette[idx][0];
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ palette[idx][3] };
                break;
            }
            case 8:
            {
                if( pos >= file.size() ) return std::unexpected( ImageError::Truncated );
                const std::size_t idx = static_cast<std::size_t>( byte_at( file, pos++ ) );
                red   = palette[idx][2];
                green = palette[idx][1];
                blue  = palette[idx][0];
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ palette[idx][3] };
                break;
            }
            case 16:
            {
                if( pos + 2 > file.size() ) return std::unexpected( ImageError::Truncated );
                const std::uint16_t s = static_cast<std::uint16_t>(
                    byte_at( file, pos ) | ( static_cast<std::uint16_t>( byte_at( file, pos + 1 ) ) << 8 ) );
                pos += 2;
                blue  = static_cast<std::uint8_t>( ( s & ( 31u << 10 ) ) >> 7 );
                green = static_cast<std::uint8_t>( ( s & ( 31u << 5 ) ) >> 2 );
                red   = static_cast<std::uint8_t>( ( s & 31u ) << 3 );
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ 0xFF };
                break;
            }
            case 24:
            {
                if( pos + 3 > file.size() ) return std::unexpected( ImageError::Truncated );
                blue  = byte_at( file, pos++ );
                green = byte_at( file, pos++ );
                red   = byte_at( file, pos++ );
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ 0xFF };
                break;
            }
            case 32:
            {
                if( pos + 4 > file.size() ) return std::unexpected( ImageError::Truncated );
                blue  = byte_at( file, pos++ );
                green = byte_at( file, pos++ );
                red   = byte_at( file, pos++ );
                const std::uint8_t alpha = byte_at( file, pos++ );
                rgba[di++] = std::byte{ red };
                rgba[di++] = std::byte{ green };
                rgba[di++] = std::byte{ blue };
                rgba[di++] = std::byte{ alpha };
                if( alpha != 255 ) flags |= ImageFlags::HasAlpha;
                break;
            }
            default:
                return std::unexpected( ImageError::BadHeader );
            }

            // Content flags + reflectivity accumulate once per outer iteration on
            // the last-written texel (legacy quirk: sub-byte formats undersample).
            if( red != green || green != blue )
                flags |= ImageFlags::HasColor;
            refl[0] += red;
            refl[1] += green;
            refl[2] += blue;
        }

        pos += pad;  // skip row padding (only nonzero for < 32-bpp)
    }

    // Average colour -> fog params (legacy reflectivity). The rewrite's fog_params
    // is Rgba (8-bit), so this is the integer average, not the legacy float vec3.
    const std::uint64_t npix = static_cast<std::uint64_t>( out_w ) * out_h;
    const Rgba fog{
        static_cast<std::uint8_t>( refl[0] / npix ),
        static_cast<std::uint8_t>( refl[1] / npix ),
        static_cast<std::uint8_t>( refl[2] / npix ),
        255 };

    Image img( static_cast<std::uint16_t>( out_w ), static_cast<std::uint16_t>( out_h ),
               PixelFormat::Rgba8, std::move( rgba ), flags );
    img.set_fog_params( fog );
    return img;
}

// ---- the registered codec -------------------------------------------------
class BmpCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "bmp"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_bmp( name, file );
    }
};

} // namespace

const IImageCodec &bmp_codec() noexcept
{
    static const BmpCodec codec;
    return codec;
}

// ---------------------------------------------------------------------------
// save_bmp — encode an image as an uncompressed Windows BMP (legacy Image_SaveBMP).
// 8-bit indexed (+ palette), 24-bit (RGB/BGR) and 32-bit (RGBA/BGRA) are written
// with the row width padded up to a multiple of 4 (biTrueWidth). RGBA/RGB sources
// are reversed to on-disk BGR(A); BGRA/BGR sources are already in BMP order
// (legacy ImageBigEndian). Round-trips with decode() on a ".bmp" name.
// ---------------------------------------------------------------------------
Result<std::vector<std::byte>> save_bmp( const Image &img )
{
    namespace u = xash::utilities;

    if( img.empty() )
        return std::unexpected( ImageError::Empty );

    // pixel_size and channel order per source format (legacy switch on pix->type).
    std::size_t pixel_size = 0;
    bool        reverse_rb = false;  // true => reverse R/B into BGR on disk
    switch( img.format() )
    {
    case PixelFormat::Indexed8:                          pixel_size = 1; break;
    case PixelFormat::Rgb8:   reverse_rb = true;         [[fallthrough]];
    case PixelFormat::Bgr8:                              pixel_size = 3; break;
    case PixelFormat::Rgba8:  reverse_rb = true;         [[fallthrough]];
    case PixelFormat::Bgra8:                             pixel_size = 4; break;
    default: return std::unexpected( ImageError::UnsupportedFeature );
    }
    if( pixel_size == 1 && !img.palette().has_value() )
        return std::unexpected( ImageError::UnsupportedFeature );

    const std::size_t width  = img.width();
    const std::size_t height = img.height();
    const std::span<const std::byte> src = img.pixels();
    if( src.size() < width * height * pixel_size )
        return std::unexpected( ImageError::Truncated );

    const std::size_t true_width  = ( width + 3 ) & ~std::size_t{ 3 };  // pad to multiple of 4
    const std::size_t cb_bmp_bits = true_width * height * pixel_size;
    const std::size_t cb_pal      = ( pixel_size == 1 ) ? 256u * 4u : 0u;

    std::vector<std::byte> out;
    out.reserve( k_bmp_struct + cb_pal + cb_bmp_bits );

    auto put_u16 = [&]( std::uint16_t v ) {
        std::byte t[2]; u::write_le<std::uint16_t>( t, v );
        out.push_back( t[0] ); out.push_back( t[1] );
    };
    auto put_u32 = [&]( std::uint32_t v ) {
        std::byte t[4]; u::write_le<std::uint32_t>( t, v );
        for( const std::byte b : t ) out.push_back( b );
    };
    auto put_byte = [&]( std::uint8_t v ) { out.push_back( std::byte{ v } ); };

    // BITMAPFILEHEADER + BITMAPINFOHEADER (bmp_t, 54 bytes, little-endian).
    put_byte( 'B' ); put_byte( 'M' );
    put_u32( static_cast<std::uint32_t>( k_bmp_struct + cb_bmp_bits + cb_pal ) ); // bfSize
    put_u32( 0u );                                                                 // reserved
    put_u32( static_cast<std::uint32_t>( k_bmp_struct + cb_pal ) );                // bfOffBits
    put_u32( static_cast<std::uint32_t>( k_info_header ) );                        // biSize
    put_u32( static_cast<std::uint32_t>( true_width ) );                          // biWidth (padded)
    put_u32( static_cast<std::uint32_t>( height ) );                              // biHeight
    put_u16( 1u );                                                                 // biPlanes
    put_u16( static_cast<std::uint16_t>( pixel_size * 8 ) );                       // biBitCount
    put_u32( k_bi_rgb );                                                           // biCompression
    put_u32( static_cast<std::uint32_t>( cb_bmp_bits ) );                          // biSizeImage
    put_u32( 0u );                                                                 // biXPelsPerMeter
    put_u32( 0u );                                                                 // biYPelsPerMeter
    put_u32( static_cast<std::uint32_t>( pixel_size == 1 ? 256u : 0u ) );          // biClrUsed
    put_u32( 0u );                                                                 // biClrImportant

    // Palette (indexed only): 256 RGBQUAD entries, on disk B,G,R,reserved. The
    // reserved byte carries alpha iff the palette is flagged 32-bit (legacy).
    if( pixel_size == 1 )
    {
        const Palette &pal = *img.palette();
        const bool store_alpha = pal.has_alpha();
        for( int i = 0; i < 256; ++i )
        {
            const Rgba c = pal[static_cast<std::size_t>( i )];
            put_byte( c.b );
            put_byte( c.g );
            put_byte( c.r );
            put_byte( store_alpha ? c.a : 0 );
        }
    }

    // Pixel rows, bottom-up, each padded to true_width. The padding texels stay
    // zero (out is grown with value-initialised bytes).
    const std::size_t bits_base = out.size();
    out.resize( bits_base + cb_bmp_bits );  // zero-filled
    for( std::size_t y = 0; y < height; ++y )
    {
        const std::size_t dst_row = bits_base + ( height - 1 - y ) * true_width * pixel_size;
        const std::size_t src_row = y * width * pixel_size;
        for( std::size_t x = 0; x < width; ++x )
        {
            const std::size_t s = src_row + x * pixel_size;
            const std::size_t d = dst_row + x * pixel_size;
            if( pixel_size == 1 )
            {
                out[d] = src[s];  // index byte, verbatim
            }
            else
            {
                out[d + 0] = reverse_rb ? src[s + 2] : src[s + 0];  // blue
                out[d + 1] = src[s + 1];                            // green
                out[d + 2] = reverse_rb ? src[s + 0] : src[s + 2];  // red
                if( pixel_size == 4 )
                    out[d + 3] = src[s + 3];                        // alpha
            }
        }
    }

    return out;
}

} // namespace xash::imagelib
