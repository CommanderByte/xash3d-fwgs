#pragma once
// xash3dpp — central ErrorCode enum
// Decision ref: docs/design/decisions-architecture.md §3 Q-5 (Chunk-2 vocabulary).
//
// This file is intentionally minimal during Chunk 3.  New error codes are
// appended at the end as each subsystem comes online; numeric values are
// committed (diagnostic logs include the integer value).

#include <cstdint>

namespace xash::core {

enum class ErrorCode : std::uint32_t
{
    Ok                 = 0,

    // Generic
    InvalidArgument    = 1,
    OutOfMemory        = 2,
    NotInitialised     = 3,
    AlreadyInitialised = 4,

    // Host
    HostFatal          = 100,  // Game-DLL or engine-internal Host_Error path
    FrameAborted       = 101,  // signal_frame_abort was tripped this frame

    // Map-load
    MapNotFound        = 200,
    MapLoadFailed      = 201,
    BspUnsupportedVersion = 202, // header version not 29/30/'BSP2'
    BspCorruptLump     = 203,    // lump directory/record validation failed
    BspBadWorld        = 204,    // world invariant broken (missing required lump, leaf 0 not solid)
};

// Stable string name for diagnostic output.  Never null.
[[nodiscard]] const char *error_code_name( ErrorCode ec ) noexcept;

} // namespace xash::core
