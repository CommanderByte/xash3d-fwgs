#pragma once
// xash3dpp — imagelib pixel formats, colours, and content flags
// Legacy reference: common/com_image.h (pixformat_t, imgFlags_t, bpc_desc_t)
// OQ-1: this is the internal type set; a rgbdata_t adapter at the (Chunk-13)
// renderer seam maps PixelFormat <-> the legacy pixformat_t 1:1.
//
// @thread-safety: pure value types + constexpr helpers — no shared state.

#include <cstddef>
#include <cstdint>

namespace xash::imagelib {

// ---------------------------------------------------------------------------
// Rgba — one 8-bit-per-channel texel (the normalised decode output element).
// ---------------------------------------------------------------------------

struct Rgba
{
    std::uint8_t r = 0, g = 0, b = 0, a = 255;

    constexpr bool operator==( const Rgba & ) const noexcept = default;
};

// ---------------------------------------------------------------------------
// PixelFormat — mirrors legacy pixformat_t order (adapter maps 1:1).
// ---------------------------------------------------------------------------

enum class PixelFormat : std::uint8_t
{
    Unknown = 0,
    Indexed8,   // 8-bit palette index (+ Palette); legacy PF_INDEXED_24/_32
    Rgba8,      // the normalised 32-bit output; legacy PF_RGBA_32
    Bgra8,      // legacy PF_BGRA_32 (DDS uncompressed)
    Rgb8,       // legacy PF_RGB_24
    Bgr8,       // legacy PF_BGR_24
    Luminance8, // legacy PF_LUMINANCE
    // Block-compressed (kept compressed for GPU upload; decoded on the GPU).
    Dxt1, Dxt3, Dxt5, Ati2,
    Bc4Signed, Bc4Unsigned, Bc5Signed, Bc5Unsigned,
    Bc6hSigned, Bc6hUnsigned, Bc7Unorm, Bc7Srgb,
    Ktx2Raw,    // KTX2 passthrough for ref_vk (legacy PF_KTX2_RAW)
};

// True for the block-compressed formats (DXT/BCn) that stay GPU-decoded.
[[nodiscard]] constexpr bool is_compressed( PixelFormat f ) noexcept
{
    return f >= PixelFormat::Dxt1 && f <= PixelFormat::Bc7Srgb;
}

// Bytes per texel for the raw formats; 0 for compressed / unknown (use
// block_bytes() for those).
[[nodiscard]] constexpr std::size_t bytes_per_pixel( PixelFormat f ) noexcept
{
    switch( f )
    {
    case PixelFormat::Indexed8:
    case PixelFormat::Luminance8: return 1;
    case PixelFormat::Rgb8:
    case PixelFormat::Bgr8:       return 3;
    case PixelFormat::Rgba8:
    case PixelFormat::Bgra8:      return 4;
    default:                      return 0;
    }
}

// 4x4-block byte size for a compressed format; 0 for non-compressed.
[[nodiscard]] constexpr std::size_t block_bytes( PixelFormat f ) noexcept
{
    switch( f )
    {
    case PixelFormat::Dxt1:
    case PixelFormat::Bc4Signed:
    case PixelFormat::Bc4Unsigned: return 8;   // 8 bytes / 4x4 block
    case PixelFormat::Dxt3:
    case PixelFormat::Dxt5:
    case PixelFormat::Ati2:
    case PixelFormat::Bc5Signed:
    case PixelFormat::Bc5Unsigned:
    case PixelFormat::Bc6hSigned:
    case PixelFormat::Bc6hUnsigned:
    case PixelFormat::Bc7Unorm:
    case PixelFormat::Bc7Srgb:     return 16;  // 16 bytes / 4x4 block
    default:                       return 0;
    }
}

// Byte size of one mip level, matching legacy Image_ComputeSize.
[[nodiscard]] constexpr std::size_t level_bytes( PixelFormat f, std::size_t w,
                                                 std::size_t h, std::size_t depth = 1 ) noexcept
{
    if( is_compressed( f ) )
        return ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * depth * block_bytes( f );
    return w * h * depth * bytes_per_pixel( f );
}

// ---------------------------------------------------------------------------
// ImageFlags — *content* descriptors of a decoded image (legacy imgFlags_t
// content bits). Process operations (flip/resample/luma/quantize) are explicit
// method calls in the rewrite, not flags.
// ---------------------------------------------------------------------------

enum class ImageFlags : std::uint32_t
{
    None        = 0,
    HasAlpha    = 1u << 0,  // has a meaningful alpha channel
    HasColor    = 1u << 1,  // not pure greyscale
    HasLuma     = 1u << 2,  // carries a fullbright/self-illum layer
    ColorIndex  = 1u << 3,  // gradient-decal colour stored in the palette
    OneBitAlpha = 1u << 4,  // '{' masked texture (index 255 transparent)
    Cubemap     = 1u << 5,
    Skybox      = 1u << 6,
    QuakeSky    = 1u << 7,  // 2:1 quake sky texture
    QuakePal    = 1u << 8,  // decoded against the Quake (not HL) palette
    DdsFormat   = 1u << 9,  // came from a DDS/KTX2 (kept compressed)
    Multilayer  = 1u << 10, // volume / array texture (depth > 1)
};

[[nodiscard]] constexpr ImageFlags operator|( ImageFlags a, ImageFlags b ) noexcept
{
    return static_cast<ImageFlags>( static_cast<std::uint32_t>( a ) | static_cast<std::uint32_t>( b ) );
}
[[nodiscard]] constexpr ImageFlags operator&( ImageFlags a, ImageFlags b ) noexcept
{
    return static_cast<ImageFlags>( static_cast<std::uint32_t>( a ) & static_cast<std::uint32_t>( b ) );
}
[[nodiscard]] constexpr ImageFlags operator~( ImageFlags a ) noexcept
{
    return static_cast<ImageFlags>( ~static_cast<std::uint32_t>( a ) );
}
constexpr ImageFlags &operator|=( ImageFlags &a, ImageFlags b ) noexcept { return a = a | b; }
constexpr ImageFlags &operator&=( ImageFlags &a, ImageFlags b ) noexcept { return a = a & b; }

// True if every bit in |mask| is set in |flags|.
[[nodiscard]] constexpr bool has_flag( ImageFlags flags, ImageFlags mask ) noexcept
{
    return ( flags & mask ) == mask;
}

} // namespace xash::imagelib
