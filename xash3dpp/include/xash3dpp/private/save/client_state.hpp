#pragma once
// xash3dpp — `.HL2` client-state block (Chunk 8, slice S8.5).
//
// SAVE_CLIENT header + decal/static-entity/sound entries.  Server-core owns
// the actual decal list / dynamic-sound snapshot / music-track state (all
// renderer- or sound-engine-sourced, sibling-scope) — this component reaches
// them ONLY through the three OQ-4 capability interfaces below, matching
// save-boundary.md's "the OQ-4 injected capability interfaces" resolution.
// nullptr on a dedicated server (no renderer/sound engine) MUST still emit a
// legacy-loadable block — see "Empty .HL2 derivation" below and the pinned
// golden test.
//
// Legacy reference: engine/server/sv_save.c :1187-1265 (SaveClientState),
// :1290-1409 (LoadClientState), :139-227 (gSaveClient/gDecalEntry/
// gStaticEntry/gSoundEntry descriptor tables — format.hpp).  Static entities
// are NOT capability-gated (sv.num_static_entities/svs.static_entities are
// plain server-core state, always available) — the caller passes them as a
// span, mirroring LevelStateWriter's `edicts` parameter.
//
// gStaticEntry's `messagenum` field (FIELD_MODELNAME — "HACKHACK: model
// stored in messagenum", format.hpp k_static_entry_desc) is a companion-TEXT
// field, not a raw copy of `entity_state_t::messagenum` — CORRECTED
// 2026-07-19 (S8.5 parity audit): the wire payload is the resolved model-name
// TEXT (descriptor_codec.hpp FieldTextBinding), mirroring S8.2's ETABLE
// classname pattern.  `StaticEntityEntry` below carries that text alongside
// the vendored (ABI-frozen, cannot gain a member) `entity_state_t`.
//
// Empty .HL2 derivation (null capabilities, no static entities, viewentity 0,
// wateralpha/wateramp 0): every SAVE_CLIENT field is DataEmpty (all-zero) ->
// the ClientHeader block is written with actualCount == 0 -> the ENTIRE body
// past the 20-byte preamble + token blob is just the block-header record (4
// bytes: int32 field-count 0) — no DECALLIST/STATICENTITY/SOUNDLIST records
// follow, because decalCount/entityCount/soundCount are all 0.  Pinned in
// test_client_state_empty_golden.
//
// @thread-safety: write_client_state/read_client_state mutate the borrowed
// SaveBuffer -> assert T_Main (S8.1-S8.4 codec precedent).  The file-backed
// wrappers additionally do filesystem I/O, still T_Main-only.

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/entity_state.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }

namespace xash::save {

// ---------------------------------------------------------------------------
// OQ-4 capability interfaces (save-boundary.md Extension axes; null on
// dedicated).  Each returns a snapshot the writer copies out immediately —
// no ownership crosses the seam.
// ---------------------------------------------------------------------------

class IDecalListProvider
{
public:
    IDecalListProvider() noexcept          = default;
    virtual ~IDecalListProvider()          = default;
    IDecalListProvider( const IDecalListProvider & )            = delete;
    IDecalListProvider &operator=( const IDecalListProvider & ) = delete;

    // R_CreateDecalList (sv_save.c:1207-1210) — ref.dllFuncs, client/renderer-
    // owned; NOT available on a dedicated server (the `#if !XASH_DEDICATED`
    // guard).  @lifetime: valid for the duration of the write_client_state
    // call only.
    [[nodiscard]] virtual std::span<const SaveDecalEntry> decals() noexcept = 0;
};

class IDynamicSoundsProvider
{
public:
    IDynamicSoundsProvider() noexcept          = default;
    virtual ~IDynamicSoundsProvider()          = default;
    IDynamicSoundsProvider( const IDynamicSoundsProvider & )            = delete;
    IDynamicSoundsProvider &operator=( const IDynamicSoundsProvider & ) = delete;

    // S_GetCurrentDynamicSounds (sv_save.c:1213-1214) — sound-engine-owned;
    // legacy skips this ENTIRELY on a landmark changelevel ("sounds won't
    // going across transition") — see ClientStateParams::include_transient_audio.
    [[nodiscard]] virtual std::span<const SaveSoundEntry> dynamic_sounds() noexcept = 0;
};

struct MusicState
{
    std::string intro_track;
    std::string main_track;
    int         track_position = 0;
};

class IMusicStateProvider
{
public:
    IMusicStateProvider() noexcept          = default;
    virtual ~IMusicStateProvider()          = default;
    IMusicStateProvider( const IMusicStateProvider & )            = delete;
    IMusicStateProvider &operator=( const IMusicStateProvider & ) = delete;

    // S_StreamGetCurrentState (sv_save.c:1216-1219) — sound-engine-owned;
    // also skipped on a landmark changelevel (music free-runs across levels,
    // legacy comment: "it's just continue playing on a next level").
    [[nodiscard]] virtual MusicState music_state() noexcept = 0;
};

// ---------------------------------------------------------------------------
// STATICENTITY entry — entity_state_t + its wire-format companion TEXT.
// ---------------------------------------------------------------------------
//
// `state.messagenum` is left untouched by write_client_state/read_client_state
// (it stays whatever the caller set it to / whatever it was before the read) —
// the WIRE value for gStaticEntry's "messagenum" field is `model_name`, per
// the FieldTextBinding companion mechanism (descriptor_codec.hpp).  An empty
// `model_name` is DataEmpty (the field is skipped on write, exactly like a
// zero raw field); on read a missing field leaves `model_name` untouched
// (default-constructed empty, for a freshly value-initialized entry).
struct StaticEntityEntry
{
    ::xash::abi::entity_state_t state{};
    std::string                 model_name;
};

// ---------------------------------------------------------------------------
// Write side.
// ---------------------------------------------------------------------------

struct ClientStateParams
{
    // --- SAVE_CLIENT scalars not covered by a capability (sv_save.c:1223-1230) ---
    short view_entity  = 0;    // NUM_FOR_EDICT(cl->pViewEntity) — edict arena is server-core, pre-resolved
    float wateralpha   = 0.0f; // sv_wateralpha.value
    float wateramp     = 0.0f; // sv_wateramp.value

    // --- Static entities (NOT capability-gated — plain server-core state) ---
    std::span<const StaticEntityEntry> static_entities{};

    // --- `!changelevel` gate (sv_save.c:1200-1219): sounds/music are skipped
    //     entirely during a landmark transition.  true == full save (the
    //     common case); false == changelevel snapshot. ---
    bool include_transient_audio = true;

    // --- Capabilities (nullptr == absent, e.g. dedicated server) ---
    IDecalListProvider      *decal_provider = nullptr;
    IDynamicSoundsProvider  *sound_provider = nullptr;
    IMusicStateProvider     *music_provider = nullptr;
};

// Assembles a `.HL2` byte image.  `buf`'s token table is reset() first
// (SaveClear, matching LevelStateWriter::write).  Pure — no filesystem I/O.
[[nodiscard]] Result<void>
write_client_state( const ClientStateParams &params, SaveBuffer &buf,
                    std::vector<std::byte> &out ) noexcept;

// ---------------------------------------------------------------------------
// Read side.
// ---------------------------------------------------------------------------

struct LoadedClientState
{
    SaveClient                       header{};
    std::vector<SaveDecalEntry>      decals;
    std::vector<StaticEntityEntry>   static_entities;
    std::vector<SaveSoundEntry>      sounds;
};

// Parses a `.HL2` image: preamble (id=='JSAV', version==CLIENT_SAVEGAME_
// VERSION 0x0067) -> token rebuild -> buf.load_from(data) -> ClientHeader
// block -> header.decal_count/entity_count/sound_count-driven read loops
// into typed lists.  Pure — no filesystem I/O; RestoreDecal/RestoreSound
// (applying a decal/sound to the live world) are server-core integration
// points, deliberately NOT implemented here (matches level_state_loader.hpp's
// IEntityRestorer split — a future slice drives these typed lists into the
// live signon buffer).
[[nodiscard]] Result<void>
read_client_state( std::span<const std::byte> image, SaveBuffer &buf, LoadedClientState &out ) noexcept;

// ---------------------------------------------------------------------------
// File-backed wrappers.  Path: `k_default_save_directory + level + ".HL2"`.
// ---------------------------------------------------------------------------

[[nodiscard]] Result<void>
write_hl2_file( ::xash::filesystem::Filesystem &fs, std::string_view level,
                const ClientStateParams &params, SaveBuffer &buf ) noexcept;

// Returns nullopt (Ok) if the file does not exist (LoadClientState's silent
// `FS_Open == NULL` guard, sv_save.c:1301-1302 — "something bad is happens"
// is the legacy comment, but the behaviour is a quiet no-op, not an error).
[[nodiscard]] Result<std::optional<LoadedClientState>>
load_hl2_file( ::xash::filesystem::Filesystem &fs, std::string_view level, SaveBuffer &buf ) noexcept;

} // namespace xash::save
