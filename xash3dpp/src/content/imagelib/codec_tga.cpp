// xash3dpp — imagelib TGA codec (decode + save)
// Legacy reference: engine/common/imagelib/img_tga.c
//   Image_LoadTGA (decode: TGA file -> RGBA), Image_SaveTGA (save: RGBA/BGRA ->
//   uncompressed type-2 TGA). Supports image types 1/9 (8-bit colormapped),
//   2/10 (24/32-bit truecolour), 3/11 (8/16-bit greyscale), each with the RLE
//   (9/10/11) variants, and the attributes-bit-0x20 vertical-flip rule.
//
// Codecs are stateless (no global `image` scratch — boundary H-1) and parse
// through bounds-checked reads (modernization H-3/H-4): every source read is
// guarded, so a truncated/hostile file yields ImageError, never a fault. Output
// is always PF_RGBA_32 (Rgba8), matching the legacy "always extract to 32-bit".

#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/private/imagelib/codec.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::imagelib {
namespace {

// ---- frozen TGA format constants (img_tga.c tga_t) ------------------------
constexpr std::size_t k_tga_header = 18;    // tga_t on-disk size
constexpr std::size_t k_max_dim    = 8192;  // IMAGE_MAXWIDTH / IMAGE_MAXHEIGHT

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// ---- decode ---------------------------------------------------------------
// Parity with Image_LoadTGA. `name` carries no decode-affecting quirks for TGA
// (legacy uses it only for diagnostics + the global-dims ValidSize check).
[[nodiscard]] Result<Image> decode_tga( std::string_view /*name*/, std::span<const std::byte> file )
{
    namespace u = xash::utilities;

    if( file.size() < k_tga_header )
        return std::unexpected( ImageError::Truncated );

    const std::uint8_t id_length       = byte_at( file, 0 );
    // byte 1 = colormap_type — parsed-but-unused in legacy (never checked).
    const std::uint8_t image_type      = byte_at( file, 2 );
    const auto         colormap_index  = u::read_le<std::uint16_t>( file.data() + 3 );
    const auto         colormap_length = u::read_le<std::uint16_t>( file.data() + 5 );
    const std::uint8_t colormap_size   = byte_at( file, 7 );
    // bytes 8/10 = x/y origin — parsed-but-unused (legacy ignores them).
    const auto         width           = u::read_le<std::uint16_t>( file.data() + 12 );
    const auto         height          = u::read_le<std::uint16_t>( file.data() + 14 );
    const std::uint8_t pixel_size      = byte_at( file, 16 );
    const std::uint8_t attributes      = byte_at( file, 17 );

    // Image_ValidSize parity: dims must lie within (0, IMAGE_MAXWIDTH/HEIGHT].
    if( width == 0 || height == 0 )
        return std::unexpected( ImageError::BadHeader );
    if( width > k_max_dim || height > k_max_dim )
        return std::unexpected( ImageError::TooLarge );

    std::size_t pos = k_tga_header + static_cast<std::size_t>( id_length );  // skip image-id comment
    if( pos > file.size() )
        return std::unexpected( ImageError::Truncated );

    const bool colormapped = ( image_type == 1 || image_type == 9 );
    const bool truecolor   = ( image_type == 2 || image_type == 10 );
    const bool greyscale   = ( image_type == 3 || image_type == 11 );
    // Legacy silently zero-fills unknown image types; we reject them (a decode
    // that reads no source bytes and returns opaque black is not worth porting).
    if( !colormapped && !truecolor && !greyscale )
        return std::unexpected( ImageError::BadHeader );

    // Colormap (type 1/9): validate then read the palette (stored B,G,R[,A]).
    std::array<Rgba, 256> palette {};
    if( colormapped )
    {
        if( pixel_size != 8 )          return std::unexpected( ImageError::BadHeader );
        if( colormap_length != 256 )   return std::unexpected( ImageError::BadHeader );
        if( colormap_index != 0 )      return std::unexpected( ImageError::BadHeader );

        std::size_t entry_bytes = 0;
        if( colormap_size == 24 )      entry_bytes = 3;
        else if( colormap_size == 32 ) entry_bytes = 4;
        else                           return std::unexpected( ImageError::BadHeader );

        const std::size_t cmap_count = colormap_length;
        if( pos + cmap_count * entry_bytes > file.size() )
            return std::unexpected( ImageError::Truncated );
        for( std::size_t i = 0; i < cmap_count; ++i )
        {
            palette[i].b = byte_at( file, pos++ );
            palette[i].g = byte_at( file, pos++ );
            palette[i].r = byte_at( file, pos++ );
            palette[i].a = ( colormap_size == 32 ) ? byte_at( file, pos++ ) : std::uint8_t{ 255 };
        }
    }
    else if( truecolor )
    {
        if( pixel_size != 24 && pixel_size != 32 )
            return std::unexpected( ImageError::BadHeader );
    }
    else  // greyscale
    {
        if( pixel_size != 8 && pixel_size != 16 )
            return std::unexpected( ImageError::BadHeader );
    }

    const std::size_t columns = width;
    const std::size_t rows    = height;
    std::vector<std::byte> rgba( columns * rows * 4 );

    // Attributes bit 0x20 set => stored top-to-bottom (no flip); clear => stored
    // bottom-to-top (the common case), so file row R lands at output row
    // rows-1-R. Legacy also honoured the IL_DONTFLIP_TGA runtime override; that
    // global toggle is gone in the rewrite, so we always apply the flip rule.
    const bool top_to_bottom = ( attributes & 0x20u ) != 0;
    const bool compressed    = ( image_type == 9 || image_type == 10 || image_type == 11 );

    std::uint8_t red = 0, green = 0, blue = 0, alpha = 0;
    ImageFlags   flags = ImageFlags::None;

    std::size_t row = 0, col = 0;
    while( row < rows )
    {
        // Uncompressed: one contiguous run of up to 0x10000 fresh pixels. RLE:
        // a control byte selects a run packet (top bit set, 1 read reused) or a
        // raw packet (top bit clear, every pixel read), each 1 + (count & 0x7f).
        std::int32_t pixelcount     = 0x10000;
        std::int32_t readpixelcount = 0x10000;

        if( compressed )
        {
            if( pos >= file.size() )
                return std::unexpected( ImageError::Truncated );
            const std::uint8_t packet = byte_at( file, pos++ );
            if( ( packet & 0x80u ) != 0 )
                readpixelcount = 1;  // run-length packet
            pixelcount = 1 + ( packet & 0x7f );
        }

        while( pixelcount-- != 0 && row < rows )
        {
            if( readpixelcount-- > 0 )
            {
                if( colormapped )
                {
                    if( pos >= file.size() )
                        return std::unexpected( ImageError::Truncated );
                    const std::uint8_t idx = byte_at( file, pos++ );
                    if( idx < colormap_length )
                    {
                        red   = palette[idx].r;
                        green = palette[idx].g;
                        alpha = palette[idx].a;
                        blue  = palette[idx].b;
                        if( alpha != 255 )
                            flags |= ImageFlags::HasAlpha;
                    }
                    else
                    {
                        blue = idx;  // out-of-range index: legacy keeps stale rgb
                    }
                }
                else if( truecolor )
                {
                    const std::size_t need = ( pixel_size == 32 ) ? 4 : 3;
                    if( pos + need > file.size() )
                        return std::unexpected( ImageError::Truncated );
                    blue  = byte_at( file, pos++ );
                    green = byte_at( file, pos++ );
                    red   = byte_at( file, pos++ );
                    alpha = 255;
                    if( pixel_size == 32 )
                    {
                        alpha = byte_at( file, pos++ );
                        if( alpha != 255 )
                            flags |= ImageFlags::HasAlpha;
                    }
                }
                else  // greyscale
                {
                    const std::size_t need = ( pixel_size == 16 ) ? 2 : 1;
                    if( pos + need > file.size() )
                        return std::unexpected( ImageError::Truncated );
                    blue = green = red = byte_at( file, pos++ );
                    if( pixel_size == 16 )
                    {
                        alpha = byte_at( file, pos++ );
                        if( alpha != 255 )
                            flags |= ImageFlags::HasAlpha;
                    }
                    else
                    {
                        alpha = 255;
                    }
                }
            }

            if( red != green || green != blue )
                flags |= ImageFlags::HasColor;

            const std::size_t out_row = top_to_bottom ? row : ( rows - 1 - row );
            const std::size_t dst     = ( out_row * columns + col ) * 4;
            rgba[dst + 0] = std::byte{ red };
            rgba[dst + 1] = std::byte{ green };
            rgba[dst + 2] = std::byte{ blue };
            rgba[dst + 3] = std::byte{ alpha };

            if( ++col == columns )
            {
                ++row;
                col = 0;
            }
        }
    }

    return Image( width, height, PixelFormat::Rgba8, std::move( rgba ), flags );
}

// ---- the registered codec -------------------------------------------------
class TgaCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "tga"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_tga( name, file );
    }
};

} // namespace

const IImageCodec &tga_codec() noexcept
{
    static const TgaCodec codec;
    return codec;
}

// ---------------------------------------------------------------------------
// save_tga — encode an Image as an uncompressed type-2 TGA (legacy Image_SaveTGA).
// 24-bit when no alpha, 32-bit when HasAlpha; pixel data is written B,G,R[,A]
// and vertically flipped (bottom row first, attributes bit 0x20 left clear), so
// it round-trips with decode_tga. Source channel order comes from the format;
// the output channel count comes from the HasAlpha flag (legacy semantics).
// ---------------------------------------------------------------------------
Result<std::vector<std::byte>> save_tga( const Image &img )
{
    namespace u = xash::utilities;

    if( img.empty() )
        return std::unexpected( ImageError::Empty );

    std::size_t src_stride = 0;
    bool        src_bgr    = false;  // true when source channels are already B,G,R
    switch( img.format() )
    {
    case PixelFormat::Rgb8:  src_stride = 3; src_bgr = false; break;
    case PixelFormat::Bgr8:  src_stride = 3; src_bgr = true;  break;
    case PixelFormat::Rgba8: src_stride = 4; src_bgr = false; break;
    case PixelFormat::Bgra8: src_stride = 4; src_bgr = true;  break;
    default:
        return std::unexpected( ImageError::UnsupportedFeature );
    }

    const std::size_t w = img.width(), h = img.height();
    const std::span<const std::byte> src = img.pixels();
    if( src.size() < w * h * src_stride )
        return std::unexpected( ImageError::Truncated );

    const bool        has_alpha    = img.has( ImageFlags::HasAlpha );
    const std::string_view comment = "Generated by Xash ImageLib";

    std::vector<std::byte> out;
    auto put_byte = [&]( std::uint8_t v ) { out.push_back( std::byte{ v } ); };
    auto put_u16  = [&]( std::uint16_t v ) {
        std::byte t[2]; u::write_le<std::uint16_t>( t, v );
        out.push_back( t[0] ); out.push_back( t[1] );
    };

    // tga_t header (only width/height are meaningful multi-byte fields).
    put_byte( static_cast<std::uint8_t>( comment.size() ) );  // id_length
    put_byte( 0 );                                            // colormap_type
    put_byte( 2 );                                            // image_type (uncompressed truecolour)
    put_u16( 0 );                                             // colormap_index
    put_u16( 0 );                                             // colormap_length
    put_byte( 0 );                                            // colormap_size
    put_u16( 0 );                                             // x_origin
    put_u16( 0 );                                             // y_origin
    put_u16( static_cast<std::uint16_t>( w ) );
    put_u16( static_cast<std::uint16_t>( h ) );
    put_byte( has_alpha ? 32 : 24 );                          // pixel_size
    put_byte( has_alpha ? 8 : 0 );                            // attributes (8 = alpha bits)

    for( const char c : comment )
        put_byte( static_cast<std::uint8_t>( c ) );

    // Pixel data: vertical flip (bottom row first) + B,G,R[,A] channel order.
    for( std::size_t yy = 0; yy < h; ++yy )
    {
        const std::size_t y      = h - 1 - yy;
        const std::size_t rowoff = y * w * src_stride;
        for( std::size_t x = 0; x < w; ++x )
        {
            const std::size_t p  = rowoff + x * src_stride;
            const std::uint8_t c0 = byte_at( src, p + 0 );
            const std::uint8_t c1 = byte_at( src, p + 1 );
            const std::uint8_t c2 = byte_at( src, p + 2 );
            if( src_bgr )
            {
                put_byte( c0 ); put_byte( c1 ); put_byte( c2 );
            }
            else
            {
                put_byte( c2 ); put_byte( c1 ); put_byte( c0 );
            }
            if( has_alpha )
                put_byte( src_stride == 4 ? byte_at( src, p + 3 ) : std::uint8_t{ 255 } );
        }
    }

    return out;
}

} // namespace xash::imagelib
