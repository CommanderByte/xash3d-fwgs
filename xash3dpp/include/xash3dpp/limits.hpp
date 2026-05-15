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

// memory subsystem
#ifndef XASH_LIMIT_MEMORY_POOL_MAX
inline constexpr std::size_t memory_pool_max = 128; // max named memory pools
#else
inline constexpr std::size_t memory_pool_max = XASH_LIMIT_MEMORY_POOL_MAX;
#endif

// utilities subsystem
#ifndef XASH_LIMIT_TOKENIZER_TOKEN_MAX
inline constexpr std::size_t tokenizer_token_max = 512; // max token bytes in Tokenizer::next()
#else
inline constexpr std::size_t tokenizer_token_max = XASH_LIMIT_TOKENIZER_TOKEN_MAX;
#endif

#ifndef XASH_LIMIT_ATLAS_MAX_SIZE
// ABI-constrained: matches the legacy atlas struct layout.  Increasing this
// value is a breaking change for any code serialising atlas coordinates.
inline constexpr std::size_t atlas_max_size = 1024; // max texture atlas dimension (px)
#else
inline constexpr std::size_t atlas_max_size = XASH_LIMIT_ATLAS_MAX_SIZE;
#endif

// filesystem subsystem
#ifndef XASH_LIMIT_FILESYSTEM_FILE_BUFFER_SIZE
inline constexpr std::size_t filesystem_file_buffer_size = 2048; // OsFile read-ahead I/O buffer
#else
inline constexpr std::size_t filesystem_file_buffer_size = XASH_LIMIT_FILESYSTEM_FILE_BUFFER_SIZE;
#endif

#ifndef XASH_LIMIT_PAK_MAX_FILES
inline constexpr std::size_t pak_max_files = 65536; // max file entries in a PAK archive
#else
inline constexpr std::size_t pak_max_files = XASH_LIMIT_PAK_MAX_FILES;
#endif

#ifndef XASH_LIMIT_WAD_MAX_LUMPS
inline constexpr std::size_t wad_max_lumps = 65535; // max lump entries in a WAD2/WAD3 archive
#else
inline constexpr std::size_t wad_max_lumps = XASH_LIMIT_WAD_MAX_LUMPS;
#endif

#ifndef XASH_LIMIT_ZIP_FILENAME_MAX
inline constexpr std::size_t zip_filename_max = 4096; // max bytes in a ZIP central-directory path
#else
inline constexpr std::size_t zip_filename_max = XASH_LIMIT_ZIP_FILENAME_MAX;
#endif

// platform subsystem
#ifndef XASH_LIMIT_PLATFORM_CONSOLE_BUFFER_SIZE
inline constexpr std::size_t platform_console_buffer_size = 1024; // console read_line line buffer
#else
inline constexpr std::size_t platform_console_buffer_size = XASH_LIMIT_PLATFORM_CONSOLE_BUFFER_SIZE;
#endif

#ifndef XASH_LIMIT_PLATFORM_LOG_BUFFER_SIZE
inline constexpr std::size_t platform_log_buffer_size = 2048; // stack buffer for core::logf()
#else
inline constexpr std::size_t platform_log_buffer_size = XASH_LIMIT_PLATFORM_LOG_BUFFER_SIZE;
#endif

} // namespace xash::limits
