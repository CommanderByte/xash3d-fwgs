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

#ifndef XASH_LIMIT_CBUF_SIZE
inline constexpr std::size_t cbuf_size = 256; // max queued command-line entries in the trusted command buffer
#else
inline constexpr std::size_t cbuf_size = XASH_LIMIT_CBUF_SIZE;
#endif

// memory subsystem
#ifndef XASH_LIMIT_MEMORY_POOL_MAX
inline constexpr std::size_t memory_pool_max = 128; // max named memory pools
#else
inline constexpr std::size_t memory_pool_max = XASH_LIMIT_MEMORY_POOL_MAX;
#endif

#ifndef XASH_LIMIT_MEMORY_POOL_NAME_LEN
inline constexpr std::size_t memory_pool_name_len = 64; // max bytes in a pool name (including null terminator)
#else
inline constexpr std::size_t memory_pool_name_len = XASH_LIMIT_MEMORY_POOL_NAME_LEN;
#endif

// host subsystem
#ifndef XASH_LIMIT_HOST_FRAME_ABORT_DETAIL_BUF
inline constexpr std::size_t host_frame_abort_detail_buf = 256; // max bytes for frame-abort detail string (including null terminator)
#else
inline constexpr std::size_t host_frame_abort_detail_buf = XASH_LIMIT_HOST_FRAME_ABORT_DETAIL_BUF;
#endif

// map_loader subsystem
#ifndef XASH_LIMIT_MAP_QPATH_MAX
inline constexpr std::size_t map_qpath_max = 64; // max bytes in a map/landmark name incl. null terminator; matches legacy MAX_QPATH (common/const.h)
#else
inline constexpr std::size_t map_qpath_max = XASH_LIMIT_MAP_QPATH_MAX;
#endif

#ifndef XASH_LIMIT_MAP_BOX_LEAFS_MAX
inline constexpr std::size_t map_box_leafs_max = 256; // box_visible cluster-list capacity; matches legacy MAX_BOX_LEAFS (com_model.h)
#else
inline constexpr std::size_t map_box_leafs_max = XASH_LIMIT_MAP_BOX_LEAFS_MAX;
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

// gameinfo subsystem
// Clamp bounds applied to gameinfo.txt / liblist.gam parsed budgets.
// Lower bounds keep the engine functional; upper bounds reflect what the
// legacy engine and SDK arrays can address without overflow.
#ifndef XASH_LIMIT_GAMEINFO_EDICTS_MIN
inline constexpr int gameinfo_edicts_min = 64; // min parsed value for GameInfo::max_edicts
#else
inline constexpr int gameinfo_edicts_min = XASH_LIMIT_GAMEINFO_EDICTS_MIN;
#endif

#ifndef XASH_LIMIT_GAMEINFO_EDICTS_MAX
inline constexpr int gameinfo_edicts_max = 8192; // max parsed value for GameInfo::max_edicts
#else
inline constexpr int gameinfo_edicts_max = XASH_LIMIT_GAMEINFO_EDICTS_MAX;
#endif

#ifndef XASH_LIMIT_GAMEINFO_TENTS_MIN
inline constexpr int gameinfo_tents_min = 32; // min parsed value for GameInfo::max_tents
#else
inline constexpr int gameinfo_tents_min = XASH_LIMIT_GAMEINFO_TENTS_MIN;
#endif

#ifndef XASH_LIMIT_GAMEINFO_TENTS_MAX
inline constexpr int gameinfo_tents_max = 4096; // max parsed value for GameInfo::max_tents
#else
inline constexpr int gameinfo_tents_max = XASH_LIMIT_GAMEINFO_TENTS_MAX;
#endif

#ifndef XASH_LIMIT_GAMEINFO_BEAMS_MIN
inline constexpr int gameinfo_beams_min = 16; // min parsed value for GameInfo::max_beams
#else
inline constexpr int gameinfo_beams_min = XASH_LIMIT_GAMEINFO_BEAMS_MIN;
#endif

#ifndef XASH_LIMIT_GAMEINFO_BEAMS_MAX
inline constexpr int gameinfo_beams_max = 2048; // max parsed value for GameInfo::max_beams
#else
inline constexpr int gameinfo_beams_max = XASH_LIMIT_GAMEINFO_BEAMS_MAX;
#endif

#ifndef XASH_LIMIT_GAMEINFO_PARTICLES_MIN
inline constexpr int gameinfo_particles_min = 256; // min parsed value for GameInfo::max_particles
#else
inline constexpr int gameinfo_particles_min = XASH_LIMIT_GAMEINFO_PARTICLES_MIN;
#endif

#ifndef XASH_LIMIT_GAMEINFO_PARTICLES_MAX
inline constexpr int gameinfo_particles_max = 65536; // max parsed value for GameInfo::max_particles
#else
inline constexpr int gameinfo_particles_max = XASH_LIMIT_GAMEINFO_PARTICLES_MAX;
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

#ifndef XASH_LIMIT_ZIP_MAX_FILES
inline constexpr std::size_t zip_max_files = 65535; // max entries in a ZIP archive (uint16 total_records)
#else
inline constexpr std::size_t zip_max_files = XASH_LIMIT_ZIP_MAX_FILES;
#endif

#ifndef XASH_LIMIT_ZIP_EOCD_SCAN_MAX
inline constexpr std::size_t zip_eocd_scan_max = 65535; // max EOCD comment-length scanned at archive tail (uint16 max)
#else
inline constexpr std::size_t zip_eocd_scan_max = XASH_LIMIT_ZIP_EOCD_SCAN_MAX;
#endif

#ifndef XASH_LIMIT_FILESYSTEM_ZLIB_INFLATE_BUF
inline constexpr std::size_t filesystem_zlib_inflate_buf = 65536; // ZlibState raw-input chunk buffer
#else
inline constexpr std::size_t filesystem_zlib_inflate_buf = XASH_LIMIT_FILESYSTEM_ZLIB_INFLATE_BUF;
#endif

#ifndef XASH_LIMIT_FILESYSTEM_SEARCH_PATH_MAX
inline constexpr std::size_t filesystem_search_path_max = 256; // informal upper bound for active search paths (std::deque, no reserve)
#else
inline constexpr std::size_t filesystem_search_path_max = XASH_LIMIT_FILESYSTEM_SEARCH_PATH_MAX;
#endif

// clock subsystem
#ifndef XASH_LIMIT_MIN_FRAMETIME
inline constexpr double min_frametime = 0.0001;  // 0.1 ms floor — matches legacy MIN_FRAMETIME
#else
inline constexpr double min_frametime = XASH_LIMIT_MIN_FRAMETIME;
#endif

#ifndef XASH_LIMIT_MAX_FRAMETIME
inline constexpr double max_frametime = 0.25;    // 250 ms ceiling — matches legacy MAX_FRAMETIME
#else
inline constexpr double max_frametime = XASH_LIMIT_MAX_FRAMETIME;
#endif

#ifndef XASH_LIMIT_MIN_FPS
inline constexpr double min_fps = 20.0;          // absolute FPS floor — matches legacy MIN_FPS
#else
inline constexpr double min_fps = XASH_LIMIT_MIN_FPS;
#endif

#ifndef XASH_LIMIT_MAX_FPS_HARD
inline constexpr double max_fps_hard = 1000.0;   // FPS ceiling with fps_override — matches legacy MAX_FPS_HARD
#else
inline constexpr double max_fps_hard = XASH_LIMIT_MAX_FPS_HARD;
#endif

#ifndef XASH_LIMIT_MAX_FPS_SOFT
inline constexpr double max_fps_soft = 200.0;    // FPS ceiling without fps_override — matches legacy MAX_FPS_SOFT
#else
inline constexpr double max_fps_soft = XASH_LIMIT_MAX_FPS_SOFT;
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

#ifndef XASH_LIMIT_PLATFORM_CRASH_FRAMES_MAX
inline constexpr std::size_t platform_crash_frames_max = 64; // max stack frames captured in crash trace
#else
inline constexpr std::size_t platform_crash_frames_max = XASH_LIMIT_PLATFORM_CRASH_FRAMES_MAX;
#endif

#ifndef XASH_LIMIT_PLATFORM_CONSOLE_EVENT_BUF
inline constexpr std::size_t platform_console_event_buf = 64; // win32 console event peek buffer depth
#else
inline constexpr std::size_t platform_console_event_buf = XASH_LIMIT_PLATFORM_CONSOLE_EVENT_BUF;
#endif

#ifndef XASH_LIMIT_PLATFORM_PATH_BUF_WCHARS
inline constexpr std::size_t platform_path_buf_wchars = 1024; // UTF-8→UTF-16 path conversion buffer (wchar_t count)
#else
inline constexpr std::size_t platform_path_buf_wchars = XASH_LIMIT_PLATFORM_PATH_BUF_WCHARS;
#endif

// networking subsystem
// Legacy reference: engine/common/net_ws.h, netchan.h capacity macros.
// These values are wire-frozen for GoldSrc/Xash protocol compatibility; do
// not raise them without a protocol-level review.
#ifndef XASH_LIMIT_NET_MAX_DATAGRAM
inline constexpr std::size_t net_max_datagram = 16384; // max unreliable UDP payload bytes
#else
inline constexpr std::size_t net_max_datagram = XASH_LIMIT_NET_MAX_DATAGRAM;
#endif

#ifndef XASH_LIMIT_NET_MAX_MULTICAST
inline constexpr std::size_t net_max_multicast = 8192; // max multicast payload bytes
#else
inline constexpr std::size_t net_max_multicast = XASH_LIMIT_NET_MAX_MULTICAST;
#endif

#ifndef XASH_LIMIT_NET_MAX_PAYLOAD
inline constexpr std::size_t net_max_payload = 196608; // max netchan message bytes (normal build)
#else
inline constexpr std::size_t net_max_payload = XASH_LIMIT_NET_MAX_PAYLOAD;
#endif

#ifndef XASH_LIMIT_NET_MAX_FRAGMENT
inline constexpr std::size_t net_max_fragment = 65535; // max single split-packet fragment bytes
#else
inline constexpr std::size_t net_max_fragment = XASH_LIMIT_NET_MAX_FRAGMENT;
#endif

#ifndef XASH_LIMIT_NET_MAX_LOOPBACK
inline constexpr std::size_t net_max_loopback = 4; // loopback ring buffer slots per socket
#else
inline constexpr std::size_t net_max_loopback = XASH_LIMIT_NET_MAX_LOOPBACK;
#endif

#ifndef XASH_LIMIT_NET_MAX_FRAGMENTS
inline constexpr std::size_t net_max_fragments = 506; // max split-packet fragment count (Xash protocol)
#else
inline constexpr std::size_t net_max_fragments = XASH_LIMIT_NET_MAX_FRAGMENTS;
#endif

#ifndef XASH_LIMIT_NET_MAX_GOLDSRC_FRAGMENTS
inline constexpr std::size_t net_max_goldsrc_fragments = 5; // max split-packet fragment count (GoldSrc protocol)
#else
inline constexpr std::size_t net_max_goldsrc_fragments = XASH_LIMIT_NET_MAX_GOLDSRC_FRAGMENTS;
#endif

#ifndef XASH_LIMIT_NET_SPLITPACKET_MIN_SIZE
inline constexpr std::size_t net_splitpacket_min_size = 508; // min split fragment body (RFC 791)
#else
inline constexpr std::size_t net_splitpacket_min_size = XASH_LIMIT_NET_SPLITPACKET_MIN_SIZE;
#endif

#ifndef XASH_LIMIT_NET_SPLITPACKET_MAX_SIZE
inline constexpr std::size_t net_splitpacket_max_size = 64000; // max split fragment total
#else
inline constexpr std::size_t net_splitpacket_max_size = XASH_LIMIT_NET_SPLITPACKET_MAX_SIZE;
#endif

#ifndef XASH_LIMIT_NET_MAX_RELIABLE_PAYLOAD
inline constexpr std::size_t net_max_reliable_payload = 1400; // max fragment / reliable packet on wire
#else
inline constexpr std::size_t net_max_reliable_payload = XASH_LIMIT_NET_MAX_RELIABLE_PAYLOAD;
#endif

#ifndef XASH_LIMIT_NET_MAX_STREAMS
inline constexpr std::size_t net_max_streams = 2; // netchan streams: normal data + file download
#else
inline constexpr std::size_t net_max_streams = XASH_LIMIT_NET_MAX_STREAMS;
#endif

#ifndef XASH_LIMIT_NET_PACKET_POOL_SLOTS
inline constexpr std::size_t net_packet_pool_slots = 64; // preallocated packet buffers
#else
inline constexpr std::size_t net_packet_pool_slots = XASH_LIMIT_NET_PACKET_POOL_SLOTS;
#endif

#ifndef XASH_LIMIT_NET_SPLITPACKET_MAX_FRAGMENTS
inline constexpr std::size_t net_splitpacket_max_fragments = 256; // SplitReassembler slots (uint8_t packet_id field range)
#else
inline constexpr std::size_t net_splitpacket_max_fragments = XASH_LIMIT_NET_SPLITPACKET_MAX_FRAGMENTS;
#endif

#ifndef XASH_LIMIT_NET_MAX_FILENAME
inline constexpr std::size_t net_max_filename = 260; // max filename length in file-fragment header (legacy MAX_OSPATH)
#else
inline constexpr std::size_t net_max_filename = XASH_LIMIT_NET_MAX_FILENAME;
#endif

#ifndef XASH_LIMIT_NET_DELTA_MAX_TABLES
inline constexpr std::size_t net_delta_max_tables = 16; // delta description tables (4-bit wire tableIndex)
#else
inline constexpr std::size_t net_delta_max_tables = XASH_LIMIT_NET_DELTA_MAX_TABLES;
#endif

#ifndef XASH_LIMIT_NET_DELTA_MAX_FIELDS
inline constexpr std::size_t net_delta_max_fields = 256; // fields per delta table (8-bit wire nameIndex)
#else
inline constexpr std::size_t net_delta_max_fields = XASH_LIMIT_NET_DELTA_MAX_FIELDS;
#endif

#ifndef XASH_LIMIT_NET_DELTA_ENCODER_NAME
inline constexpr std::size_t net_delta_encoder_name = 32; // custom-encoder function name buffer (legacy funcName[32])
#else
inline constexpr std::size_t net_delta_encoder_name = XASH_LIMIT_NET_DELTA_ENCODER_NAME;
#endif

#ifndef XASH_LIMIT_NET_DELTA_GS_MASK_BYTES
inline constexpr std::size_t net_delta_gs_mask_bytes = 8; // GoldSrc changed-field mask byte groups (3-bit count caps writes at 7)
#else
inline constexpr std::size_t net_delta_gs_mask_bytes = XASH_LIMIT_NET_DELTA_GS_MASK_BYTES;
#endif

// server subsystem
// Legacy reference: engine/server/sv_game.c SV_AllocStringPool (:3074) —
// string arena sized 65536 * ceil(max_edicts / 1024), two halves
// (dynamic + static phase).
#ifndef XASH_LIMIT_SERVER_STRING_BLOCK
inline constexpr std::size_t server_string_block = 65536; // string arena bytes per edict quantum
#else
inline constexpr std::size_t server_string_block = XASH_LIMIT_SERVER_STRING_BLOCK;
#endif

#ifndef XASH_LIMIT_SERVER_STRING_QUANTUM
inline constexpr std::size_t server_string_quantum = 1024; // max_edicts per string-arena block
#else
inline constexpr std::size_t server_string_quantum = XASH_LIMIT_SERVER_STRING_QUANTUM;
#endif

} // namespace xash::limits
