#pragma once
// xash3dpp — imagelib encode (save) API
// Legacy reference: engine/common/imagelib/img_*.c save paths (FS_SaveImage).
// Encoders produce a byte buffer; the caller writes it through the filesystem,
// so codecs stay filesystem-decoupled (OQ-1) and unit-testable on buffers.
//
// @thread-safety: pure functions over caller-owned data — no shared state.

#include <xash3dpp/imagelib/errors.hpp>
#include <xash3dpp/imagelib/image.hpp>

#include <cstddef>
#include <vector>

namespace xash::imagelib {

// Encode an indexed (Indexed8 + palette) image as a single-lump WAD3 buffer —
// the player-spray "pack" path (legacy Image_SaveWAD). Byte-compatible with the
// legacy writer; round-trips with ImageDecoder::decode() on a ".wad" name.
[[nodiscard]] Result<std::vector<std::byte>> save_wad( const Image &img );

} // namespace xash::imagelib
