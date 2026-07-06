// xash3dpp — imagelib WAD3 codec (unpack + pack)
// Legacy reference: engine/common/imagelib/img_wad.c
//   Image_LoadWAD (unpack: WAD3 file -> RGBA), Image_SaveWAD (pack: indexed
//   spray -> single-lump WAD3), Image_GenerateMipmaps (point-sample decimate).
// Byte-format frozen: common/wadfile.h (dwadinfo_t/dlumpinfo_t/mip_t).
//
// This is the "WAD texture pack/unpack" chunk deliverable. Codecs are stateless
// (no global scratch — boundary H-1) and parse through bounds-checked read_le
// (modernization H-3/H-4).

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

// ---- frozen WAD3 format constants (common/wadfile.h) ----------------------
constexpr std::int32_t k_wad3_ident = ( '3' << 24 ) | ( 'D' << 16 ) | ( 'A' << 8 ) | 'W'; // "WAD3" LE
constexpr std::int8_t  k_typ_palette = 64;
constexpr std::int8_t  k_typ_miptex  = 67;
constexpr std::size_t  k_mip_header  = 40;  // mip_t: name[16] + w + h + offsets[4]
constexpr std::size_t  k_wad_header  = 12;  // dwadinfo_t: ident + numlumps + infotableofs
constexpr std::size_t  k_lump_rec    = 32;  // dlumpinfo_t
constexpr std::size_t  k_max_dim     = 256; // WorldCraft mip cap

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// ---- unpack ---------------------------------------------------------------
// Parity with Image_LoadWAD: return the first decodable TYP_MIPTEX/TYP_PALETTE
// lump expanded to RGBA. Index 255 is transparent for classic miptex; a
// TYP_PALETTE lump becomes a soft-alpha gradient.
[[nodiscard]] Result<Image> decode_wad( std::string_view /*name*/, std::span<const std::byte> file )
{
    using u32 = std::uint32_t;
    namespace u = xash::utilities;

    if( file.size() < k_wad_header )
        return std::unexpected( ImageError::Truncated );

    const auto numlumps     = u::read_le<std::int32_t>( file.data() + 4 );
    const auto infotableofs = u::read_le<std::int32_t>( file.data() + 8 );
    if( numlumps <= 0 || infotableofs <= 0 || static_cast<std::size_t>( infotableofs ) >= file.size() )
        return std::unexpected( ImageError::BadHeader );

    for( std::int32_t i = 0; i < numlumps; ++i )
    {
        const std::size_t rec = static_cast<std::size_t>( infotableofs ) + static_cast<std::size_t>( i ) * k_lump_rec;
        if( rec + k_lump_rec > file.size() )
            break;

        const auto filepos  = u::read_le<std::int32_t>( file.data() + rec + 0 );
        const auto disksize = u::read_le<std::int32_t>( file.data() + rec + 4 );
        const auto type     = static_cast<std::int8_t>( byte_at( file, rec + 12 ) );

        if( type != k_typ_miptex && type != k_typ_palette )
            continue;
        if( filepos < 0 || disksize < 0 )
            continue;

        const std::size_t mip_off = static_cast<std::size_t>( filepos );
        const std::size_t mip_sz  = static_cast<std::size_t>( disksize );
        if( mip_sz < k_mip_header || mip_off + mip_sz > file.size() )
            continue;

        const std::span<const std::byte> mip = file.subspan( mip_off, mip_sz );
        const auto width   = u::read_le<u32>( mip.data() + 16 );
        const auto height  = u::read_le<u32>( mip.data() + 20 );
        const auto offset0 = u::read_le<u32>( mip.data() + 24 );
        if( width == 0 || height == 0 || width > k_max_dim || height > k_max_dim )
            continue;

        const std::size_t m0 = static_cast<std::size_t>( width ) * height;
        if( offset0 == 0 || offset0 + m0 > mip_sz )
            continue;

        // Palette sits after all four mip levels and the 2-byte colour count.
        const std::size_t m1 = m0 / 4, m2 = m0 / 16, m3 = m0 / 64;
        const std::size_t pal_off = k_mip_header + m0 + m1 + m2 + m3 + 2;
        if( pal_off + 256u * 3u > mip_sz )
            continue;  // bounds-checked (H-4; legacy trusts the lump)

        const std::size_t px_off = offset0;

        // TYP_PALETTE => soft-alpha gradient from the front (index 255) colour.
        std::array<std::uint8_t, 256 * 3> grad {};
        bool alpha_mode = false;
        auto pal_rgb = [&]( std::size_t idx, std::size_t c ) -> std::uint8_t {
            return byte_at( mip, pal_off + idx * 3 + c );
        };
        if( type == k_typ_palette )
        {
            const std::uint8_t fr = pal_rgb( 255, 0 ), fg = pal_rgb( 255, 1 ), fb = pal_rgb( 255, 2 );
            for( int j = 0; j < 256; ++j )
            {
                const float t = static_cast<float>( j ) / 255.0f;
                grad[j * 3 + 0] = static_cast<std::uint8_t>( fr * t );
                grad[j * 3 + 1] = static_cast<std::uint8_t>( fg * t );
                grad[j * 3 + 2] = static_cast<std::uint8_t>( fb * t );
            }
            alpha_mode = true;
        }

        std::vector<std::byte> rgba( m0 * 4 );
        for( std::size_t j = 0; j < m0; ++j )
        {
            const std::uint8_t idx = byte_at( mip, px_off + j );
            const std::uint8_t r = alpha_mode ? grad[idx * 3 + 0] : pal_rgb( idx, 0 );
            const std::uint8_t g = alpha_mode ? grad[idx * 3 + 1] : pal_rgb( idx, 1 );
            const std::uint8_t b = alpha_mode ? grad[idx * 3 + 2] : pal_rgb( idx, 2 );
            const std::uint8_t a = alpha_mode ? idx : ( idx == 255 ? 0 : 255 );
            rgba[j * 4 + 0] = std::byte{ r };
            rgba[j * 4 + 1] = std::byte{ g };
            rgba[j * 4 + 2] = std::byte{ b };
            rgba[j * 4 + 3] = std::byte{ a };
        }

        return Image( static_cast<std::uint16_t>( width ), static_cast<std::uint16_t>( height ),
                      PixelFormat::Rgba8, std::move( rgba ),
                      ImageFlags::HasColor | ImageFlags::HasAlpha );
    }

    return std::unexpected( ImageError::UnknownFormat );
}

// ---- the registered codec -------------------------------------------------
class WadCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "wad"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_wad( name, file );
    }
};

} // namespace

const IImageCodec &wad_codec() noexcept
{
    static const WadCodec codec;
    return codec;
}

// ---------------------------------------------------------------------------
// save_wad — pack an indexed image into a single-lump WAD3 (legacy Image_SaveWAD).
// ---------------------------------------------------------------------------
Result<std::vector<std::byte>> save_wad( const Image &img )
{
    namespace u = xash::utilities;

    if( img.empty() )
        return std::unexpected( ImageError::Empty );
    if( img.format() != PixelFormat::Indexed8 || !img.palette().has_value() )
        return std::unexpected( ImageError::UnsupportedFeature );

    const std::size_t w = img.width(), h = img.height();
    const std::size_t m0 = w * h, m1 = m0 / 4, m2 = m0 / 16, m3 = m0 / 64;
    const std::span<const std::byte> src = img.pixels();
    if( src.size() < m0 )
        return std::unexpected( ImageError::Truncated );

    const bool gradient = img.has( ImageFlags::ColorIndex );
    const std::int8_t lump_type = gradient ? k_typ_palette : k_typ_miptex;

    std::vector<std::byte> out;
    auto put_u32 = [&]( std::uint32_t v ) {
        std::byte t[4]; u::write_le<std::uint32_t>( t, v );
        for( const std::byte b : t ) out.push_back( b );
    };
    auto put_u16 = [&]( std::uint16_t v ) {
        std::byte t[2]; u::write_le<std::uint16_t>( t, v );
        for( const std::byte b : t ) out.push_back( b );
    };
    auto put_name = [&]( std::string_view s, std::size_t field ) {
        for( std::size_t i = 0; i < field; ++i )
            out.push_back( i < s.size() ? std::byte{ static_cast<std::uint8_t>( s[i] ) } : std::byte{ 0 } );
    };
    auto put_byte = [&]( std::uint8_t v ) { out.push_back( std::byte{ v } ); };

    // dwadinfo_t: ident, numlumps=1, infotableofs (patched at the end).
    put_u32( static_cast<std::uint32_t>( k_wad3_ident ) );
    put_u32( 1u );
    const std::size_t infotableofs_pos = out.size();
    put_u32( 0u );  // placeholder

    // mip_t header.
    const std::uint32_t o0 = static_cast<std::uint32_t>( k_mip_header );
    const std::uint32_t o1 = o0 + static_cast<std::uint32_t>( m0 );
    const std::uint32_t o2 = o1 + static_cast<std::uint32_t>( m1 );
    const std::uint32_t o3 = o2 + static_cast<std::uint32_t>( m2 );
    put_name( "{LOGO", 16 );
    put_u32( static_cast<std::uint32_t>( w ) );
    put_u32( static_cast<std::uint32_t>( h ) );
    put_u32( o0 ); put_u32( o1 ); put_u32( o2 ); put_u32( o3 );

    // mip level 0 (verbatim), then point-sampled mips 1..3 (Image_GenerateMipmaps).
    for( std::size_t j = 0; j < m0; ++j )
        out.push_back( src[j] );
    for( int lvl = 1; lvl <= 3; ++lvl )
    {
        const std::size_t mw = w >> lvl, mh = h >> lvl, step = std::size_t{ 1 } << lvl;
        for( std::size_t y = 0; y < mh; ++y )
            for( std::size_t x = 0; x < mw; ++x )
                out.push_back( src[( y * step ) * w + ( x * step )] );
    }

    // palette: 2-byte count (256) then 256 RGB triples.
    put_u16( 256 );
    const Palette &pal = *img.palette();
    if( gradient )
    {
        const Rgba front = pal[255];
        for( int i = 0; i < 256; ++i )
        {
            const float t = static_cast<float>( i ) / 255.0f;
            put_byte( static_cast<std::uint8_t>( front.r * t ) );
            put_byte( static_cast<std::uint8_t>( front.g * t ) );
            put_byte( static_cast<std::uint8_t>( front.b * t ) );
        }
    }
    else
    {
        for( int i = 0; i < 256; ++i )
        {
            const Rgba c = pal[static_cast<std::size_t>( i )];
            put_byte( c.r ); put_byte( c.g ); put_byte( c.b );
        }
    }

    // pad to a multiple of 4.
    while( out.size() % 4 != 0 )
        out.push_back( std::byte{ 0 } );

    // dlumpinfo_t.
    const std::uint32_t infotableofs = static_cast<std::uint32_t>( out.size() );
    const std::uint32_t disksize = o3 + static_cast<std::uint32_t>( m3 ) + 2u + 256u * 3u;
    put_u32( k_wad_header );   // filepos (just past the header)
    put_u32( disksize );        // disksize
    put_u32( disksize );        // size
    put_byte( static_cast<std::uint8_t>( lump_type ) );
    put_byte( 0 );              // attribs
    put_byte( 0 ); put_byte( 0 ); // pad0, pad1
    put_name( "tempdecal", 16 );

    // patch infotableofs.
    u::write_le<std::uint32_t>( out.data() + infotableofs_pos, infotableofs );

    return out;
}

} // namespace xash::imagelib
