#pragma once
// xash3dpp — imagelib error codes and Result<T> alias
// @thread-safety: pure types (enum + Result<T> alias) — no shared state.
// Legacy reference: engine/common/imagelib/* return-false failure paths.

#include <cstdint>
#include <expected>

namespace xash::imagelib {

// ---------------------------------------------------------------------------
// ImageError — typed decode/encode failure codes for std::expected returns
// (modernization H-2: replaces the legacy qboolean + global-`image` channel).
// ---------------------------------------------------------------------------

enum class ImageError : std::uint32_t
{
    UnknownFormat,      // no codec recognised the extension or magic
    Empty,              // zero-length input
    Truncated,          // a parse read would run past the end of the buffer
    BadHeader,          // magic / version / dimensions invalid
    UnsupportedFeature, // valid but unsupported variant (interlaced PNG, KTX2
                        //   supercompression, compressed DDS without hardware, …)
    TooLarge,           // exceeds the imagelib dimension / lump caps
    OutOfMemory,        // pool allocation failed
};

[[nodiscard]] constexpr const char *to_string( ImageError e ) noexcept
{
    switch( e )
    {
    case ImageError::UnknownFormat:      return "unknown-format";
    case ImageError::Empty:              return "empty";
    case ImageError::Truncated:          return "truncated";
    case ImageError::BadHeader:          return "bad-header";
    case ImageError::UnsupportedFeature: return "unsupported-feature";
    case ImageError::TooLarge:           return "too-large";
    case ImageError::OutOfMemory:        return "out-of-memory";
    }
    return "unknown";
}

// Result<T> — success-or-ImageError alias (std::expected<T, ImageError>).
template<typename T>
using Result = std::expected<T, ImageError>;

} // namespace xash::imagelib
