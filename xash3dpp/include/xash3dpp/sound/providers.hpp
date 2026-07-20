#pragma once
// xash3dpp — sound cross-thread provider PODs + provider/sink interfaces
// (Chunk 9, slice S9.1).  The SND-OQ-1 resolved shape: providers read client
// state ONLY on T_Main; immutable POD snapshots ship down the audio command
// stream (P-2); the mix/decoder thread never re-reads live cl.*/host.* state.
// Legacy reference: engine/client/sound/{s_main.c (S_UpdateFrame listener
// publish), s_mix.c (menu/key_dest gates), s_dsp.c (waterlevel room select)},
// R9.5 client-state coupling recon.
// Boundary spec: docs/boundaries/sound-boundary.md §Dependencies (R9.5 three
// groups), §Threading (SND-OQ-1 resolved), §Extension axes (P-2).

#include <xash3dpp/abi/sound_api.hpp>   // ::xash::abi::sound_t, k_num_ambients
#include <xash3dpp/utilities/math.hpp>  // ::xash::utilities::Vec3

#include <array>
#include <cstdint>

namespace xash::sound {

using Vec3 = ::xash::utilities::Vec3;

// ---------------------------------------------------------------------------
// ListenerSnapshot (P-2) — the per-frame listener pose + timing/environment
// the mixer needs, captured on T_Main at the S_UpdateFrame publish point and
// shipped one-way down the command stream (R9.5 groups a/b/c).  Replaces the
// live-read snd_globals_t.{origin,forward,right,up,entnum} members.
// ---------------------------------------------------------------------------
struct ListenerSnapshot
{
    // group a — listener pose (snd_globals_t origin/forward/right/up/entnum,
    //   published by S_UpdateFrame, s_main.c:1590).
    Vec3 origin  {};
    Vec3 forward {};
    Vec3 right   {};
    Vec3 up      {};
    int  entnum = 0;

    // group b — per-frame timing (ambient volume ramp + soundfade advance).
    float frametime = 0.0f;

    // group c — DSP room-selection input (legacy reads cl.local.waterlevel on
    //   T_Main; only the derived idsp_room index crosses to the mix thread).
    int waterlevel = 0;
};

// ---------------------------------------------------------------------------
// MixGateSnapshot (P-2) — the menu/pause/focus gating booleans that legacy
// keys off cls.key_dest / cl.paused / cl.background / Host_IsSinglePlayerGame()
// scattered inside the mix loop (s_mix.c:333-349,562-566, R9.5 group f),
// consolidated into one per-frame POD.  The mixer must apply them in the same
// branch order to preserve behaviour (e.g. console + local sound still plays).
// ---------------------------------------------------------------------------
struct MixGateSnapshot
{
    bool  paused        = false; // cl.paused
    bool  background    = false; // cl.background (demo / background map)
    bool  in_menu       = false; // cls.key_dest == key_menu
    bool  single_player = false; // Host_IsSinglePlayerGame()
    bool  lost_focus    = false; // snd_mute_losefocus focus-mute gate

    // soundfade -> master gain (the fade_state_ control->mix crossing; R9.5
    //   marshals this instead of a live read).  1.0 == no fade.
    float soundfade_gain = 1.0f;
};

// ---------------------------------------------------------------------------
// RegistrationSnapshot (P-2) — registration-time data folded once per
// registration event (R9.5 group g); the ambient sfx handle set legacy caches
// in snd_globals_t.ambient_sfx[]/have_ambient_sfx.
// ---------------------------------------------------------------------------
struct RegistrationSnapshot
{
    std::array<::xash::abi::sound_t, ::xash::abi::k_num_ambients> ambient_sfx {};
    bool have_ambient_sfx = false;
};

// ---------------------------------------------------------------------------
// IEntitySpatialProvider (P-5) — the CL_GetEntitySpatialization /
// CL_GetMovieSpatialization seam (R9.5 group d).  Resolves a channel origin
// from live client entity state.
//
// @thread-safety: T_Main ONLY.  The client entity array is mutated by netchan
// parse on T_Main; an off-Main deref races the parse (SND-OQ-1).  Only the
// resolved origin crosses to the mix thread, as a channel-param update on the
// command stream.
// ---------------------------------------------------------------------------
class IEntitySpatialProvider
{
public:
    IEntitySpatialProvider() noexcept                                        = default;
    virtual ~IEntitySpatialProvider()                                        = default;
    IEntitySpatialProvider( const IEntitySpatialProvider & )                 = delete;
    IEntitySpatialProvider &operator=( const IEntitySpatialProvider & )      = delete;

    // Resolve entity `entnum`'s spatial origin (legacy CL_GetEntitySpatialization
    // fills ch->origin).  Returns false if the entity is not spatializable
    // (legacy false -> the caller drops the channel); on true, `origin` is set.
    // @thread-safety: caller guarantees T_Main.
    [[nodiscard]] virtual bool resolve_origin( int entnum, Vec3 &origin ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// IMouthSink (P-5) — the mouth-animation write-back seam (R9.5 group e).  The
// mix computes `mouthopen` for voice/stream/raw channels; this marshals it back
// toward client state.  The SOLE reverse-direction data flow from sound into
// client state.
//
// @thread-safety: called from the mix side (T_AudioDecoder); the implementation
// marshals to T_Main or writes an atomic per-entity slot — it must NOT mutate
// cl_entity_t.mouth directly off T_Main (SND-OQ-1).  The sndavg/sndcount
// accumulators stay mix-private and never cross this seam.
// ---------------------------------------------------------------------------
class IMouthSink
{
public:
    IMouthSink() noexcept                             = default;
    virtual ~IMouthSink()                             = default;
    IMouthSink( const IMouthSink & )                  = delete;
    IMouthSink &operator=( const IMouthSink & )       = delete;

    // Commit the computed mouth-open amplitude for entity `entnum`.
    virtual void set_mouth_open( int entnum, int mouthopen ) noexcept = 0;
};

} // namespace xash::sound
