#pragma once
// xash3dpp — imagelib decode-codec interface (O-2)
// Private: the polymorphic codec hierarchy behind ImageDecoder::decode().
// Each codec is a stateless singleton registered by ImageDecoder; keeping them
// stateless (no global `image` scratch — boundary H-1) makes one shared
// instance reentrant, the enabler for the Chunk-7 worker pool.
//
// @thread-safety: codec instances are const/stateless — safe to share.

#include <xash3dpp/imagelib/errors.hpp>
#include <xash3dpp/imagelib/image.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace xash::imagelib {

// A decoder for one image-file family (the legacy load_game[] row).
struct IImageCodec
{
    virtual ~IImageCodec() = default;

    // True if this codec decodes files with the given lowercase extension
    // (no leading dot) — the legacy load_game[] table key.
    [[nodiscard]] virtual bool handles( std::string_view ext ) const noexcept = 0;

    // Decode a whole file buffer into an Image. `name` carries the original
    // filename for the name-prefix quirks ('{' masked, "sky", '#' logo, …).
    [[nodiscard]] virtual Result<Image> decode( std::string_view name,
                                                std::span<const std::byte> file ) const = 0;
};

// Per-codec singleton accessors (defined in each codec_*.cpp). ImageDecoder
// assembles the registry from these — one line per codec as they land.
[[nodiscard]] const IImageCodec &wad_codec() noexcept;
[[nodiscard]] const IImageCodec &tga_codec() noexcept;
[[nodiscard]] const IImageCodec &bmp_codec() noexcept;
[[nodiscard]] const IImageCodec &dds_codec() noexcept;

} // namespace xash::imagelib
