#pragma once
// xash3dpp — sfx registry: S_FindName / S_RegisterSound / S_LoadSound /
// S_FreeSound / S_GetSfxByHandle port (Chunk 9, slice S9.6). PARITY-CRITICAL
// (name resolution + the LAZY decode-at-play-time timing).
// Legacy reference: engine/client/sound/s_load.c (S_FindName :149-204,
// S_RegisterSound :312-336, S_LoadSound :103-141, S_FreeSound :211-244,
// S_GetSfxByHandle :338-351, S_CreateDefaultSound :80-96) + sound.h:77-88
// (S_TestSoundChar / S_SkipSoundChar).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Owned state
// (`s_knownSfx`/`s_sfxHashList`/`s_sentenceImmediateName` -> Sound-owned
// state), §Extension axes P-3, §Quirks ("VOX single-immediate-slot").
//
// DEVIATIONS FROM LEGACY (documented, not silent — see also sound-boundary.md
// and the S9.6 implementation-agent report):
//  - name->handle lookup uses std::unordered_map + std::vector instead of a
//    fixed MAX_SFX/hashNext chain. Same O(1)-ish behaviour; this port still
//    enforces xash::limits::sound_max_sfx as an explicit capacity and returns
//    k_invalid_sound_handle on overflow (same OBSERVABLE failure class as
//    legacy running out of s_knownSfx slots), without reproducing the
//    specific open-chaining hash structure.
//  - register_sound() NEVER eagerly decodes. Legacy's S_RegisterSound calls
//    S_LoadSound immediately UNLESS a registration sequence is active
//    (`!s_registering`, s_load.c:333); this slice does not model
//    S_BeginRegistration/S_EndRegistration sequences at all (out of scope),
//    so the choice was made to ALWAYS defer decode to first play — strictly
//    lazier than legacy's non-registering-mode behaviour, but matches this
//    slice's explicit obligation ("resolve LAZILY at play time").
//  - S_LoadSound's post-decode Sound_Process resample-to-11k/22k/44k
//    correction (soundlib, s_load.c:131-136) is NOT ported — soundlib scope
//    (S9.2), not yet built.
//  - FilesystemAudioLoader does a SINGLE-path resolution (name as given, or
//    name+".wav" if the extension is unrecognised) instead of soundlib's
//    fuller two-path DEFAULT_SOUNDPATH resolution (snd_main.c FS_LoadSound) —
//    that orchestration is soundlib's job (sibling scope), not built yet.
//  - sfx_t::servercount (the registration-GC key, bumped by every S_FindName
//    call) is not modelled: no S_BeginRegistration/S_EndRegistration
//    equivalent exists this slice.
//
// @thread-safety: SfxRegistry is confined to T_Main (S_RegisterSound/
// S_FindName/S_LoadSound are all T_Main-only call sites in legacy — the
// mix/decoder thread never touches the sfx table). Not asserted here; the
// owning Sound::* entry points assert ThreadRole::Main (sound.cpp).

#include <xash3dpp/abi/sound_api.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/sound/vox.hpp> // IVoxAudioResolver, ImmediateSentenceSlot
#include <xash3dpp/sound/audio_data.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xash::filesystem {
class Filesystem;
}

namespace xash::sound {

// legacy sound_t (s_load.c uses `int` return values; abi::sound_t == int32_t).
using SfxHandle = ::xash::abi::sound_t;

inline constexpr SfxHandle k_invalid_sound_handle = -1;     // S_FindName/S_RegisterSound failure (s_load.c:159,188,317,330)
inline constexpr SfxHandle k_sentence_handle      = -99999; // SENTENCE_INDEX (s_load.c:32)

// ---------------------------------------------------------------------------
// S_TestSoundChar / S_SkipSoundChar (sound.h:77-88) — verbatim port. Checks
// index 0 OR 1 (not just the leading byte); a std::string_view of length 1
// safely treats a would-be index-1 read as "absent" (matches a C string's
// implicit NUL terminator at that position never equalling a real sentinel
// char). Shared by registry.cpp (S_RegisterSound) and channel_alloc.cpp
// (S_StartSound/S_RestoreSound/S_AmbientSound/S_AlterChannel).
// ---------------------------------------------------------------------------
[[nodiscard]] constexpr bool test_sound_char( std::string_view s, char c ) noexcept
{
    if( s.empty() )
        return false;
    if( s[0] == c )
        return true;
    return s.size() > 1 && s[1] == c;
}

[[nodiscard]] constexpr std::string_view skip_sound_char( std::string_view s ) noexcept
{
    return ( !s.empty() && s[0] == '!' ) ? s.substr( 1 ) : s;
}

// ---------------------------------------------------------------------------
// IAudioLoader — the S_LoadSound decode seam (name -> decoded audio).
// nullopt == "could not resolve/decode"; SfxRegistry itself synthesizes the
// S_CreateDefaultSound fallback (see make_default_sound below) so load_sfx()
// always succeeds, matching S_LoadSound's own unconditional guarantee.
// ---------------------------------------------------------------------------
class IAudioLoader
{
public:
    IAudioLoader() noexcept                         = default;
    virtual ~IAudioLoader()                         = default;
    IAudioLoader( const IAudioLoader & )             = delete;
    IAudioLoader &operator=( const IAudioLoader & )  = delete;

    [[nodiscard]] virtual std::optional<AudioData> load( std::string_view name ) noexcept = 0;
};

// S_CreateDefaultSound (s_load.c:80-96): 1 second of silence at the DMA
// rate, mono, 16-bit.
[[nodiscard]] AudioData make_default_sound() noexcept;

// ---------------------------------------------------------------------------
// FilesystemAudioLoader — production IAudioLoader: Filesystem::load_file +
// the linked codec table (codec.hpp). See file-header deviations for the
// simplified (single-path) resolution vs. soundlib's fuller FS_LoadSound.
// ---------------------------------------------------------------------------
class FilesystemAudioLoader final : public IAudioLoader
{
public:
    explicit FilesystemAudioLoader( ::xash::filesystem::Filesystem &fs ) noexcept : fs_( &fs ) {}

    [[nodiscard]] std::optional<AudioData> load( std::string_view name ) noexcept override;

private:
    ::xash::filesystem::Filesystem *fs_; // @lifetime: caller (outlives this loader)
};

// One registered sound (sfx_t equivalent).
struct SfxSlot
{
    std::string               name;  // sfx_t::name (COM_FixSlashes'd, as registered)
    std::optional<AudioData>  cache; // sfx_t::cache — LAZY: nullopt until first load_sfx() call
};

// ---------------------------------------------------------------------------
// SfxRegistry — S_FindName + S_RegisterSound + S_LoadSound + S_FreeSound +
// S_GetSfxByHandle + the '!'-sentence/SENTENCE_INDEX routing
// (ImmediateSentenceSlot, vox.hpp) rolled into one Sound-owned component.
// Implements IVoxAudioResolver so VOX word resolution shares the exact same
// name/decode path S_StartSound uses (VOX_LoadSound calls the SAME
// S_FindName/S_LoadSound, s_vox.c:507,153).
//
// @thread-safety: see file header (T_Main only).
// ---------------------------------------------------------------------------
class SfxRegistry final : public IVoxAudioResolver
{
public:
    // `loader` is borrowed and may be null (a headless/test load — every
    // load_sfx() call then falls straight through to make_default_sound()).
    explicit SfxRegistry( IAudioLoader *loader = nullptr ) noexcept : loader_( loader )
    {
        slots_.reserve( ::xash::limits::sound_max_sfx );
    }
    ~SfxRegistry() override = default;

    SfxRegistry( const SfxRegistry & )            = delete;
    SfxRegistry &operator=( const SfxRegistry & ) = delete;

    // S_RegisterSound (s_load.c:312-336). '!'-prefixed names route to the
    // immediate-sentence slot and return k_sentence_handle WITHOUT touching
    // the sfx table or decoding anything (see file-header deviation on
    // laziness). Leading '/'/'\\' stripped (mapper quirk, s_load.c:326-327).
    [[nodiscard]] SfxHandle register_sound( std::string_view name ) noexcept;

    // S_FindName (s_load.c:149-204), minus VOX routing (register_sound's
    // job). Always creates a slot for a new name (returns k_invalid_sound_handle
    // only on an empty/oversized name or table overflow).
    [[nodiscard]] SfxHandle find_name( std::string_view name ) noexcept;

    // S_GetSfxByHandle (s_load.c:338-351): k_sentence_handle re-derives via
    // the immediate slot; anything else is a direct table index.
    [[nodiscard]] const SfxSlot *get( SfxHandle handle ) noexcept;

    // S_LoadSound (s_load.c:103-141), LAZY: decodes via `loader_` on first
    // call for this slot, caches, and returns the same AudioData on every
    // later call — "resolve LAZILY at play time" (register_sound() never
    // calls this). Always returns non-null for a valid handle (the
    // S_CreateDefaultSound fallback on any load failure, matching legacy's
    // own unconditional guarantee); nullptr only for an invalid/out-of-range
    // handle or k_sentence_handle (sentences have no audio of their own —
    // legacy never calls S_LoadSound directly on SENTENCE_INDEX either, VOX
    // resolves per-word instead).
    [[nodiscard]] const AudioData *load_sfx( SfxHandle handle ) noexcept;

    [[nodiscard]] std::size_t count() const noexcept { return slots_.size(); }

    // ImmediateSentenceSlot access (vox.hpp) — the s_sentenceImmediateName
    // single-slot handoff register_sound() writes into for '!' names.
    [[nodiscard]] const ImmediateSentenceSlot &immediate_slot() const noexcept { return immediate_; }

    // IVoxAudioResolver (vox.hpp) — resolve()/release() share this SAME
    // table+loader (VOX_LoadSound/VOX_FreeWord call the identical
    // S_FindName/S_LoadSound/FS_FreeSound the plain S_StartSound path uses).
    [[nodiscard]] const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept override;
    void release( const AudioData *data ) noexcept override;

private:
    IAudioLoader *loader_; // @lifetime: caller (outlives this registry); nullable

    // @pre-reserved: sound_max_sfx (reserved in the .cpp constructor)
    std::vector<SfxSlot> slots_; // s_knownSfx[MAX_SFX]

    std::unordered_map<std::string, SfxHandle> by_name_; // s_sfxHashList (name -> index)
    ImmediateSentenceSlot                      immediate_; // s_sentenceImmediateName
};

} // namespace xash::sound
