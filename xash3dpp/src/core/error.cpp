// xash3dpp — core::ErrorCode → string-name mapping
#include <xash3dpp/core/error.hpp>

namespace xash::core {

const char *error_code_name( ErrorCode ec ) noexcept
{
    switch (ec) {
    case ErrorCode::Ok:                 return "Ok";
    case ErrorCode::InvalidArgument:    return "InvalidArgument";
    case ErrorCode::OutOfMemory:        return "OutOfMemory";
    case ErrorCode::NotInitialised:     return "NotInitialised";
    case ErrorCode::AlreadyInitialised: return "AlreadyInitialised";
    case ErrorCode::HostFatal:          return "HostFatal";
    case ErrorCode::FrameAborted:       return "FrameAborted";
    case ErrorCode::MapNotFound:        return "MapNotFound";
    case ErrorCode::MapLoadFailed:      return "MapLoadFailed";
    case ErrorCode::BspUnsupportedVersion: return "BspUnsupportedVersion";
    case ErrorCode::BspCorruptLump:     return "BspCorruptLump";
    case ErrorCode::BspBadWorld:        return "BspBadWorld";
    }
    return "Unknown";
}

} // namespace xash::core
