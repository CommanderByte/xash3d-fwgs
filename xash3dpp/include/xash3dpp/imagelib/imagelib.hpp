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
#include <span>
#include <string_view>

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

    // Decode a whole image-file buffer, dispatching by the filename extension
    // to a registered IImageCodec (O-2). `name` carries the original filename
    // for the name-prefix quirks ('{' masked, "sky", '#' logo, …); `file` is
    // the raw bytes (no filesystem — codecs take spans, OQ-1). Main-thread
    // today (bumps stats); the codecs themselves are stateless/reentrant.
    [[nodiscard]] Result<Image> decode( std::string_view name,
                                        std::span<const std::byte> file );

    // Encoding lives in <xash3dpp/imagelib/save.hpp> (save_wad, …) — free
    // functions that return a byte buffer for the caller to write.

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::imagelib
