#pragma once
// xash3dpp — imagelib (image codec library) public API
// Legacy reference: engine/common/imagelib/ (img_main.c, img_utils.c,
//   img_wad.c, img_bmp/tga/dds/ktx2/png.c, img_quant.c)
// Boundary: docs/boundaries/content-boundary.md — Q-11 satellite verdict:
//   a SEPARATE target (xash3dpp_imagelib) from the model cache.
// Modernization: docs/modernization-opportunities/content-modernization.md O-2.
//
// @thread-safety: init()/shutdown() are main-thread only (assert Main). The
//   decoder owns its per-call scratch — there is no global `imglib_t image`
//   singleton (boundary H-1). Once the IImageCodec registry lands (O-2) the
//   codecs are stateless and one ImageDecoder is safe to share across worker
//   threads; until then treat a single instance as main-thread.

#include <xash3dpp/imagelib/errors.hpp>
#include <xash3dpp/imagelib/image.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace xash::imagelib {

// ---------------------------------------------------------------------------
// Instrumentation (three-tier model — docs/design/debug-stats-design.md)
// ---------------------------------------------------------------------------

struct ImageStats {
    // Tier 1 — always-on: a plain counter bumped on the cold decode path.
    std::uint64_t images_decoded  = 0;  // decode() calls that produced an image
    std::uint64_t decode_failures = 0;  // decode() calls that returned an error
};

// ---------------------------------------------------------------------------
// ImageDecoder — owns decode scratch and the (future) IImageCodec registry.
// Replaces the legacy global `imglib_t image` (boundary O-2 / H-1).
// ---------------------------------------------------------------------------

class ImageDecoder {
public:
    ImageDecoder();
    ~ImageDecoder();

    ImageDecoder(const ImageDecoder&)            = delete;
    ImageDecoder& operator=(const ImageDecoder&) = delete;
    ImageDecoder(ImageDecoder&&) noexcept;
    ImageDecoder& operator=(ImageDecoder&&) noexcept;

    // Lifecycle — main-thread only.
    [[nodiscard]] bool init();
    void shutdown();

    [[nodiscard]] const ImageStats& stats() const noexcept;

    // TODO(Chunk 7, O-2): register the per-format IImageCodec implementations
    //   and add the decode/encode surface (all std::expected, boundary H-2):
    //     std::expected<Image, ImageError> decode(std::string_view name,
    //                                              std::span<const std::byte> file);
    //     std::expected<void,  ImageError> save  (...);              // WAD3 pack
    //   The "WAD texture pack/unpack test" deliverable exercises the MIP/WAD3
    //   codec on raw buffers (no filesystem needed — codecs take spans).

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::imagelib
