#pragma once
// xash3dpp — shared compile-time limits
// All subsystems that need fixed buffer or count limits import this header.
//
// To override individual values for a specialised build, create a
// CMake-generated include/xash3dpp/limits_override.hpp and set
// XASH_HAS_LIMITS_OVERRIDE before including this file.

#include <cstddef>
#include <cstdint>

#if defined(XASH_HAS_LIMITS_OVERRIDE)
#   include <xash3dpp/limits_override.hpp>
#endif

namespace xash::limits {

// cmd_cvar subsystem
#ifndef XASH_LIMIT_CMD_LINE_MAX
inline constexpr std::size_t cmd_line_max = 1024; // max bytes in a single command line
#else
inline constexpr std::size_t cmd_line_max = XASH_LIMIT_CMD_LINE_MAX;
#endif

#ifndef XASH_LIMIT_CMD_TOKENS_MAX
inline constexpr std::size_t cmd_tokens_max = 80; // max tokenised arguments per command
#else
inline constexpr std::size_t cmd_tokens_max = XASH_LIMIT_CMD_TOKENS_MAX;
#endif

#ifndef XASH_LIMIT_ALIAS_NAME_MAX
inline constexpr std::size_t alias_name_max = 64; // max alias/command/cvar name length
#else
inline constexpr std::size_t alias_name_max = XASH_LIMIT_ALIAS_NAME_MAX;
#endif

#ifndef XASH_LIMIT_CVAR_HASH_BUCKETS
inline constexpr std::size_t cvar_hash_buckets = 64; // hash table bucket count for registry
#else
inline constexpr std::size_t cvar_hash_buckets = XASH_LIMIT_CVAR_HASH_BUCKETS;
#endif

#ifndef XASH_LIMIT_CVAR_CHANGE_LOG_CAPACITY
inline constexpr std::size_t cvar_change_log_capacity = 256; // circular debug change-log depth
#else
inline constexpr std::size_t cvar_change_log_capacity = XASH_LIMIT_CVAR_CHANGE_LOG_CAPACITY;
#endif

#ifndef XASH_LIMIT_CMD_OBSERVER_MAX
inline constexpr std::size_t cmd_observer_max = 16; // max simultaneous ICvarObserver registrations
#else
inline constexpr std::size_t cmd_observer_max = XASH_LIMIT_CMD_OBSERVER_MAX;
#endif

} // namespace xash::limits
