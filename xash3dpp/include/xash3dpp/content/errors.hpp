#pragma once
// xash3dpp — content (model load) error codes and Result<T> alias
// @thread-safety: pure types (enum + Result<T> alias) — no shared state.
// Legacy reference: engine/common/model.c Mod_LoadModel failure paths
//   (Host_Error on bad magic / CRC mismatch / MAX_MODELS overflow).

#include <cstdint>
#include <expected>

namespace xash::content {

// ---------------------------------------------------------------------------
// LoadError — typed model-load failure codes for std::expected returns
// (modernization H-2: replaces the legacy `qboolean *loaded` + `crash` channel).
// ---------------------------------------------------------------------------

enum class LoadError : std::uint32_t
{
    NotFound,           // file could not be read from the filesystem
    Empty,              // zero-length file
    Truncated,          // a parse read would run past the end of the file
    BadMagic,           // unrecognised format ident (not IDST / IDSP / IDPO / BSP)
    BadVersion,         // known format, wrong version
    UnsupportedFeature, // valid but unsupported variant
    CacheFull,          // model cache is at MAX_MODELS (legacy Host_Error)
    CrcMismatch,        // reload with a changed CRC (cheat-detection)
    OutOfMemory,        // pool allocation failed
};

[[nodiscard]] constexpr const char *to_string( LoadError e ) noexcept
{
    switch( e )
    {
    case LoadError::NotFound:           return "not-found";
    case LoadError::Empty:              return "empty";
    case LoadError::Truncated:          return "truncated";
    case LoadError::BadMagic:           return "bad-magic";
    case LoadError::BadVersion:         return "bad-version";
    case LoadError::UnsupportedFeature: return "unsupported-feature";
    case LoadError::CacheFull:          return "cache-full";
    case LoadError::CrcMismatch:        return "crc-mismatch";
    case LoadError::OutOfMemory:        return "out-of-memory";
    }
    return "unknown";
}

// Result<T> — success-or-LoadError alias (std::expected<T, LoadError>).
template<typename T>
using Result = std::expected<T, LoadError>;

} // namespace xash::content
