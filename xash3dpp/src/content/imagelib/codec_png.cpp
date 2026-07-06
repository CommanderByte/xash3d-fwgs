// xash3dpp — imagelib PNG codec (decode + save)
// Legacy reference: engine/common/imagelib/img_png.c
//   Image_LoadPNG (decode: PNG file -> RGBA over miniz inflate + the PNG adaptive
//   filters), Image_SavePNG (save: RGB/RGBA -> None-filtered, best-compression
//   PNG). Byte-format frozen: engine/common/imagelib/img_png.h (png_t/png_ihdr_t).
//
// A hand-rolled chunk walk over miniz `mz_inflate`: verify the 8-byte signature
// and every chunk's CRC-32, accept the 8-bit GREY/RGB/RGBA/ALPHA/PALETTE colour
// types (reject other bit depths, Adam7 interlacing, non-zero compression/filter
// methods), resolve tRNS transparency, and expand everything to Rgba8 (the legacy
// "always extracted to 32-bit").
//
// Codecs are stateless (no global `image` scratch — boundary H-1) and parse
// through bounds-checked reads (modernization H-3/H-4): every source read is
// range-checked, so a truncated/hostile PNG yields an ImageError, never a fault
// (the legacy loop can over-read a short tRNS or a past-EOF chunk; this one cannot).
//
// @thread-safety: the codec instance is const/stateless — safe to share.

#include <xash3dpp/imagelib/save.hpp>
#include <xash3dpp/private/imagelib/codec.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <miniz.h>

namespace xash::imagelib {
namespace {

// ---- frozen PNG format constants (engine/common/imagelib/img_png.h) --------
constexpr std::size_t   k_png_header = 33;    // sizeof(png_t): 8 sig + 4 len + "IHDR" + 13 ihdr + 4 crc
constexpr std::uint32_t k_ihdr_len   = 13;    // sizeof(png_ihdr_t)
constexpr std::size_t   k_max_dim    = 8192;  // IMAGE_MAXWIDTH / IMAGE_MAXHEIGHT (Image_ValidSize)

// png_colortype — RGB is BIT(1); the palette/alpha bits layer on top of it, so
// (colortype & k_ct_rgb) tests "has colour" and (colortype & k_ct_alpha) tests
// "has an alpha sample" exactly as the legacy flag arithmetic does.
constexpr std::uint8_t k_ct_grey    = 0;
constexpr std::uint8_t k_ct_rgb     = 2;
constexpr std::uint8_t k_ct_palette = 3;
constexpr std::uint8_t k_ct_alpha   = 4;
constexpr std::uint8_t k_ct_rgba    = 6;

// png_filter — the per-scanline adaptive filter type byte.
constexpr std::uint8_t k_f_none    = 0;
constexpr std::uint8_t k_f_sub     = 1;
constexpr std::uint8_t k_f_up      = 2;
constexpr std::uint8_t k_f_average = 3;
constexpr std::uint8_t k_f_paeth   = 4;

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// PNG stores multi-byte lengths / dimensions / CRCs big-endian (legacy BigLong).
[[nodiscard]] std::uint32_t read_be32( std::span<const std::byte> s, std::size_t off ) noexcept
{
    return ( static_cast<std::uint32_t>( byte_at( s, off + 0 ) ) << 24 )
         | ( static_cast<std::uint32_t>( byte_at( s, off + 1 ) ) << 16 )
         | ( static_cast<std::uint32_t>( byte_at( s, off + 2 ) ) <<  8 )
         |   static_cast<std::uint32_t>( byte_at( s, off + 3 ) );
}

void put_be32( std::vector<std::byte> &v, std::uint32_t x )
{
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 24 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >> 16 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>( ( x >>  8 ) & 0xFF ) } );
    v.push_back( std::byte{ static_cast<std::uint8_t>(   x         & 0xFF ) } );
}

// CRC-32 (ISO 3309, the PNG/zlib polynomial) over s[off, off+len). Matches the
// legacy CRC32_Init/ProcessBuffer/Final path (the frozen IEND CRC 0xAE426082
// confirms the standard polynomial).
[[nodiscard]] std::uint32_t crc32_of( std::span<const std::byte> s, std::size_t off, std::size_t len ) noexcept
{
    // SAFETY: std::byte* -> const unsigned char* is permitted for object-
    // representation access; miniz reads exactly len caller-bounded bytes, no write.
    return static_cast<std::uint32_t>( mz_crc32(
        MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>( s.data() + off ), len ) );
}

// True if the four bytes at `off` equal the four-character chunk tag.
[[nodiscard]] bool tag_at( std::span<const std::byte> s, std::size_t off, const char ( &tag )[5] ) noexcept
{
    return byte_at( s, off + 0 ) == static_cast<std::uint8_t>( tag[0] )
        && byte_at( s, off + 1 ) == static_cast<std::uint8_t>( tag[1] )
        && byte_at( s, off + 2 ) == static_cast<std::uint8_t>( tag[2] )
        && byte_at( s, off + 3 ) == static_cast<std::uint8_t>( tag[3] );
}

// Append a full PNG chunk: 4-byte big-endian length, 4-byte tag, data, 4-byte CRC.
void append_chunk( std::vector<std::byte> &out, const char ( &tag )[5], std::span<const std::byte> data )
{
    put_be32( out, static_cast<std::uint32_t>( data.size() ) );
    const std::size_t crc_start = out.size();
    for( int i = 0; i < 4; ++i )
        out.push_back( std::byte{ static_cast<std::uint8_t>( tag[i] ) } );
    for( const std::byte b : data )
        out.push_back( b );
    put_be32( out, crc32_of( out, crc_start, out.size() - crc_start ) );
}

// ---- decode ---------------------------------------------------------------
// Parity with Image_LoadPNG. `name` carries no decode-affecting quirks for PNG
// (legacy uses it only for diagnostics + the global-dims Image_ValidSize check).
[[nodiscard]] Result<Image> decode_png( std::string_view /*name*/, std::span<const std::byte> file )
{
    // --- fixed 33-byte header: signature + IHDR chunk (len/type/data/crc) ---
    if( file.size() < k_png_header )
        return std::unexpected( ImageError::Truncated );

    static constexpr std::array<std::uint8_t, 8> sig = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };  // \x89 P N G \r \n \x1a \n
    for( std::size_t i = 0; i < sig.size(); ++i )
        if( byte_at( file, i ) != sig[i] )
            return std::unexpected( ImageError::BadHeader );

    if( read_be32( file, 8 ) != k_ihdr_len )   // IHDR length field must be 13
        return std::unexpected( ImageError::BadHeader );
    if( !tag_at( file, 12, "IHDR" ) )
        return std::unexpected( ImageError::BadHeader );

    const std::uint32_t width  = read_be32( file, 16 );
    const std::uint32_t height = read_be32( file, 20 );
    if( width == 0 || height == 0 )
        return std::unexpected( ImageError::BadHeader );
    if( width > k_max_dim || height > k_max_dim )
        return std::unexpected( ImageError::TooLarge );

    const std::uint8_t bitdepth  = byte_at( file, 24 );
    const std::uint8_t colortype = byte_at( file, 25 );
    const std::uint8_t compress  = byte_at( file, 26 );
    const std::uint8_t filtmeth  = byte_at( file, 27 );
    const std::uint8_t interlace = byte_at( file, 28 );

    // Legacy accepts only 8-bit depth, the five listed colour types, compression
    // method 0, filter method 0, and no interlacing (Adam7 explicitly rejected) —
    // all "valid PNG, unsupported variant" per the errors.hpp UnsupportedFeature.
    if( bitdepth != 8 )
        return std::unexpected( ImageError::UnsupportedFeature );
    if( colortype != k_ct_grey && colortype != k_ct_rgb && colortype != k_ct_palette
        && colortype != k_ct_alpha && colortype != k_ct_rgba )
        return std::unexpected( ImageError::UnsupportedFeature );
    if( compress != 0 || filtmeth != 0 || interlace != 0 )
        return std::unexpected( ImageError::UnsupportedFeature );

    // IHDR CRC covers the "IHDR" tag + its 13 data bytes (file[12, 29)).
    if( crc32_of( file, 12, 4 + k_ihdr_len ) != read_be32( file, 29 ) )
        return std::unexpected( ImageError::BadHeader );

    // --- walk the remaining chunks: gather IDAT, note PLTE/tRNS, need IEND ---
    std::size_t   pos = k_png_header;
    std::size_t   trns_off = 0, trns_len = 0;
    std::size_t   plte_off = 0, plte_len = 0;  // plte_len is an entry count (chunk_len / 3)
    bool          have_trns = false, have_plte = false, have_iend = false;
    std::uint32_t iend_len = 0;                 // legacy checks the last chunk_len == 0
    std::vector<std::uint8_t> idat;

    while( !have_iend && pos < file.size() )
    {
        if( pos + 4 > file.size() )
            return std::unexpected( ImageError::Truncated );
        const std::uint32_t chunk_len = read_be32( file, pos );
        iend_len = chunk_len;

        if( chunk_len > 0x7FFFFFFFu )              // legacy: chunk_len > INT_MAX
            return std::unexpected( ImageError::BadHeader );
        if( chunk_len > file.size() - pos )        // legacy: chunk size past file size
            return std::unexpected( ImageError::Truncated );

        const std::size_t type_off = pos + 4;
        const std::size_t data_off = pos + 8;
        // Bounds-check tag(4) + data(chunk_len) + crc(4). Legacy trusts these and
        // can over-read the final chunk; we return Truncated instead of faulting.
        if( data_off + static_cast<std::size_t>( chunk_len ) + 4 > file.size() )
            return std::unexpected( ImageError::Truncated );

        if( tag_at( file, type_off, "tRNS" ) )
        {
            have_trns = true;
            trns_off  = data_off;
            trns_len  = chunk_len;
        }
        else if( tag_at( file, type_off, "PLTE" ) )
        {
            have_plte = true;
            plte_off  = data_off;
            plte_len  = chunk_len / 3;
        }
        else if( tag_at( file, type_off, "IDAT" ) )
        {
            for( std::size_t k = 0; k < chunk_len; ++k )
                idat.push_back( byte_at( file, data_off + k ) );
        }
        else if( tag_at( file, type_off, "IEND" ) )
        {
            have_iend = true;
        }

        // Verify the chunk CRC over tag + data, then step past the CRC field.
        if( crc32_of( file, type_off, 4 + static_cast<std::size_t>( chunk_len ) )
            != read_be32( file, data_off + chunk_len ) )
            return std::unexpected( ImageError::BadHeader );

        pos = data_off + chunk_len + 4;
    }

    if( idat.empty() )                              // no IDAT chunks
        return std::unexpected( ImageError::BadHeader );
    if( colortype == k_ct_palette && !have_plte )   // indexed image needs a PLTE
        return std::unexpected( ImageError::BadHeader );
    if( !have_iend )                                 // stream ended before IEND
        return std::unexpected( ImageError::Truncated );
    if( iend_len != 0 )                              // IEND chunk must be empty
        return std::unexpected( ImageError::BadHeader );

    // --- decompress the concatenated IDAT stream (zlib-wrapped deflate) -----
    std::size_t pixel_size = 0;   // bytes per unfiltered sample tuple
    switch( colortype )
    {
    case k_ct_grey:
    case k_ct_palette: pixel_size = 1; break;
    case k_ct_alpha:   pixel_size = 2; break;
    case k_ct_rgb:     pixel_size = 3; break;
    default:           pixel_size = 4; break;  // k_ct_rgba
    }

    const std::size_t pixel_count = static_cast<std::size_t>( width ) * height;
    const std::size_t rowsize     = pixel_size * width;
    const std::size_t raw_size    = static_cast<std::size_t>( height ) * ( rowsize + 1 );  // +1 filter byte/row

    std::vector<std::uint8_t> inflated( raw_size );
    {
        mz_stream stream = {};
        stream.next_in   = idat.data();
        stream.avail_in  = static_cast<std::uint32_t>( idat.size() );  // mz_stream.avail_in is 32-bit
        stream.next_out  = inflated.data();
        stream.avail_out = static_cast<std::uint32_t>( raw_size );
        if( mz_inflateInit2( &stream, MZ_DEFAULT_WINDOW_BITS ) != MZ_OK )
            return std::unexpected( ImageError::BadHeader );
        const int ret = mz_inflate( &stream, MZ_NO_FLUSH );
        mz_inflateEnd( &stream );
        if( ret != MZ_OK && ret != MZ_STREAM_END )
            return std::unexpected( ImageError::BadHeader );
    }

    // --- reverse the PNG adaptive filters into packed scanlines -------------
    // Each inflated row is [filter byte][rowsize bytes]; reconstruct the original
    // samples into a tightly packed buffer. The prior row is all-zero for row 0,
    // which collapses Up->None, Sub/Paeth->Sub and Average->(left>>1) exactly as
    // the legacy special-cased first row does.
    std::vector<std::uint8_t> scan( pixel_count * pixel_size );
    for( std::size_t y = 0; y < height; ++y )
    {
        const std::size_t   in_row = y * ( rowsize + 1 );
        const std::uint8_t  ft     = inflated[in_row];
        const std::size_t   in     = in_row + 1;
        std::uint8_t       *cur    = scan.data() + y * rowsize;
        const std::uint8_t *prior  = ( y == 0 ) ? nullptr : scan.data() + ( y - 1 ) * rowsize;

        for( std::size_t i = 0; i < rowsize; ++i )
        {
            const std::uint8_t x = inflated[in + i];
            const int a = ( i >= pixel_size ) ? cur[i - pixel_size] : 0;             // Raw(left)
            const int b = prior ? prior[i] : 0;                                       // Prior(up)
            const int c = ( prior && i >= pixel_size ) ? prior[i - pixel_size] : 0;   // Prior(up-left)

            int recon = x;
            switch( ft )
            {
            case k_f_none:                                    break;
            case k_f_sub:     recon = x + a;                  break;
            case k_f_up:      recon = x + b;                  break;
            case k_f_average: recon = x + ( ( a + b ) >> 1 ); break;
            case k_f_paeth:
            {
                const int p  = a + b - c;
                int pa = p - a; if( pa < 0 ) pa = -pa;
                int pb = p - b; if( pb < 0 ) pb = -pb;
                int pc = p - c; if( pc < 0 ) pc = -pc;
                // Legacy tie-break: pick c only on a strict double-win, else b if
                // pb<pa, else a (this reproduces the exact PaethPredictor branch).
                const int pred = ( pc < pa && pc < pb ) ? c : ( pb < pa ) ? b : a;
                recon = x + pred;
                break;
            }
            default:
                return std::unexpected( ImageError::BadHeader );  // unknown filter type
            }
            cur[i] = static_cast<std::uint8_t>( recon );
        }
    }

    // --- content flags (parity with legacy) --------------------------------
    ImageFlags flags = ImageFlags::None;
    if( ( colortype & k_ct_rgb ) != 0 )                   // RGB / PALETTE / RGBA
        flags |= ImageFlags::HasColor;
    if( have_trns || ( colortype & k_ct_alpha ) != 0 )    // any tRNS, or ALPHA / RGBA
        flags |= ImageFlags::HasAlpha;

    // --- expand every colour type to Rgba8 ---------------------------------
    // Reads of the palette / tRNS tables go through `file` bounds-checked: for a
    // valid PNG these land inside the chunk; a malformed short table yields 0
    // rather than the legacy adjacent-memory over-read.
    auto file_byte = [&]( std::size_t base, std::size_t k ) -> std::uint8_t {
        const std::size_t off = base + k;
        return off < file.size() ? byte_at( file, off ) : std::uint8_t{ 0 };
    };

    std::vector<std::byte> rgba( pixel_count * 4 );
    switch( colortype )
    {
    case k_ct_rgb:
    {
        // tRNS holds a 16-bit-per-sample colour key; for 8-bit images the high
        // byte is zero, so the (possibly >255) key only ever matches an 8-bit
        // sample when that high byte is indeed zero — legacy compares the same way.
        std::uint32_t rk = 0, gk = 0, bk = 0;
        if( have_trns )
        {
            rk = ( static_cast<std::uint32_t>( file_byte( trns_off, 0 ) ) << 8 ) | file_byte( trns_off, 1 );
            gk = ( static_cast<std::uint32_t>( file_byte( trns_off, 2 ) ) << 8 ) | file_byte( trns_off, 3 );
            bk = ( static_cast<std::uint32_t>( file_byte( trns_off, 4 ) ) << 8 ) | file_byte( trns_off, 5 );
        }
        for( std::size_t p = 0; p < pixel_count; ++p )
        {
            const std::uint8_t r = scan[p * 3 + 0], g = scan[p * 3 + 1], b = scan[p * 3 + 2];
            rgba[p * 4 + 0] = std::byte{ r };
            rgba[p * 4 + 1] = std::byte{ g };
            rgba[p * 4 + 2] = std::byte{ b };
            const bool keyed = have_trns && rk == r && gk == g && bk == b;
            rgba[p * 4 + 3] = keyed ? std::byte{ 0 } : std::byte{ 0xFF };
        }
        break;
    }
    case k_ct_grey:
    {
        std::uint32_t rk = 0;
        if( have_trns )
            rk = ( static_cast<std::uint32_t>( file_byte( trns_off, 0 ) ) << 8 ) | file_byte( trns_off, 1 );
        for( std::size_t p = 0; p < pixel_count; ++p )
        {
            const std::uint8_t v = scan[p];
            rgba[p * 4 + 0] = std::byte{ v };
            rgba[p * 4 + 1] = std::byte{ v };
            rgba[p * 4 + 2] = std::byte{ v };
            const bool keyed = have_trns && rk == v;
            rgba[p * 4 + 3] = keyed ? std::byte{ 0 } : std::byte{ 0xFF };
        }
        break;
    }
    case k_ct_alpha:
    {
        for( std::size_t p = 0; p < pixel_count; ++p )
        {
            const std::uint8_t v = scan[p * 2 + 0], al = scan[p * 2 + 1];
            rgba[p * 4 + 0] = std::byte{ v };
            rgba[p * 4 + 1] = std::byte{ v };
            rgba[p * 4 + 2] = std::byte{ v };
            rgba[p * 4 + 3] = std::byte{ al };
        }
        break;
    }
    case k_ct_palette:
    {
        for( std::size_t p = 0; p < pixel_count; ++p )
        {
            const std::uint8_t idx = scan[p];
            if( idx < plte_len )
            {
                const std::size_t e = static_cast<std::size_t>( idx ) * 3;
                rgba[p * 4 + 0] = std::byte{ file_byte( plte_off, e + 0 ) };
                rgba[p * 4 + 1] = std::byte{ file_byte( plte_off, e + 1 ) };
                rgba[p * 4 + 2] = std::byte{ file_byte( plte_off, e + 2 ) };
                if( have_trns && idx < trns_len )
                    rgba[p * 4 + 3] = std::byte{ file_byte( trns_off, idx ) };
                else
                    rgba[p * 4 + 3] = std::byte{ 0xFF };
            }
            else  // out-of-range index: legacy emits opaque black
            {
                rgba[p * 4 + 0] = std::byte{ 0 };
                rgba[p * 4 + 1] = std::byte{ 0 };
                rgba[p * 4 + 2] = std::byte{ 0 };
                rgba[p * 4 + 3] = std::byte{ 0xFF };
            }
        }
        break;
    }
    default:  // k_ct_rgba — the reconstructed scanline is already R,G,B,A
    {
        for( std::size_t p = 0; p < pixel_count; ++p )
        {
            rgba[p * 4 + 0] = std::byte{ scan[p * 4 + 0] };
            rgba[p * 4 + 1] = std::byte{ scan[p * 4 + 1] };
            rgba[p * 4 + 2] = std::byte{ scan[p * 4 + 2] };
            rgba[p * 4 + 3] = std::byte{ scan[p * 4 + 3] };
        }
        break;
    }
    }

    return Image( static_cast<std::uint16_t>( width ), static_cast<std::uint16_t>( height ),
                  PixelFormat::Rgba8, std::move( rgba ), flags );
}

// ---- the registered codec -------------------------------------------------
class PngCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "png"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_png( name, file );
    }
};

} // namespace

const IImageCodec &png_codec() noexcept
{
    static const PngCodec codec;
    return codec;
}

// ---------------------------------------------------------------------------
// save_png — encode an Image as an 8-bit PNG (legacy Image_SavePNG). Colour type
// is RGB when the image has no alpha, RGBA when it carries HasAlpha; every row is
// filtered as None and the stream is deflated at best compression, exactly like
// the legacy writer. Source channel order comes from the format (BGR is swapped
// to RGB on the way out); output channel count comes from HasAlpha. Round-trips
// with decode_png. (The deflate bytes are not guaranteed identical to legacy —
// miniz and the legacy zlib may emit different but equivalent deflate streams.)
// ---------------------------------------------------------------------------
Result<std::vector<std::byte>> save_png( const Image &img )
{
    if( img.empty() )
        return std::unexpected( ImageError::Empty );

    std::size_t src_stride = 0;
    bool        src_bgr    = false;  // true when source channels are stored B,G,R
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

    const bool         has_alpha = img.has( ImageFlags::HasAlpha );
    const std::size_t  out_px    = has_alpha ? 4u : 3u;
    const std::uint8_t colortype = has_alpha ? k_ct_rgba : k_ct_rgb;

    // Filter every row as None (0), converting the source channel order to RGB[A].
    std::vector<std::uint8_t> filtered;
    filtered.reserve( h * ( 1 + w * out_px ) );
    for( std::size_t y = 0; y < h; ++y )
    {
        filtered.push_back( k_f_none );
        for( std::size_t x = 0; x < w; ++x )
        {
            const std::size_t  p  = ( y * w + x ) * src_stride;
            const std::uint8_t c0 = byte_at( src, p + 0 );
            const std::uint8_t c1 = byte_at( src, p + 1 );
            const std::uint8_t c2 = byte_at( src, p + 2 );
            if( src_bgr )
            {
                filtered.push_back( c2 ); filtered.push_back( c1 ); filtered.push_back( c0 );
            }
            else
            {
                filtered.push_back( c0 ); filtered.push_back( c1 ); filtered.push_back( c2 );
            }
            if( has_alpha )
                filtered.push_back( src_stride == 4 ? byte_at( src, p + 3 ) : std::uint8_t{ 0xFF } );
        }
    }

    // Deflate the filtered stream (zlib-wrapped, best compression like legacy).
    const mz_ulong bound = mz_compressBound( static_cast<mz_ulong>( filtered.size() ) );
    std::vector<unsigned char> comp( bound != 0 ? bound : 1 );
    mz_ulong comp_len = static_cast<mz_ulong>( comp.size() );
    if( mz_compress2( comp.data(), &comp_len, filtered.data(),
                      static_cast<mz_ulong>( filtered.size() ), MZ_BEST_COMPRESSION ) != MZ_OK )
        return std::unexpected( ImageError::OutOfMemory );

    // Assemble: signature + IHDR + IDAT + IEND (each chunk carries its CRC).
    std::vector<std::byte> out;
    static constexpr std::array<std::uint8_t, 8> sig = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    for( const std::uint8_t s : sig )
        out.push_back( std::byte{ s } );

    std::vector<std::byte> ihdr;
    put_be32( ihdr, static_cast<std::uint32_t>( w ) );
    put_be32( ihdr, static_cast<std::uint32_t>( h ) );
    ihdr.push_back( std::byte{ 8 } );          // bit depth
    ihdr.push_back( std::byte{ colortype } );  // colour type
    ihdr.push_back( std::byte{ 0 } );          // compression method
    ihdr.push_back( std::byte{ 0 } );          // filter method
    ihdr.push_back( std::byte{ 0 } );          // interlace method
    append_chunk( out, "IHDR", ihdr );

    std::vector<std::byte> idat;
    idat.reserve( static_cast<std::size_t>( comp_len ) );
    for( mz_ulong i = 0; i < comp_len; ++i )
        idat.push_back( std::byte{ comp[i] } );
    append_chunk( out, "IDAT", idat );

    append_chunk( out, "IEND", std::span<const std::byte>{} );

    return out;
}

} // namespace xash::imagelib
