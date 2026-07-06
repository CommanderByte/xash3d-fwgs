// xash3dpp — imagelib DDS codec (decode; kept GPU-compressed)
// Legacy reference: engine/common/imagelib/img_dds.c
//   Image_LoadDDS (parse header, map FourCC/DXGI -> pixel format, keep the raw
//   block bytes — the renderer decodes them on the GPU) + Image_DXTGetPixelFormat
//   + the DXT3/DXT5 alpha-block scanners (Image_CheckDXT3Alpha/Image_CheckDXT5Alpha).
// Byte-format frozen: engine/common/imagelib/img_dds.h (dds_t / dds_pixf_t /
//   dds_caps_t / dds_header_dxt10_t) + common/com_image.h (DXT_ENCODE_*).
//
// Deliberate parity divergence: legacy silently rejects compressed formats unless
// the IL_DDS_HARDWARE global flag is set. The rewrite has no such global — the
// renderer advertises hardware support at the Chunk-13 seam — so compressed DDS
// is ALWAYS accepted here. The legacy `image` global scratch and the decal
// reflectivity (dwReserved1[1] -> fogParams) are intentionally not reproduced.
//
// Codecs are stateless (no global scratch — boundary H-1) and bounds-check every
// read (modernization H-4): a malformed file yields an ImageError, never a fault.

#include <xash3dpp/private/imagelib/codec.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::imagelib {
namespace {

using u32 = std::uint32_t;

// ---- frozen DDS format constants (img_dds.h / com_image.h) -----------------

// Assemble a little-endian FourCC as read_le<uint32> would see it on disk.
[[nodiscard]] constexpr u32 four_cc( char a, char b, char c, char d ) noexcept
{
    return   static_cast<u32>( static_cast<std::uint8_t>( a ) )
         | ( static_cast<u32>( static_cast<std::uint8_t>( b ) ) << 8 )
         | ( static_cast<u32>( static_cast<std::uint8_t>( c ) ) << 16 )
         | ( static_cast<u32>( static_cast<std::uint8_t>( d ) ) << 24 );
}

constexpr u32 k_dds_ident = four_cc( 'D', 'D', 'S', ' ' );  // "DDS " magic

constexpr u32 k_type_dxt1 = four_cc( 'D', 'X', 'T', '1' );
constexpr u32 k_type_dxt2 = four_cc( 'D', 'X', 'T', '2' );
constexpr u32 k_type_dxt3 = four_cc( 'D', 'X', 'T', '3' );
constexpr u32 k_type_dxt4 = four_cc( 'D', 'X', 'T', '4' );
constexpr u32 k_type_dxt5 = four_cc( 'D', 'X', 'T', '5' );
constexpr u32 k_type_dx10 = four_cc( 'D', 'X', '1', '0' );
constexpr u32 k_type_ati2 = four_cc( 'A', 'T', 'I', '2' );
constexpr u32 k_type_bc5s = four_cc( 'B', 'C', '5', 'S' );
constexpr u32 k_type_bc4s = four_cc( 'B', 'C', '4', 'S' );
constexpr u32 k_type_bc4u = four_cc( 'B', 'C', '4', 'U' );

// DXGI formats used by the DX10 extension header (img_dds.h dxgi_format_t).
constexpr u32 k_dxgi_bc4_typeless = 79, k_dxgi_bc4_unorm = 80, k_dxgi_bc4_snorm = 81;
constexpr u32 k_dxgi_bc5_typeless = 82, k_dxgi_bc5_unorm = 83, k_dxgi_bc5_snorm = 84;
constexpr u32 k_dxgi_bc6h_typeless = 94, k_dxgi_bc6h_uf16 = 95, k_dxgi_bc6h_sf16 = 96;
constexpr u32 k_dxgi_bc7_typeless = 97, k_dxgi_bc7_unorm = 98, k_dxgi_bc7_unorm_srgb = 99;

// dwFlags (header)
constexpr u32 k_dds_mipmapcount = 0x00020000u;
constexpr u32 k_dds_linearsize  = 0x00080000u;
constexpr u32 k_dds_pitch       = 0x00000008u;
constexpr u32 k_dds_depth       = 0x00800000u;
// dwFlags (pixelformat)
constexpr u32 k_pf_fourcc    = 0x00000004u;
constexpr u32 k_pf_luminance = 0x00020000u;
constexpr u32 k_pf_dudv      = 0x00080000u;
// dwCaps1 / dwCaps2
constexpr u32 k_dds_complex = 0x00000008u;
constexpr u32 k_dds_cubemap = 0x00000200u;
constexpr u32 k_dds_volume  = 0x00200000u;

// DXT_ENCODE_* (com_image.h): stored in dwReserved1[0], read as a 16-bit word.
constexpr u32 k_encode_color_ycocg  = 0x1A01u;
constexpr u32 k_encode_normal_ag_lo = 0x1A05u;  // NORMAL_AG_ORTHO
constexpr u32 k_encode_normal_ag_hi = 0x1A09u;  // NORMAL_AG_AZIMUTHAL

// dds_t field byte offsets (the 128-byte header includes the 4-byte magic).
constexpr std::size_t k_dds_header   = 128;  // sizeof(dds_t)
constexpr std::size_t k_dxt10_header = 20;   // sizeof(dds_header_dxt10_t)
constexpr std::size_t k_pixf_size    = 32;   // sizeof(dds_pixf_t)

constexpr std::size_t k_off_ident      = 0;
constexpr std::size_t k_off_size       = 4;
constexpr std::size_t k_off_flags      = 8;
constexpr std::size_t k_off_height     = 12;
constexpr std::size_t k_off_width      = 16;
constexpr std::size_t k_off_linearsize = 20;
constexpr std::size_t k_off_depth      = 24;
constexpr std::size_t k_off_mipcount   = 28;
constexpr std::size_t k_off_reserved0  = 36;   // dwReserved1[0] (encode)
constexpr std::size_t k_off_pf_size    = 76;
constexpr std::size_t k_off_pf_flags   = 80;
constexpr std::size_t k_off_pf_fourcc  = 84;
constexpr std::size_t k_off_pf_bits    = 88;
constexpr std::size_t k_off_caps1      = 108;
constexpr std::size_t k_off_caps2      = 112;
constexpr std::size_t k_off_dxgi       = 128;  // dds_header_dxt10_t.dxgiFormat

constexpr u32 k_image_max_dim = 8192;  // legacy IMAGE_MAXWIDTH / IMAGE_MAXHEIGHT

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// ---- pixel-format mapping (legacy Image_DXTGetPixelFormat) -----------------

// FourCC -> PixelFormat. DXT2/DXT4 map like DXT3/DXT5: legacy additionally clears
// IMAGE_HAS_ALPHA for those premultiplied variants, but that clear runs before —
// and is overwritten by — the later alpha scan, so it has no observable effect;
// the plain mapping is byte-exact.
[[nodiscard]] PixelFormat map_fourcc( u32 cc ) noexcept
{
    switch( cc )
    {
    case k_type_dxt1: return PixelFormat::Dxt1;
    case k_type_dxt2:
    case k_type_dxt3: return PixelFormat::Dxt3;
    case k_type_dxt4:
    case k_type_dxt5: return PixelFormat::Dxt5;
    case k_type_ati2: return PixelFormat::Ati2;
    case k_type_bc5s: return PixelFormat::Bc5Signed;
    case k_type_bc4s: return PixelFormat::Bc4Signed;
    case k_type_bc4u: return PixelFormat::Bc4Unsigned;
    default:          return PixelFormat::Unknown;
    }
}

// DXGI format -> PixelFormat (the "DX10" FourCC extension-header branch).
[[nodiscard]] PixelFormat map_dxgi( u32 fmt ) noexcept
{
    switch( fmt )
    {
    case k_dxgi_bc4_typeless:
    case k_dxgi_bc4_unorm:      return PixelFormat::Bc4Unsigned;
    case k_dxgi_bc4_snorm:      return PixelFormat::Bc4Signed;
    case k_dxgi_bc6h_sf16:      return PixelFormat::Bc6hSigned;
    case k_dxgi_bc6h_uf16:
    case k_dxgi_bc6h_typeless:  return PixelFormat::Bc6hUnsigned;
    case k_dxgi_bc7_unorm:
    case k_dxgi_bc7_typeless:   return PixelFormat::Bc7Unorm;
    case k_dxgi_bc7_unorm_srgb: return PixelFormat::Bc7Srgb;
    case k_dxgi_bc5_typeless:   return PixelFormat::Ati2;
    case k_dxgi_bc5_unorm:      return PixelFormat::Bc5Unsigned;
    case k_dxgi_bc5_snorm:      return PixelFormat::Bc5Signed;
    default:                    return PixelFormat::Unknown;
    }
}

// ---- alpha-block scanners (legacy Image_CheckDXT3Alpha/Image_CheckDXT5Alpha) --
// Reproduced verbatim (including the legacy quirk that DXT3 reads the block's
// SECOND 8 bytes as "alpha"), but bounds-checked so a short buffer stops the scan
// instead of faulting.

[[nodiscard]] bool check_dxt3_alpha( std::span<const std::byte> fin, u32 w, u32 h ) noexcept
{
    std::size_t base = 0;
    for( u32 y = 0; y < h; y += 4 )
    {
        for( u32 x = 0; x < w; x += 4 )
        {
            const std::size_t alpha = base + 8;  // legacy: alpha = fin + 8
            base += 16;
            if( alpha + 8 > fin.size() )
                return false;

            for( u32 j = 0; j < 4; ++j )
            {
                u32 s_alpha = static_cast<u32>( byte_at( fin, alpha + 2 * j ) )
                            + 256u * byte_at( fin, alpha + 2 * j + 1 );
                for( u32 i = 0; i < 4; ++i )
                {
                    if( x + i < w && y + j < h && s_alpha == 0 )
                        return true;
                    s_alpha >>= 4;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool check_dxt5_alpha( std::span<const std::byte> fin, u32 w, u32 h ) noexcept
{
    std::size_t base = 0;
    for( u32 y = 0; y < h; y += 4 )
    {
        for( u32 x = 0; x < w; x += 4 )
        {
            const std::size_t mask = base + 2;  // legacy: alphamask = fin + 2
            base += 16;
            if( base > fin.size() )
                return false;

            // last three alpha-index bytes -> alphamask[3..5]
            u32 bits = static_cast<u32>( byte_at( fin, mask + 3 ) )
                     | ( static_cast<u32>( byte_at( fin, mask + 4 ) ) << 8 )
                     | ( static_cast<u32>( byte_at( fin, mask + 5 ) ) << 16 );

            for( u32 j = 2; j < 4; ++j )
                for( u32 i = 0; i < 4; ++i )
                {
                    if( x + i < w && y + j < h && ( bits & 0x07u ) )
                        return true;
                    bits >>= 3;
                }
        }
    }
    return false;
}

// ---- decode ----------------------------------------------------------------

[[nodiscard]] Result<Image> decode_dds( std::string_view /*name*/, std::span<const std::byte> file )
{
    namespace u = xash::utilities;

    // Header must be fully present (magic + 124-byte dds_t body).
    if( file.size() < k_dds_header )
        return std::unexpected( ImageError::Truncated );

    const auto u32_at = [&]( std::size_t off ) noexcept -> u32 {
        return u::read_le<u32>( file.data() + off );
    };

    // Magic + structure-size sanity (legacy: DDSHEADER, dwSize==124, pf.dwSize==32).
    if( u32_at( k_off_ident ) != k_dds_ident )
        return std::unexpected( ImageError::BadHeader );
    if( u32_at( k_off_size ) != k_dds_header - sizeof( u32 ) )
        return std::unexpected( ImageError::BadHeader );
    if( u32_at( k_off_pf_size ) != k_pixf_size )
        return std::unexpected( ImageError::BadHeader );

    const u32 dw_flags     = u32_at( k_off_flags );
    const u32 height       = u32_at( k_off_height );
    const u32 width        = u32_at( k_off_width );
    const u32 linear_size  = u32_at( k_off_linearsize );
    const u32 dw_depth     = u32_at( k_off_depth );
    const u32 mipmap_count = u32_at( k_off_mipcount );
    const u32 reserved0    = u32_at( k_off_reserved0 );
    const u32 pf_flags     = u32_at( k_off_pf_flags );
    const u32 fourcc       = u32_at( k_off_pf_fourcc );
    const u32 rgb_bits     = u32_at( k_off_pf_bits );
    const u32 caps1        = u32_at( k_off_caps1 );
    const u32 caps2        = u32_at( k_off_caps2 );

    // Optional DX10 extension header (only when the FourCC is literally "DX10").
    std::size_t headers_offset = k_dds_header;
    u32 dxgi_format = 0;
    if( fourcc == k_type_dx10 )
    {
        if( file.size() < k_dds_header + k_dxt10_header )
            return std::unexpected( ImageError::Truncated );
        dxgi_format = u32_at( k_off_dxgi );
        headers_offset += k_dxt10_header;
    }

    // Dimension bounds (legacy Image_ValidSize: 0 < dim <= 8192).
    if( width == 0 || height == 0 || width > k_image_max_dim || height > k_image_max_dim )
        return std::unexpected( ImageError::BadHeader );

    // image.depth: the volume depth only when the DEPTH flag is set (legacy).
    const u32 image_depth = ( dw_flags & k_dds_depth ) ? dw_depth : 1;

    // Pixel format. hdr->dwDepth is forced to 1 unless a true volume texture; it
    // drives only the volume-size adjustment below (not image.depth).
    const u32 hdr_depth = ( caps2 & k_dds_volume ) ? dw_depth : 1;

    PixelFormat type = PixelFormat::Unknown;
    if( pf_flags & k_pf_fourcc )
    {
        type = ( fourcc == k_type_dx10 ) ? map_dxgi( dxgi_format ) : map_fourcc( fourcc );
    }
    else if( !( pf_flags & ( k_pf_dudv | k_pf_luminance ) ) )
    {
        // Uncompressed: the bit-count selects the raw layout. (Legacy treats the
        // DUDV/LUMINANCE-flagged variants as unsupported -> PF_UNKNOWN.)
        switch( rgb_bits )
        {
        case 32: type = PixelFormat::Bgra8;      break;
        case 24: type = PixelFormat::Bgr8;       break;
        case 8:  type = PixelFormat::Luminance8; break;
        default: type = PixelFormat::Unknown;    break;
        }
    }

    ImageFlags flags = ImageFlags::None;
    if( ( caps1 & k_dds_complex ) && ( caps2 & k_dds_cubemap ) )
        flags |= ImageFlags::Cubemap;

    // Volume-size adjustment (legacy Image_DXTAdjustVolume): a real volume texture
    // gets a single-level linear size and the LINEARSIZE flag forced on.
    u32         eff_flags  = dw_flags;
    std::size_t eff_linear = linear_size;
    if( hdr_depth > 1 )
    {
        eff_linear = level_bytes( type, width, height, hdr_depth );
        eff_flags |= k_dds_linearsize;
    }

    // Compressed formats are ALWAYS accepted (no IL_DDS_HARDWARE gate — see top).
    if( type == PixelFormat::Unknown )
        return std::unexpected( ImageError::UnsupportedFeature );

    // Total payload size (legacy Image_DXTCalcSize / Image_DXTCalcMipmapSize).
    const auto calc_mipmap_size = [&]() noexcept -> std::size_t {
        std::size_t total = 0;
        const u32 levels = std::max<u32>( 1, mipmap_count );
        for( u32 i = 0; i < levels; ++i )
        {
            const std::size_t lw = std::max<u32>( 1, width >> i );
            const std::size_t lh = std::max<u32>( 1, height >> i );
            const std::size_t ld = std::max<u32>( 1, image_depth >> i );
            total += level_bytes( type, lw, lh, ld );
        }
        return total;
    };

    std::size_t buffsize;
    if( caps2 & k_dds_cubemap )
        buffsize = calc_mipmap_size() * 6;             // all six faces
    else if( eff_flags & k_dds_mipmapcount )
        buffsize = calc_mipmap_size();
    else if( eff_flags & ( k_dds_linearsize | k_dds_pitch ) )
        buffsize = eff_linear;
    else
        buffsize = calc_mipmap_size();

    const std::size_t data_avail = file.size() - headers_offset;
    if( buffsize > data_avail )
        return std::unexpected( ImageError::Truncated );  // legacy: buffsize>filesize rejects
    if( buffsize == 0 )
        return std::unexpected( ImageError::BadHeader );   // degenerate size fields

    const std::span<const std::byte> fin = file.subspan( headers_offset, buffsize );

    // Content flags (legacy encode switch + real-alpha scan).
    const u32 encode = reserved0 & 0xFFFFu;  // (word)header.dwReserved1[0]
    if( encode == k_encode_color_ycocg
        || ( encode >= k_encode_normal_ag_lo && encode <= k_encode_normal_ag_hi ) )
    {
        flags |= ImageFlags::HasColor;  // YCoCg / AG-normal encoders carry colour
    }
    else
    {
        if( type == PixelFormat::Dxt3 && check_dxt3_alpha( fin, width, height ) )
            flags |= ImageFlags::HasAlpha;
        else if( type == PixelFormat::Dxt5 && check_dxt5_alpha( fin, width, height ) )
            flags |= ImageFlags::HasAlpha;
        else if( type == PixelFormat::Bc5Signed || type == PixelFormat::Bc5Unsigned )
            flags |= ImageFlags::HasAlpha;
        else if( type == PixelFormat::Bc7Unorm || type == PixelFormat::Bc7Srgb )
            flags |= ImageFlags::HasAlpha;

        if( !( pf_flags & k_pf_luminance ) )
            flags |= ImageFlags::HasColor;
        if( type == PixelFormat::Bgra8 || type == PixelFormat::Rgba8 )
            flags |= ImageFlags::HasAlpha;
    }

    if( type == PixelFormat::Luminance8 )
        flags &= ~( ImageFlags::HasColor | ImageFlags::HasAlpha );

    flags |= ImageFlags::DdsFormat;

    // Volume/array marker (modernization: legacy carries depth only, no flag).
    if( image_depth > 1 )
        flags |= ImageFlags::Multilayer;

    // Assemble the kept-compressed image (raw block bytes copied verbatim).
    std::vector<std::byte> pixels( fin.begin(), fin.end() );

    Image img( static_cast<std::uint16_t>( width ),
               static_cast<std::uint16_t>( height ),
               type, std::move( pixels ), flags );

    img.set_depth( static_cast<std::uint16_t>( std::max<u32>( 1, image_depth ) ) );
    if( dw_flags & k_dds_mipmapcount )
        img.set_mip_count( static_cast<std::uint8_t>( std::clamp<u32>( mipmap_count, 1, 255 ) ) );

    return img;
}

// ---- the registered codec --------------------------------------------------
class DdsCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "dds"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_dds( name, file );
    }
};

} // namespace

const IImageCodec &dds_codec() noexcept
{
    static const DdsCodec codec;
    return codec;
}

} // namespace xash::imagelib
