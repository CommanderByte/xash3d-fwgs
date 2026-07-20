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

#ifndef XASH_LIMIT_PLATFORM_THREAD_NAME_MAX
inline constexpr std::size_t platform_thread_name_max = 32; // spawn_thread() debugger-visible name buffer (bytes, incl. null terminator); POSIX pthread_setname_np further truncates to 16 bytes on Linux/Android
#else
inline constexpr std::size_t platform_thread_name_max = XASH_LIMIT_PLATFORM_THREAD_NAME_MAX;
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
// Legacy reference: engine/common/world.h :32-33 (AREA_NODES/AREA_DEPTH)
// and engine/server/sv_game.c SV_AllocStringPool (:3074) — string arena
// sized 65536 * ceil(max_edicts / 1024), two halves (dynamic + static).
#ifndef XASH_LIMIT_SERVER_AREA_NODES
inline constexpr std::size_t server_area_nodes = 32; // areanode pool (worst-case 2^(depth+1)-1 = 31)
#else
inline constexpr std::size_t server_area_nodes = XASH_LIMIT_SERVER_AREA_NODES;
#endif

#ifndef XASH_LIMIT_SERVER_AREA_DEPTH
inline constexpr std::size_t server_area_depth = 4; // areanode subdivision depth
#else
inline constexpr std::size_t server_area_depth = XASH_LIMIT_SERVER_AREA_DEPTH;
#endif

#ifndef XASH_LIMIT_SERVER_LIGHTSTYLES
inline constexpr std::size_t server_lightstyles = 256; // MAX_LIGHTSTYLES (protocol limit, FWGS raised from 64)
#else
inline constexpr std::size_t server_lightstyles = XASH_LIMIT_SERVER_LIGHTSTYLES;
#endif

#ifndef XASH_LIMIT_SERVER_LIGHTSTYLE_PATTERN
inline constexpr std::size_t server_lightstyle_pattern = 256; // lightstyle_t pattern/map buffer
#else
inline constexpr std::size_t server_lightstyle_pattern = XASH_LIMIT_SERVER_LIGHTSTYLE_PATTERN;
#endif

// Physics scratch — engine/server/server.h :59 (MAX_PUSHED_ENTS) and
// engine/server/sv_phys.c :44 (MAX_CLIP_PLANES).  The pusher stack has no
// legacy overflow check; the rewrite bound-checks + logs but keeps the cap.
#ifndef XASH_LIMIT_SERVER_PUSHED_ENTS
inline constexpr std::size_t server_pushed_ents = 256; // MAX_PUSHED_ENTS
#else
inline constexpr std::size_t server_pushed_ents = XASH_LIMIT_SERVER_PUSHED_ENTS;
#endif

#ifndef XASH_LIMIT_SERVER_CLIP_PLANES
inline constexpr std::size_t server_clip_planes = 5; // MAX_CLIP_PLANES (FlyMove)
#else
inline constexpr std::size_t server_clip_planes = XASH_LIMIT_SERVER_CLIP_PLANES;
#endif

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

// Precache table capacities — engine/common/protocol.h :109-127 (the
// non-low-memory branch this fork targets); each is a wire bit-width
// (12/11/10/10 bits) and therefore a protocol constant, not tunable.
#ifndef XASH_LIMIT_SV_MAX_MODELS
inline constexpr std::size_t sv_max_models = 4096; // MAX_MODELS (12 bits)
#else
inline constexpr std::size_t sv_max_models = XASH_LIMIT_SV_MAX_MODELS;
#endif

#ifndef XASH_LIMIT_SV_MAX_SOUNDS
inline constexpr std::size_t sv_max_sounds = 2048; // MAX_SOUNDS (11 bits)
#else
inline constexpr std::size_t sv_max_sounds = XASH_LIMIT_SV_MAX_SOUNDS;
#endif

#ifndef XASH_LIMIT_SV_MAX_EVENTS
inline constexpr std::size_t sv_max_events = 1024; // MAX_EVENTS (10 bits)
#else
inline constexpr std::size_t sv_max_events = XASH_LIMIT_SV_MAX_EVENTS;
#endif

#ifndef XASH_LIMIT_SV_MAX_GENERIC
inline constexpr std::size_t sv_max_generic = 1024; // MAX_CUSTOM generic files (10 bits)
#else
inline constexpr std::size_t sv_max_generic = XASH_LIMIT_SV_MAX_GENERIC;
#endif

// content subsystem
#ifndef XASH_LIMIT_CONTENT_MAX_MODELS
inline constexpr std::size_t content_max_models = 4096; // model-cache slot cap; matches the protocol MAX_MODELS precache width (see sv_max_models)
#else
inline constexpr std::size_t content_max_models = XASH_LIMIT_CONTENT_MAX_MODELS;
#endif

// ABI-frozen: match the studiohdr array bounds (engine/studio.h). The bone
// solver's per-frame scratch (studio_bones[], boneused[], adj[]) is sized by
// these; raising them is a studio-format compatibility change.
#ifndef XASH_LIMIT_STUDIO_MAX_BONES
inline constexpr std::size_t studio_max_bones = 128; // matches legacy MAXSTUDIOBONES
#else
inline constexpr std::size_t studio_max_bones = XASH_LIMIT_STUDIO_MAX_BONES;
#endif

#ifndef XASH_LIMIT_STUDIO_MAX_CONTROLLERS
inline constexpr std::size_t studio_max_controllers = 32; // matches legacy MAXSTUDIOCONTROLLERS
#else
inline constexpr std::size_t studio_max_controllers = XASH_LIMIT_STUDIO_MAX_CONTROLLERS;
#endif

// save subsystem
// ON-DISK FORMAT constants baked into the .sav/.HL1-3 container (a file is
// written and read against these exact bounds, and by external tools), NOT
// tunable implementation budgets.  Legacy reference: engine/server/sv_save.c.
// NOTE: max level connections (16) is NOT redefined here — it is the frozen ABI
// value ::xash::abi::k_max_level_connections (abi/eiface.hpp:149, eiface.h:318,
// the SAVERESTOREDATA.levelList[] bound); save code uses that constant directly.
#ifndef XASH_LIMIT_SAVE_HEAP_SIZE
inline constexpr std::size_t save_heap_size = 0x400000; // SAVE_HEAPSIZE — 4 MiB working buffer (sv_save.c:36)
#else
inline constexpr std::size_t save_heap_size = XASH_LIMIT_SAVE_HEAP_SIZE;
#endif

#ifndef XASH_LIMIT_SAVE_HASH_STRINGS
inline constexpr std::size_t save_hash_strings = 0xFFF; // SAVE_HASHSTRINGS — 4095 max unique tokens (sv_save.c:37)
#else
inline constexpr std::size_t save_hash_strings = XASH_LIMIT_SAVE_HASH_STRINGS;
#endif

#ifndef XASH_LIMIT_SAVE_CONTAINER_NAME_FIELD
inline constexpr std::size_t save_container_name_field = 260; // FORMAT field width (NOT an OS path limit): zero-padded embedded-record name[MAX_OSPATH] in each .sav container record (sv_save.c:497,647-706)
#else
inline constexpr std::size_t save_container_name_field = XASH_LIMIT_SAVE_CONTAINER_NAME_FIELD;
#endif

// sound subsystem
// Legacy reference: engine/client/sound.h (mixer limits) + common/com_model.h
// (NUM_AMBIENTS).  The channel counts are ABI-frozen: MAX_CHANNELS sizes
// snd_globals_t.channels[] (sound_api.h), which the client/game DLL walks —
// raising them is an ABI-compatibility change, not a tunable budget.
#ifndef XASH_LIMIT_SOUND_NUM_AMBIENT_CHANNELS
inline constexpr std::size_t sound_num_ambient_channels = 4; // NUM_AMBIENTS (common/com_model.h:39)
#else
inline constexpr std::size_t sound_num_ambient_channels = XASH_LIMIT_SOUND_NUM_AMBIENT_CHANNELS;
#endif

#ifndef XASH_LIMIT_SOUND_NUM_DYNAMIC_CHANNELS
inline constexpr std::size_t sound_num_dynamic_channels = 60; // dynamic portion of MAX_DYNAMIC_CHANNELS = 60 + NUM_AMBIENTS (engine/client/sound.h:44)
#else
inline constexpr std::size_t sound_num_dynamic_channels = XASH_LIMIT_SOUND_NUM_DYNAMIC_CHANNELS;
#endif

#ifndef XASH_LIMIT_SOUND_NUM_STATIC_CHANNELS
inline constexpr std::size_t sound_num_static_channels = 256; // static-channel headroom (engine/client/sound.h:45, "Scourge Of Armagon has too many static sounds")
#else
inline constexpr std::size_t sound_num_static_channels = XASH_LIMIT_SOUND_NUM_STATIC_CHANNELS;
#endif

// MAX_CHANNELS = 256 + MAX_DYNAMIC_CHANNELS = 256 + (60 + NUM_AMBIENTS) = 320
// (engine/client/sound.h:44-45).  Composed from the sub-constants above so the
// 4 + 60 + 256 breakdown stays self-consistent; ABI-frozen at 320.
inline constexpr std::size_t sound_max_channels =
    sound_num_static_channels + sound_num_dynamic_channels + sound_num_ambient_channels;

#ifndef XASH_LIMIT_SOUND_MAX_RAW_CHANNELS
inline constexpr std::size_t sound_max_raw_channels = 48; // MAX_RAW_CHANNELS (engine/client/sound.h:46)
#else
inline constexpr std::size_t sound_max_raw_channels = XASH_LIMIT_SOUND_MAX_RAW_CHANNELS;
#endif

#ifndef XASH_LIMIT_SOUND_MAX_RAW_SAMPLES
inline constexpr std::size_t sound_max_raw_samples = 16384; // MAX_RAW_SAMPLES (engine/client/sound.h:47)
#else
inline constexpr std::size_t sound_max_raw_samples = XASH_LIMIT_SOUND_MAX_RAW_SAMPLES;
#endif

#ifndef XASH_LIMIT_SOUND_PAINTBUFFER_SIZE
inline constexpr std::size_t sound_paintbuffer_size = 1024; // PAINTBUFFER_SIZE (engine/client/sound.h:31)
#else
inline constexpr std::size_t sound_paintbuffer_size = XASH_LIMIT_SOUND_PAINTBUFFER_SIZE;
#endif

// SOUND_DMA_SPEED = SOUND_44k = 44100 (engine/client/sound.h:27,29): the
// GoldSrc hardware playback rate; the SPSC ring / DeviceSpec default (SND-OQ-5).
#ifndef XASH_LIMIT_SOUND_DMA_SPEED
inline constexpr std::uint32_t sound_dma_speed = 44100;
#else
inline constexpr std::uint32_t sound_dma_speed = XASH_LIMIT_SOUND_DMA_SPEED;
#endif

// VOX (sentence word-sequencer, Chunk 9 slice S9.4). Legacy reference:
// engine/client/sound/s_vox.c + engine/client/sound.h.
#ifndef XASH_LIMIT_SOUND_VOX_SENTENCE_TABLE_MAX
inline constexpr std::size_t sound_vox_sentence_table_max = 4096; // CVOXFILESENTENCEMAX (s_vox.c:25)
#else
inline constexpr std::size_t sound_vox_sentence_table_max = XASH_LIMIT_SOUND_VOX_SENTENCE_TABLE_MAX;
#endif

#ifndef XASH_LIMIT_SOUND_VOX_WORD_MAX
inline constexpr std::size_t sound_vox_word_max = 64; // CVOXWORDMAX (engine/client/sound.h:38)
#else
inline constexpr std::size_t sound_vox_word_max = XASH_LIMIT_SOUND_VOX_WORD_MAX;
#endif

#ifndef XASH_LIMIT_SOUND_VOX_DIR_MAX
inline constexpr std::size_t sound_vox_dir_max = 32; // VOX_LoadSound's szpath[32] (s_vox.c:446)
#else
inline constexpr std::size_t sound_vox_dir_max = XASH_LIMIT_SOUND_VOX_DIR_MAX;
#endif

#ifndef XASH_LIMIT_SOUND_VOX_SENTENCE_TEXT_MAX
inline constexpr std::size_t sound_vox_sentence_text_max = 512; // VOX_LoadSound's buffer[512] (s_vox.c:446)
#else
inline constexpr std::size_t sound_vox_sentence_text_max = XASH_LIMIT_SOUND_VOX_SENTENCE_TEXT_MAX;
#endif

#ifndef XASH_LIMIT_SOUND_VOX_IMMEDIATE_NAME_MAX
inline constexpr std::size_t sound_vox_immediate_name_max = 256; // s_sentenceImmediateName's `string` (MAX_STRING, common/xash3d_types.h:12)
#else
inline constexpr std::size_t sound_vox_immediate_name_max = XASH_LIMIT_SOUND_VOX_IMMEDIATE_NAME_MAX;
#endif

// input subsystem
// Legacy reference: engine/client/input/in_keys.c:37-43 (keys[265] — ~255
// real keys + 9 international slots) and in_touch.c's touch_button_t fixed
// char arrays (name[32], texture[256], command[256], in_touch.c:57-59).
#ifndef XASH_LIMIT_INPUT_KEY_COUNT
inline constexpr std::size_t input_key_count = 265; // ARRAYSIZE(keys) — keeps xash::input::k_key_count in sync
#else
inline constexpr std::size_t input_key_count = XASH_LIMIT_INPUT_KEY_COUNT;
#endif

#ifndef XASH_LIMIT_INPUT_TOUCH_NAME_MAX
inline constexpr std::size_t input_touch_name_max = 32; // touch_button_t::name[32]
#else
inline constexpr std::size_t input_touch_name_max = XASH_LIMIT_INPUT_TOUCH_NAME_MAX;
#endif

#ifndef XASH_LIMIT_INPUT_TOUCH_FIELD_MAX
inline constexpr std::size_t input_touch_field_max = 256; // touch_button_t::texture[256] / command[256]
#else
inline constexpr std::size_t input_touch_field_max = XASH_LIMIT_INPUT_TOUCH_FIELD_MAX;
#endif

} // namespace xash::limits
