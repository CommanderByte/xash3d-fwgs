#pragma once
// xash3dpp — channel allocation + alter/spatialize (Chunk 9, slice S9.6).
// PARITY-CRITICAL. Legacy reference: engine/client/sound/s_main.c —
// SND_GetChannelTimeLeft (:284-331), SND_FStreamIsPlaying (:267-275),
// SND_PickDynamicChannel (:342-416), SND_PickStaticChannel (:428-462),
// S_MaybeAlterChannel (:464-500), S_AlterChannel (:514-532),
// S_SpatializeChannel (:539-552), SND_Spatialize (:559-608).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Interface (start_sound/
// stop_sound), §Quirks ("Channel selection", "Alter/stop paths"), §Threading
// (SND-OQ-1: providers/listener pose read ONLY on T_Main).
//
// These are free functions over an explicit `std::span<MixChannel>` (the
// P-5 narrowest-state-signature idiom) instead of methods on a stateful
// class — the channel ARRAY (with its [ambient][dynamic][static) index
// partition, mirroring `snd.channels[]`) is owned by Sound::Impl (sound.cpp),
// which calls these in the exact legacy operation order (alter-check -> pick
// -> init channel state -> immediate spatialize).
//
// @thread-safety: T_Main only (legacy: the entire mixer/allocation tree is
// T_Main; SND-OQ-1 keeps it that way for the entry surface — the mix-side
// paint loop, T_AudioDecoder, never calls any of these). Not asserted here;
// the owning Sound::* entry points assert ThreadRole::Main (sound.cpp).

#include <xash3dpp/private/sound/mixer.hpp> // MixChannel
#include <xash3dpp/private/sound/registry.hpp> // SfxHandle
#include <xash3dpp/private/sound/vox.hpp>   // IVoxTimeLeftQuery, VoxSystem
#include <xash3dpp/sound/providers.hpp>     // Vec3, IEntitySpatialProvider

#include <cstdint>
#include <span>
#include <string_view>

namespace xash::sound {

// ---------------------------------------------------------------------------
// free_channel — S_FreeChannel (s_main.c:184-200), MINUS the SoundAPI
// client-override notify (S_NotifyChannelUpdate — the pfnS_UpdateChannel
// dispatch; deferred per the boundary's "layout-pinned, plumbing deferred"
// ABI note) and MINUS SND_CloseMouth (the mouth-animation close hook; not
// modelled this slice — only IMouthSink's mix-time write-back is a boundary
// deliverable, and even that is not wired here). Tears down a VOX binding
// via `vox` (VOX_FreeWord + Mem_Free2(&ch->words), i.e.
// VoxSystem::unbind_channel) when `ch.is_sentence`, then zeroes the
// remaining identity/state fields exactly like legacy's channel_t reset.
// ---------------------------------------------------------------------------
void free_channel( MixChannel &ch, VoxSystem *vox ) noexcept;

// ---------------------------------------------------------------------------
// channel_time_left — SND_GetChannelTimeLeft (s_main.c:284-331).
// ---------------------------------------------------------------------------
[[nodiscard]] int channel_time_left( const MixChannel &ch, IVoxTimeLeftQuery *vox ) noexcept;

// ---------------------------------------------------------------------------
// snd_stream_is_playing — SND_FStreamIsPlaying (s_main.c:267-275): scan the
// dynamic range [NUM_AMBIENTS, MAX_DYNAMIC_CHANNELS) for a CHAN_STREAM match
// on `sfx`. `channels` must already cover at least that range.
// ---------------------------------------------------------------------------
[[nodiscard]] bool snd_stream_is_playing( std::span<const MixChannel> channels, SfxHandle sfx ) noexcept;

// ---------------------------------------------------------------------------
// pick_dynamic_channel — SND_PickDynamicChannel (s_main.c:342-416).
// `channels` MUST already cover [0, MAX_DYNAMIC_CHANNELS) (ambient + dynamic
// partitions); only [NUM_AMBIENTS, MAX_DYNAMIC_CHANNELS) is scanned. Frees
// the chosen victim's VOX binding via `vox` (S_FreeChannel's
// `if (ch->words) VOX_FreeWord(ch)` half, s_main.c:189-190) when non-null —
// the reset-to-zero half is the CALLER's job (S_StartSound's
// `memset(target_chan,0,...)`, s_main.c:670).
// ---------------------------------------------------------------------------
struct DynamicPickResult
{
    int  index  = -1;    // -1 == no channel; see `ignore`
    bool ignore = false; // true == caller must NOT log "dropped sound" (silent no-op)
};

[[nodiscard]] DynamicPickResult pick_dynamic_channel( std::span<MixChannel> channels, int listener_entnum,
                                                       int entnum, int channel, SfxHandle sfx,
                                                       IVoxTimeLeftQuery *time_left_source,
                                                       VoxSystem *vox ) noexcept;

// ---------------------------------------------------------------------------
// pick_static_channel — SND_PickStaticChannel (s_main.c:428-462).
// `total_channels` mirrors snd.total_channels (in/out: bumped when a NEW
// static slot beyond the previous high-water mark is allocated). `channels`
// must already cover [0, sound_max_channels) (the full fixed array — static
// slots are appended past MAX_DYNAMIC_CHANNELS up to that capacity). Returns
// -1 on overflow (`total_channels == max_channels`, s_main.c:451-455) — the
// caller logs "no free channels" (Con_DPrintf S_ERROR).
// ---------------------------------------------------------------------------
[[nodiscard]] int pick_static_channel( std::span<MixChannel> channels, int &total_channels, const Vec3 &pos,
                                       SfxHandle sfx ) noexcept;

// ---------------------------------------------------------------------------
// alter_channel — S_MaybeAlterChannel (s_main.c:464-500) + S_AlterChannel
// (s_main.c:514-532). `channels`/`total_channels` scope the scan to
// [NUM_AMBIENTS, total_channels) (ambients excluded, s_main.c:522).
// `sfx_name` is the NEW sfx's name (for the `S_TestSoundChar(sfx->name,'!')`
// is_sentence test, s_main.c:520) — pass "" only when `sfx` identifies a
// genuinely-invalid/absent sfx (S_StopSound's `S_FindName` can return such a
// slot; matches legacy, which still dereferences sfx->name unconditionally —
// this port requires a valid name string instead, see channel_alloc.cpp).
// `vox` frees a SND_STOP victim's VOX binding, mirroring S_FreeChannel.
// Returns true iff a channel was altered (found + STOP/CHANGE applied).
// ---------------------------------------------------------------------------
[[nodiscard]] bool alter_channel( std::span<MixChannel> channels, int total_channels, int entnum, int channel,
                                  SfxHandle sfx, std::string_view sfx_name, int vol, int pitch, std::uint32_t flags,
                                  VoxSystem *vox ) noexcept;

// ---------------------------------------------------------------------------
// spatialize_pan — S_SpatializeChannel (s_main.c:539-552): the pan/
// attenuation math only (no client/entity/listener state).
// ---------------------------------------------------------------------------
struct SpatializePan
{
    int left_vol  = 0;
    int right_vol = 0;
};

[[nodiscard]] SpatializePan spatialize_pan( int master_vol, float dot, float dist ) noexcept;

// ---------------------------------------------------------------------------
// spatialize — SND_Spatialize (s_main.c:559-608), MINUS the pfnS_Spatialize
// client-override dispatch (a Sound-level concern, not this free-function
// layer — see sound.cpp) and MINUS VOX_SetChanVol (caller applies via
// VoxSystem::apply_word_volume for chan.is_sentence channels — matches
// legacy's own call site immediately after this function returns,
// s_main.c:574,607).
//
// `provider` resolves per-entity origins for non-static channels
// (IEntitySpatialProvider, SND-OQ-1 — T_Main only); null -> the channel is
// zeroed (matches `!CL_GetEntitySpatialization(ch)`, s_main.c:580-585).
// ---------------------------------------------------------------------------
void spatialize( MixChannel &ch, int listener_entnum, const Vec3 &listener_origin, const Vec3 &listener_right,
                 bool bugcomp_attn_none, IEntitySpatialProvider *provider ) noexcept;

// ---------------------------------------------------------------------------
// spatialize_with_origin — the SND-OQ-1 thread split of spatialize() (S9.7b).
// IDENTICAL math; the ONLY difference is that the provider call has already
// happened and its result is passed in:
//   `entity_origin_valid == false` reproduces `!CL_GetEntitySpatialization(ch)`
//   (volumes zeroed, s_main.c:580-585); `true` supplies the origin the provider
//   returned.  Both are ignored when the channel never consults the provider at
//   all (view-entity channel, or FL_CHAN_STATIC_SOUND).
//
// spatialize() above is now a thin wrapper: it performs the provider call and
// forwards here, so single-threaded callers are byte-for-byte unchanged.
//
// @thread-safety: pure math over the arguments — runs wherever the channel
// array lives (T_AudioDecoder under the topology, T_Main without it).  The
// provider itself is never touched here, which is precisely what lets the
// decoder run the legacy call site while SND-OQ-1 keeps providers on T_Main.
// ---------------------------------------------------------------------------
void spatialize_with_origin( MixChannel &ch, int listener_entnum, const Vec3 &listener_origin,
                             const Vec3 &listener_right, bool bugcomp_attn_none, bool entity_origin_valid,
                             const Vec3 &entity_origin ) noexcept;

// True iff spatialize() would consult IEntitySpatialProvider for a channel with
// these identity fields — i.e. the channel is not the listener's own and is not
// a FL_CHAN_STATIC_SOUND channel (s_main.c:568-586).  T_Main uses this to make
// EXACTLY the provider calls legacy would, no more (S9.7b).
[[nodiscard]] constexpr bool spatialize_needs_provider( int entnum, int listener_entnum,
                                                        std::uint32_t chan_flags ) noexcept
{
    if( entnum == listener_entnum )
        return false;
    return ( chan_flags & ::xash::abi::k_fl_chan_static_sound ) == 0;
}

} // namespace xash::sound
