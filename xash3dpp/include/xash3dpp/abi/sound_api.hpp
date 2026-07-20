#pragma once
// xash3dpp — vendored frozen client sound interface: channel_t, rawchan_t,
// snd_globals_t, sound_api_t (engine->client, 5 slots), sound_interface_t
// (client->engine, int version + 8 slots) + the support PODs they name.
// Legacy reference: common/sound_api.h (CL_SOUND_INTERFACE_VERSION :37,
// channel_t :76-105, rawchan_t :107-123, snd_globals_t :132-160,
// sound_api_t :170-178, sound_interface_t :181-194).
// Boundary spec: docs/boundaries/sound-boundary.md §External ABI contracts.
//
// Byte-exact mirror of the tables/structs exchanged with a client DLL that
// overrides the sound engine via HUD_GetSoundInterface (a Xash3D FWGS
// extension — cl_game.c:110).  The header is "Experimental implementation,
// backward compatibility is not guaranteed" (sound_api.h:36) yet its layout is
// frozen for any ABI-conformant client: field/slot ORDER, types (incl. the
// LLP64-width uintptr_t reserved padding), and the sole `volatile` qualifier on
// rawchan_t::s_rawend are kept verbatim.  These structs are LAYOUT-PINNED; the
// live game-DLL plumbing through them is deferred to the ABI-shim chunk
// (boundary: "layout-pinned, plumbing deferred").  tests/sound/
// test_sound_api_layout.cpp pins every field offset + struct size against the
// actual legacy header.

#include <xash3dpp/abi/abi_types.hpp> // vec3_t, byte, qboolean (self-contained)

#include <cstddef>
#include <cstdint>

// @annotation-exempt: abi-pod — this header is a wholly-vendored, layout-frozen
// mirror of the client sound interface.  The support records (portable_
// samplepair_t, sfx_t, channel_t, rawchan_t, snd_globals_t, ...) are frozen
// PODs (abi-pod); sound_api_t / sound_interface_t are frozen function-pointer
// tables (fnptr-table).  Field/slot order, types, and the raw cross-link
// pointers are dictated by the ABI — we do not own their design, so the QN
// annotation matrix (per-member @lifetime) does not apply, and @thread-safety
// is a caller/engine contract, not this header's (QN §ANNOTATION_DISCIPLINE).
namespace xash::abi {

// legacy: sound_api.h :37 — negotiated once at client-DLL init.  Experimental;
// no backward-compat guarantee.
inline constexpr int k_cl_sound_interface_version = 1;

// legacy: sound_api.h :47 — opaque sfx handle (index into the sfx table).
using sound_t = std::int32_t;

// legacy: common/const.h — normal (non-low-memory) desktop MAX_QPATH.  The
// SoundAPI is only negotiated on desktop builds, so this is the ABI-effective
// width of sfx_t::name (matches xash3d_types.h:75 MAX_QPATH 64).
inline constexpr std::size_t k_max_qpath = 64;

// legacy: com_model.h :39 NUM_AMBIENTS — automatic ambient sound slots.
inline constexpr std::size_t k_num_ambients = 4;

// legacy: sound_api.h :95 — grace window before an inaudible channel is freed.
inline constexpr float k_max_channel_inaudible_time = 0.1f;

// legacy: sound_api.h :39-45 — voxword / channel flag bits (BIT(n) = 1u << n).
inline constexpr std::uint32_t k_fl_voxword_in_cache      = 1u << 0;
inline constexpr std::uint32_t k_fl_chan_use_loop         = 1u << 0;
inline constexpr std::uint32_t k_fl_chan_static_sound     = 1u << 1;
inline constexpr std::uint32_t k_fl_chan_local_sound      = 1u << 2;
inline constexpr std::uint32_t k_fl_chan_sentence_finished = 1u << 4;
inline constexpr std::uint32_t k_fl_chan_finished         = 1u << 5;

// wavdata_t (engine/common/common.h :534) is referenced only through pointers
// below; a forward declaration is layout-sufficient (pointer width is fixed).
struct wavdata_t;

// legacy: sound_api.h :49-53 — one mixed stereo sample pair (int32 accumulator).
struct portable_samplepair_t
{
    int left;
    int right;
};

// legacy: sound_api.h :55-63 — a registered sound effect + its decode cache.
struct sfx_t
{
    char           name[k_max_qpath];
    wavdata_t     *cache;

    int            servercount;
    std::uint32_t  hashValue;   // legacy: uint
    sfx_t         *hashNext;
};

// legacy: sound_api.h :65-74 — one VOX sentence word (dynamically allocated,
// null-sfx terminates the word list).
struct voxword_t
{
    sfx_t        *sfx;
    std::uint16_t volume;       // volume percent
    std::uint16_t pitch;        // pitch shift percent
    std::uint8_t  timecompress; // percent of skipped data
    std::uint8_t  start;        // percent at which playback starts
    std::uint8_t  end;          // percent at which playback ends
    std::uint8_t  flags;
};

// legacy: sound_api.h :76-105 — one mix channel.  engine_reserved/game_reserved
// are frozen ABI padding shared with the game DLL (LLP64: 8*8 bytes on x64,
// 8*4 on x86).
struct channel_t
{
    char       name[16];    // keep sentence name
    sfx_t     *sfx;         // sfx number

    vec3_t     origin;      // only use if fixed_origin is set
    float      dist_mult;   // distance multiplier (attenuation/clipK)

    int        entchannel;  // sound channel (CHAN_STREAM, CHAN_VOICE, etc.)
    std::uint32_t flags;    // legacy: uint
    std::int16_t entnum;    // entity soundsource   (legacy: short)
    std::int16_t master_vol; // 0-255 master volume
    std::int16_t leftvol;   // 0-255 left volume
    std::int16_t rightvol;  // 0-255 right volume
    std::int16_t basePitch; // base pitch percent (100% = normal)
    std::uint8_t word_index; // legacy: byte

    // HACKHACK: count when this channel became inaudible so it is not freed if
    // it could be respatialized soon (legacy MAX_CHANNEL_INAUDIBLE_TIME).
    float      inauduble_free_time;

    double     sample;
    double     forced_end;
    wavdata_t *data;
    voxword_t *words;       // (num_words + 1) entries, null sfx terminates

    std::uintptr_t engine_reserved[8]; // only for engine developers
    std::uintptr_t game_reserved[8];   // free space for game developers
};

// legacy: sound_api.h :107-123 — one raw/voice streaming channel.  s_rawend is
// the ONLY volatile-qualified field across the whole ABI (a legacy hint that
// raw/voice producers and the mix consumer race on the ring write cursor even
// in legacy intent — the xash3dpp SPSC ring replaces it, boundary G-3 door).
// rawsamples is a flexible array member (contributes 0 to sizeof, like legacy).
#if defined( _MSC_VER )
#  pragma warning( push )
#  pragma warning( disable : 4200 ) // nonstandard extension: FAM — ABI-frozen
#endif
struct rawchan_t
{
    std::int16_t          entnum;
    std::int16_t          master_vol;
    std::int16_t          leftvol;     // 0-255 left volume
    std::int16_t          rightvol;    // 0-255 right volume
    float                 dist_mult;   // distance multiplier (attenuation/clipK)
    vec3_t                origin;      // only use if fixed_origin is set
    volatile std::uint32_t s_rawend;   // legacy: volatile uint (race cursor)
    float                 oldtime;     // catch time jumps

    std::uintptr_t engine_reserved[8]; // only for engine developers
    std::uintptr_t game_reserved[8];   // free space for game developers

    std::size_t           max_samples; // buffer length
    portable_samplepair_t rawsamples[]; // variable sized
};
#if defined( _MSC_VER )
#  pragma warning( pop )
#endif

// legacy: sound_api.h :125-130 — negotiated device sample format.
struct snd_format_t
{
    std::uint32_t speed; // legacy: uint
    std::uint8_t  width;
    std::uint8_t  channels;
};

// legacy: sound_api.h :132-160 — the `snd` global handed to a client DLL's
// pfnS_Init.  channels/raw_channels are `* const` (frozen ABI pointer members);
// the const-ness is retained verbatim (it does not affect layout).
struct snd_globals_t
{
    // dma
    const char   *backend_name;
    std::uint8_t *buffer;      // legacy: byte*
    snd_format_t  format;
    qboolean      initialized; // sound engine is active
    int           samples;     // mono samples in buffer
    int           samplepos;   // in mono samples

    int           paintedtime; // total samples mixed at speed
    int           soundtime;   // total samples played out at dma speed

    // listener (client, camera, etc)
    vec3_t        origin;
    vec3_t        forward, right, up;
    int           entnum;
    qboolean      streaming;      // playing AVI-file
    qboolean      stream_paused;  // pause only background track

    // SoundAPI shared pointers
    channel_t    *const channels;
    int           max_channels;
    int           total_channels;
    rawchan_t   **const raw_channels;
    int           max_raw_channels;
    sound_t       ambient_sfx[k_num_ambients];
    qboolean      have_ambient_sfx;
};

// legacy: sound_api.h :162-167 — voice codec info returned to the client.
struct voice_audio_info_t
{
    std::uint32_t width;      // legacy: uint
    std::uint32_t samplerate;
    std::uint32_t frame_size; // in samples
};

// legacy: sound_api.h :170-178 — API from engine to client (client CALLS
// these).  5 function slots; slot ORDER is the ABI.
struct sound_api_t
{
    qboolean           ( *CL_GetEntitySpatialization )( channel_t *ch );
    sfx_t             *( *S_GetSfxByHandle )( sound_t handle );
    // Voice extensions
    void               ( *pfnS_RawEntSamples )( int entnum, std::uint32_t samples, std::uint32_t rate,
                                                std::uint16_t width, std::uint16_t channels,
                                                const std::uint8_t *data, int snd_vol, float attn );
    void               ( *pfnSND_ForceInitMouth )( int entnum );
    voice_audio_info_t ( *pfnVoice_GetAudioInfo )( void );
};

// legacy: sound_api.h :181-194 — callbacks from client to engine (engine CALLS
// these when a custom sound implementation is active).  Versioned via the
// leading `int version`; 8 function slots.  `ch == NULL` is the documented
// "channel freed" sentinel for both pfnS_Update* callbacks.
struct sound_interface_t
{
    int version;

    qboolean ( *pfnS_Init )( snd_globals_t *globals );
    void     ( *pfnS_Shutdown )( void );
    void     ( *pfnS_UpdateSound )( void );
    // Full paint: endtime (sample pairs), dma buffer, paintedtime in/out.
    void     ( *pfnS_PaintChannels )( int endtime );
    void     ( *pfnS_UpdateChannel )( int ch_idx, const channel_t *ch, sound_t handle ); // ch=NULL -> freed
    void     ( *pfnS_UpdateRawChannel )( int raw_idx, rawchan_t *ch );                    // ch=NULL -> freed
    void     ( *pfnS_Spatialize )( channel_t *ch );
    void     ( *pfnS_FreeSound )( sfx_t *sfx, sound_t handle );
};

} // namespace xash::abi
