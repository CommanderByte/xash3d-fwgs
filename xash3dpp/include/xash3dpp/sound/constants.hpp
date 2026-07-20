#pragma once
// xash3dpp — sound entry-surface constants (Chunk 9, slice S9.6).
// Legacy reference: common/const.h:610-632 (CHAN_*/ATTN_*/PITCH_NORM/
// VOL_NORM — shared client+server vocabulary) + engine/common/protocol.h:
// 139-151 (SND_* — the S_StartSound `flags` bitfield; also used by the
// networking svc_sound wire format, sibling-scope, not re-derived here).
//
// Not yet ported anywhere else in xash3dpp; defined here since sound's
// public entry surface (start_sound()'s `channel`/`attn`/`pitch`/`flags`
// parameters) needs concrete values to be USABLE by callers. If/when
// networking's svc_sound work lands, it may want to reference these
// directly rather than duplicate them (see the S9.6 uncertainty list).
//
// @thread-safety: pure constants — safe from any thread.

#include <cstdint>

namespace xash::sound {

// ---------------------------------------------------------------------------
// Channel numbers (const.h:610-618). CHAN_NETWORKVOICE_* reserve a wire-only
// range not used by the engine-side entry surface — omitted.
// ---------------------------------------------------------------------------
inline constexpr int k_chan_auto   = 0;
inline constexpr int k_chan_weapon = 1;
inline constexpr int k_chan_voice  = 2;
inline constexpr int k_chan_item   = 3;
inline constexpr int k_chan_body   = 4;
inline constexpr int k_chan_stream = 5; // allocate from static OR dynamic area
inline constexpr int k_chan_static = 6; // allocate from the static area

// ---------------------------------------------------------------------------
// Attenuation (const.h:621-624).
// ---------------------------------------------------------------------------
inline constexpr float k_attn_none   = 0.0f;
inline constexpr float k_attn_norm   = 0.8f;
inline constexpr float k_attn_idle   = 2.0f;
inline constexpr float k_attn_static = 1.25f;

// ---------------------------------------------------------------------------
// Pitch / volume (const.h:627,632).
// ---------------------------------------------------------------------------
inline constexpr int   k_pitch_norm_flag = 100; // PITCH_NORM (mixer.hpp's k_pitch_norm mirrors this value)
inline constexpr float k_vol_norm        = 1.0f;

// SND_CLIP_DISTANCE (engine/client/sound.h:48) — the attn -> dist_mult divisor.
inline constexpr float k_sound_clip_distance = 1000.0f;

// ---------------------------------------------------------------------------
// S_StartSound `flags` bitfield (protocol.h:139-151). SND_VOLUME/
// SND_ATTENUATION/SND_PITCH/SND_FILTER_CLIENT/SND_RESTORE_POSITION are
// svc_sound WIRE-only bits (networking's concern, consumed before
// S_StartSound is ever called) — defined here for completeness/parity but
// not tested by any S9.6 entry-surface function, matching the verified
// legacy S_StartSound body (engine/client/sound/s_main.c:626-740), which
// only branches on SND_STOP/SND_CHANGE_VOL/SND_CHANGE_PITCH/
// SND_STOP_LOOPING/SND_LOCALSOUND.
// ---------------------------------------------------------------------------
inline constexpr std::uint32_t k_snd_volume           = 1u << 0;
inline constexpr std::uint32_t k_snd_attenuation       = 1u << 1;
inline constexpr std::uint32_t k_snd_sequence          = 1u << 2;
inline constexpr std::uint32_t k_snd_pitch             = 1u << 3;
inline constexpr std::uint32_t k_snd_sentence          = 1u << 4;
inline constexpr std::uint32_t k_snd_stop              = 1u << 5;
inline constexpr std::uint32_t k_snd_change_vol        = 1u << 6;
inline constexpr std::uint32_t k_snd_change_pitch      = 1u << 7;
inline constexpr std::uint32_t k_snd_spawning          = 1u << 8;
inline constexpr std::uint32_t k_snd_localsound        = 1u << 9;
inline constexpr std::uint32_t k_snd_stop_looping      = 1u << 10;
inline constexpr std::uint32_t k_snd_filter_client     = 1u << 11;
inline constexpr std::uint32_t k_snd_restore_position  = 1u << 12;

} // namespace xash::sound
