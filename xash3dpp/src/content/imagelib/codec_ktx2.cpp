// xash3dpp — imagelib KTX2 codec (decode; kept GPU-compressed)
// Legacy reference: engine/common/imagelib/img_ktx2.c
//   Image_LoadKTX2 (validate the 12-byte identifier, parse the header) +
//   Image_KTX2Format (vkFormat -> PixelFormat) + Image_KTX2Parse (map the
//   format, reject unsupported containers, cross-check every mip size, then
//   concatenate the mip levels into one kept-compressed buffer).
// Byte-format frozen: engine/common/imagelib/img_ktx2.h (ktx2_header_t /
//   ktx2_index_t / ktx2_level_t, the KTX2_IDENTIFIER, the KTX2_FORMAT_* vkFormats).
//
// Deliberate parity divergences (all documented at their site below):
//  1. No IL_KTX2_RAW global. Legacy stores the whole file as PF_KTX2_RAW only
//     when that global is set; the rewrite ALWAYS falls back to Ktx2Raw for a
//     valid-but-unrecognised KTX2 (so ref_vk can consume it at the Chunk-13 seam).
//  2. Structural rejects (supercompression / faceCount>1 / layerCount>1 /
//     pixelDepth>1) are hoisted BEFORE the vkFormat map and return
//     UnsupportedFeature. Legacy checks the format first and would raw-pass these
//     under IL_KTX2_RAW — but such data is not consumable as-is, so we reject it.
//  3. DdsFormat is set. The legacy KTX2 loader does NOT set IMAGE_DDS_FORMAT (only
//     img_dds.c does); the rewrite unifies "kept-compressed GPU upload" under the
//     one flag (pixel_format.hpp: "came from a DDS/KTX2 (kept compressed)").
//  4. The legacy redundant pre-copy (memcpy the whole prefix, then overwrite it in
//     the concat loop) is not reproduced — every byte it writes is overwritten.
//  5. ktx2_index_t and each level's uncompressedByteLength are read-but-unused in
//     legacy; the rewrite skips them entirely (no observable effect).
//
// Codecs are stateless (no global scratch — boundary H-1) and bounds-check every
// read (modernization H-4): a malformed file yields an ImageError, never a fault.

#include <xash3dpp/private/imagelib/codec.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::imagelib {
namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

// ---- frozen KTX2 layout constants (img_ktx2.h) -----------------------------

// The 12-byte KTX2 identifier: "\xABKTX 20\xBB\r\n\x1A\n".
constexpr std::size_t                       k_ident_size = 12;
constexpr std::array<std::uint8_t, k_ident_size> k_identifier = {
    0xABu, 'K', 'T', 'X', ' ', '2', '0', 0xBBu, '\r', '\n', 0x1Au, '\n'
};

constexpr std::size_t k_header_size   = 36;  // ktx2_header_t: 9 x uint32_t
constexpr std::size_t k_index_size    = 32;  // ktx2_index_t: 4 x u32 + 2 x u64
constexpr std::size_t k_level_size    = 24;  // ktx2_level_t: 3 x uint64_t
constexpr std::size_t k_levels_offset = k_ident_size + k_header_size + k_index_size;   // 80
constexpr std::size_t k_minimal_size  = k_levels_offset + k_level_size;                // 104

// File-relative offsets of the ktx2_header_t fields (header begins after the id).
constexpr std::size_t k_off_vkformat = k_ident_size + 0;    // 12
constexpr std::size_t k_off_width    = k_ident_size + 8;    // 20  (typeSize at +4 unused)
constexpr std::size_t k_off_height   = k_ident_size + 12;   // 24
constexpr std::size_t k_off_depth    = k_ident_size + 16;   // 28
constexpr std::size_t k_off_layers   = k_ident_size + 20;   // 32
constexpr std::size_t k_off_faces    = k_ident_size + 24;   // 36
constexpr std::size_t k_off_levels   = k_ident_size + 28;   // 40
constexpr std::size_t k_off_supercmp = k_ident_size + 32;   // 44

// KTX2_FORMAT_* — the VkFormat subset legacy maps (img_ktx2.h ktx2_format_t).
constexpr u32 k_vk_bc4_unorm   = 139;
constexpr u32 k_vk_bc4_snorm   = 140;
constexpr u32 k_vk_bc5_unorm   = 141;
constexpr u32 k_vk_bc5_snorm   = 142;
constexpr u32 k_vk_bc6h_ufloat = 143;
constexpr u32 k_vk_bc6h_sfloat = 144;
constexpr u32 k_vk_bc7_unorm   = 145;
constexpr u32 k_vk_bc7_srgb    = 146;

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

// ---- vkFormat -> PixelFormat mapping (legacy Image_KTX2Format) --------------
// The content flags mirror the legacy per-format SetBits: BC4 carries none, BC5
// gains HAS_ALPHA (2 components), BC6H gains HAS_COLOR (3 HDR components), BC7
// gains HAS_COLOR|HAS_ALPHA (4 components).

struct FormatMap
{
    PixelFormat type;
    ImageFlags  flags;
};

[[nodiscard]] FormatMap map_vk_format( u32 vk ) noexcept
{
    switch( vk )
    {
    case k_vk_bc4_unorm:   return { PixelFormat::Bc4Unsigned,  ImageFlags::None };
    case k_vk_bc4_snorm:   return { PixelFormat::Bc4Signed,    ImageFlags::None };
    case k_vk_bc5_unorm:   return { PixelFormat::Bc5Unsigned,  ImageFlags::HasAlpha };
    case k_vk_bc5_snorm:   return { PixelFormat::Bc5Signed,    ImageFlags::HasAlpha };
    case k_vk_bc6h_ufloat: return { PixelFormat::Bc6hUnsigned, ImageFlags::HasColor };
    case k_vk_bc6h_sfloat: return { PixelFormat::Bc6hSigned,   ImageFlags::HasColor };
    case k_vk_bc7_unorm:   return { PixelFormat::Bc7Unorm, ImageFlags::HasColor | ImageFlags::HasAlpha };
    case k_vk_bc7_srgb:    return { PixelFormat::Bc7Srgb,  ImageFlags::HasColor | ImageFlags::HasAlpha };
    default:               return { PixelFormat::Unknown, ImageFlags::None };
    }
}

// ---- decode ----------------------------------------------------------------

[[nodiscard]] Result<Image> decode_ktx2( std::string_view /*name*/, std::span<const std::byte> file )
{
    namespace u = ::xash::utilities;

    // The fixed prologue must be fully present: identifier + header + index + one
    // level record (legacy KTX2_MINIMAL_HEADER_SIZE).
    if( file.size() < k_minimal_size )
        return std::unexpected( ImageError::Truncated );

    // 12-byte KTX2 identifier (legacy memcmp against KTX2_IDENTIFIER).
    for( std::size_t i = 0; i < k_ident_size; ++i )
    {
        if( byte_at( file, i ) != k_identifier[i] )
            return std::unexpected( ImageError::BadHeader );
    }

    // All header/first-level reads below sit within the verified 104-byte prologue.
    const auto u32_at = [&]( std::size_t off ) noexcept -> u32 {
        return u::read_le<u32>( file.data() + off );
    };
    const auto u64_at = [&]( std::size_t off ) noexcept -> u64 {
        return u::read_le<u64>( file.data() + off );
    };

    const u32 vk_format    = u32_at( k_off_vkformat );
    const u32 pixel_width  = u32_at( k_off_width );
    const u32 pixel_height = u32_at( k_off_height );
    const u32 pixel_depth  = u32_at( k_off_depth );
    const u32 layer_count  = u32_at( k_off_layers );
    const u32 face_count   = u32_at( k_off_faces );
    const u32 level_count  = u32_at( k_off_levels );
    const u32 supercomp    = u32_at( k_off_supercmp );

    // Structurally-unsupported containers (see divergence 2). Legacy rejects each
    // of these too, only later; hoisting them keeps the Ktx2Raw fallback strictly
    // for single-face/-layer, 2D, non-supercompressed files.
    if( supercomp != 0 )
        return std::unexpected( ImageError::UnsupportedFeature );
    if( pixel_depth > 1 )
        return std::unexpected( ImageError::UnsupportedFeature );
    if( face_count > 1 )
        return std::unexpected( ImageError::UnsupportedFeature );
    if( layer_count > 1 )
        return std::unexpected( ImageError::UnsupportedFeature );

    // legacy image.depth = Q_max( 1, pixelDepth ); pixelDepth is 0 or 1 by now.
    const auto out_depth = static_cast<std::uint16_t>( std::max<u32>( 1, pixel_depth ) );
    const auto out_w     = static_cast<std::uint16_t>( pixel_width );   // legacy width is a word
    const auto out_h     = static_cast<std::uint16_t>( pixel_height );  // (truncates >65535, as legacy)

    const FormatMap fmt = map_vk_format( vk_format );

    // --- unrecognised vkFormat: kept-raw passthrough --------------------------
    // (legacy PF_KTX2_RAW; unconditional here — divergence 1). The whole file is
    // handed to the renderer verbatim. Ktx2Raw is NOT is_compressed() in the
    // rewrite, but still a kept-as-is GPU format, so DdsFormat marks it.
    if( fmt.type == PixelFormat::Unknown )
    {
        std::vector<std::byte> raw( file.begin(), file.end() );
        Image img( out_w, out_h, PixelFormat::Ktx2Raw, std::move( raw ), ImageFlags::DdsFormat );
        img.set_depth( out_depth );  // num_mips stays 1, matching the legacy raw path
        return img;
    }

    // --- recognised BCn vkFormat: concatenate kept-compressed mip levels -------
    if( level_count == 0 )
        return std::unexpected( ImageError::BadHeader );  // legacy: "file has no mip levels"

    // The level index array (level_count records) must fit after the prologue
    // (legacy: levelCount*sizeof(ktx2_level_t) + KTX2_LEVELS_OFFSET > filesize).
    // Rearranged to avoid overflow; file.size() >= k_minimal_size > k_levels_offset.
    const std::size_t levels_bytes = static_cast<std::size_t>( level_count ) * k_level_size;
    if( levels_bytes > file.size() - k_levels_offset )
        return std::unexpected( ImageError::Truncated );  // legacy: "file abruptly ends"

    // Pass 1 — validate each level's computed size and its data extent.
    std::size_t total_size = 0;
    for( u32 mip = 0; mip < level_count; ++mip )
    {
        const std::size_t level_off  = k_levels_offset + static_cast<std::size_t>( mip ) * k_level_size;
        const u64         byte_offset = u64_at( level_off + 0 );
        const u64         byte_length = u64_at( level_off + 8 );  // (+16 uncompressed: unused)

        // Guard the shift: legacy `pixelWidth >> mip` is UB for mip>=32 (and can
        // never widen a real mip chain); clamp to the 1x1 tail instead of faulting.
        const u32 w = ( mip >= 32u ) ? 1u : std::max<u32>( 1, pixel_width  >> mip );
        const u32 h = ( mip >= 32u ) ? 1u : std::max<u32>( 1, pixel_height >> mip );
        const std::size_t mip_size = level_bytes( fmt.type, w, h, 1 );

        // Legacy: mip_size != level.byteLength -> reject (declared format lied).
        if( byte_length != static_cast<u64>( mip_size ) )
            return std::unexpected( ImageError::BadHeader );

        // Legacy max_offset check, per level and overflow-safe: the payload span
        // [byteOffset, byteOffset+byteLength) must lie within the file.
        if( byte_offset > file.size() || byte_length > file.size() - byte_offset )
            return std::unexpected( ImageError::Truncated );

        total_size += mip_size;
    }

    // Pass 2 — concatenate the level payloads in level order (byte_offset is
    // re-read; the file span is const, so pass 1's bounds still hold).
    std::vector<std::byte> pixels;
    pixels.reserve( total_size );
    for( u32 mip = 0; mip < level_count; ++mip )
    {
        const std::size_t level_off   = k_levels_offset + static_cast<std::size_t>( mip ) * k_level_size;
        const std::size_t byte_offset = static_cast<std::size_t>( u64_at( level_off + 0 ) );
        const std::size_t byte_length = static_cast<std::size_t>( u64_at( level_off + 8 ) );
        for( std::size_t i = 0; i < byte_length; ++i )
            pixels.push_back( file[byte_offset + i] );
    }

    Image img( out_w, out_h, fmt.type, std::move( pixels ), fmt.flags | ImageFlags::DdsFormat );
    img.set_depth( out_depth );
    img.set_mip_count( static_cast<std::uint8_t>( std::clamp<u32>( level_count, 1, 255 ) ) );

    return img;
}

// ---- the registered codec --------------------------------------------------
class Ktx2Codec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "ktx2"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_ktx2( name, file );
    }
};

} // namespace

const IImageCodec &ktx2_codec() noexcept
{
    static const Ktx2Codec codec;
    return codec;
}

} // namespace xash::imagelib
