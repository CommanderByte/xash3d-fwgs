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
// @thread-safety: SfxRegistry has NO internal synchronisation. It is OWNED by
// T_Main (S_RegisterSound/S_FindName/S_LoadSound are all T_Main-only call sites
// in legacy), and the one off-Main reader — T_AudioDecoder resolving a VOX word
// at word-advance time — goes through sound.cpp's LockedSfxResolver, which
// serializes every access on Sound::Impl::registry_mutex_. That mutex must NOT
// be held across a decode: use the lookup()/cached()/install() split below
// (CONC-9). The role is not asserted here (the decoder legitimately reaches the
// locked adapter); the owning Sound::* entry points assert ThreadRole::Main
// (sound.cpp).

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
//
// @thread-safety: load() is reachable from BOTH T_Main (SfxRegistry::
// load_sfx() — registration-time decode) AND T_AudioDecoder (sound.cpp's
// LockedSfxResolver, serving a VOX word's lazy resolve — deliberately called
// with Sound::Impl::registry_mutex_ released, gate finding CONC-9). No single
// per-call assert can name both legitimate callers; every implementation
// must therefore be reentrant and must not touch state shared with the
// registry (see registry.cpp's FilesystemAudioLoader::load() for the
// concrete instance of this contract).
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
//
// @thread-safety: see IAudioLoader's @thread-safety above — load() is called
// from both T_Main and T_AudioDecoder and must stay reentrant; this
// implementation touches only its borrowed `fs_` (itself a stateless-per-call
// handle) and returns an owned AudioData, so it satisfies that contract.
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
    std::string name; // sfx_t::name (COM_FixSlashes'd, as registered)

    // sfx_t::cache — LAZY: nullopt until the first load_sfx()/install() call.
    //
    // ADDRESS-STABILITY INVARIANT (the whole S9.7b borrow design rests on it):
    //   an `AudioData` address, once handed out by load_sfx()/resolve()/
    //   install(), stays valid until the SfxRegistry itself is destroyed.
    // Two facts make that true, by construction rather than by timing:
    //   (a) the constructor does slots_.reserve( limits::sound_max_sfx ) and
    //       find_name() REFUSES to grow past that (MAX_SFX, s_load.c:187-188),
    //       so the slot vector never reallocates and `&slots_[i]` — hence
    //       `&*slots_[i].cache` — never moves;
    //   (b) nothing ever destroys a cache entry while the registry lives:
    //       release() is a documented no-op and install() never overwrites an
    //       existing entry (see both below).
    // This is precisely what makes it safe to put a borrowed
    // `const AudioData *` into a queued AudioCommand::source (audio_command.hpp)
    // and into MixChannel::source while T_AudioDecoder owns the channel array:
    // the pointee cannot be freed or relocated out from under either.
    std::optional<AudioData> cache;
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

    // -----------------------------------------------------------------------
    // The load_sfx()/resolve() decode path, SPLIT so a caller holding an
    // EXTERNAL mutex (Sound::Impl::registry_mutex_) never holds it across the
    // loader's blocking file I/O + decode (gate finding CONC-9: T_Main stalling
    // the High-priority decoder on the same mutex shows up as ring underruns).
    //
    // Protocol — lock / unlock / lock:
    //   1. LOCKED:   lookup()/cached() — resolve the handle, observe whether a
    //                cache entry already exists, and take the loader pointer.
    //   2. UNLOCKED: run loader->load(name).  Nothing registry-owned is touched.
    //   3. LOCKED:   install() — a DOUBLE-CHECKED install: if someone else won
    //                the race meanwhile, our decode is discarded and the
    //                winner's pointer is returned.
    // Step 3 cannot violate the address-stability invariant (SfxSlot::cache):
    // the loser's AudioData is destroyed before it was ever handed out, and the
    // winner's entry is never replaced.
    // -----------------------------------------------------------------------

    // The already-decoded audio for `handle`, or nullptr if it has not been
    // decoded yet.  Never decodes.
    [[nodiscard]] const AudioData *cached( SfxHandle handle ) const noexcept;

    // The injected loader (immutable for this registry's lifetime; nullable).
    [[nodiscard]] IAudioLoader *loader() const noexcept { return loader_; }

    // Install a decode result for `handle` and return the STABLE address of
    // whatever this slot ends up holding.  `decoded == nullopt` installs the
    // S_CreateDefaultSound fallback (s_load.c:129), exactly like load_sfx().
    // If the slot was filled while the caller was decoding unlocked, `decoded`
    // is discarded and the incumbent is returned (double-checked install).
    // nullptr only for an invalid/out-of-range handle or k_sentence_handle.
    [[nodiscard]] const AudioData *install( SfxHandle handle, std::optional<AudioData> decoded ) noexcept;

    // Phase-1 result of the split resolve() path above.
    struct ResolveLookup
    {
        SfxHandle        handle   = k_invalid_sound_handle;
        bool             in_cache = false;   // FL_VOXWORD_IN_CACHE (was it ALREADY decoded?)
        const AudioData *cached   = nullptr; // non-null == nothing left to do
        IAudioLoader    *loader   = nullptr; // what phase 2 must call, unlocked
        std::string      name;               // the slot's stored (COM_FixSlashes'd) name
    };

    // resolve()'s phase 1: S_FindName + the in_cache observation, no decode.
    [[nodiscard]] ResolveLookup lookup( std::string_view path ) noexcept;

    [[nodiscard]] std::size_t count() const noexcept { return slots_.size(); }

    // ImmediateSentenceSlot access (vox.hpp) — the s_sentenceImmediateName
    // single-slot handoff register_sound() writes into for '!' names.
    [[nodiscard]] const ImmediateSentenceSlot &immediate_slot() const noexcept { return immediate_; }

    // IVoxAudioResolver (vox.hpp) — resolve()/release() share this SAME
    // table+loader (VOX_LoadSound/VOX_FreeWord call the identical
    // S_FindName/S_LoadSound/FS_FreeSound the plain S_StartSound path uses).
    //
    // resolve() is the ALL-IN-ONE (lookup + decode + install) form, for callers
    // that hold no lock.  A caller that DOES hold one (sound.cpp's
    // LockedSfxResolver) must use the lookup()/install() split above instead so
    // the decode happens unlocked — see the CONC-9 note there.
    [[nodiscard]] const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept override;

    // IVoxAudioResolver::release — a DELIBERATE NO-OP.  See registry.cpp for
    // the full rationale (gate findings F-1/F-2/CONC-1): decoded audio is
    // RETAINED for the registry's lifetime, because its address is borrowed by
    // MixChannel::source, by bound VOX words and by in-flight
    // AudioCommand::source pointers on the MPSC — all of them off T_Main.
    // The seam is kept intact (VoxSystem still calls it) so the call site
    // stays honest about ownership; only the destruction is gone.
    void release( const AudioData *data ) noexcept override;

private:
    IAudioLoader *loader_; // @lifetime: caller (outlives this registry); nullable

    // @pre-reserved: sound_max_sfx (reserved in the .cpp constructor)
    std::vector<SfxSlot> slots_; // s_knownSfx[MAX_SFX]

    std::unordered_map<std::string, SfxHandle> by_name_; // s_sfxHashList (name -> index)
    ImmediateSentenceSlot                      immediate_; // s_sentenceImmediateName
};

} // namespace xash::sound
