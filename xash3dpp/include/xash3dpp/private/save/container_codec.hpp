#pragma once
// xash3dpp — `.sav` container codec (Chunk 8, slice S8.5).
//
// The outer container shell: GAME_HEADER block + the pfnSaveGlobalState blob
// seam + token table, followed by the embedded `.HL1`/`.HL2`/`.HL3` (and, in
// the future, `.HLX` — SAV-OQ-1) records copied in verbatim.  Pure state<->
// bytes; no filesystem I/O in the core (write_sav_container / read_sav_
// container) — the file-backed wrappers at the bottom of this header round
// through the existing xash3dpp filesystem surface (filesystem/filesystem.hpp,
// sibling-owned, not extended here).
//
// Legacy reference: engine/server/sv_save.c :369-389 (DirectoryCount),
// :647-670 (DirectoryCopy — write), :679-706 (DirectoryExtract — read,
// extension-blind by design), :1704-1770 (SaveGameSlot — the container
// preamble + GameHeader + pfnSaveGlobalState + DirectoryCopy sequence),
// :1783-1835 (SaveReadHeader — the read-side mirror + pfnRestoreGlobalState).
// Byte layout: deep-dive "Container record format (DirectoryCopy /
// DirectoryExtract)".
//
// Container preamble (5 int32s — NO tableCount; the `.sav` container has no
// ETABLE, unlike `.HL1`): id ('JSAV'), version (SAVEGAME_VERSION 0x0071),
// size (GameHeader block + global-state blob bytes), tokenCount, tokenSize.
// Then: tokens[tokenSize], data[size] (GameHeader block, then whatever
// pfnSaveGlobalState appended), then N embedded records (DirectoryCopy shape:
// name[260] zero-padded + int32 fileSize + fileSize bytes), N ==
// GameHeader.mapCount, extension-blind on read (SAV-OQ-1 "Preserved quirk,
// load-bearing").
//
// @thread-safety: write_sav_container/read_sav_container mutate the borrowed
// SaveBuffer -> assert T_Main (matching the S8.1-S8.4 codec precedent).  The
// file-backed wrappers additionally do filesystem I/O, still T_Main-only
// (save-boundary.md §Threading: save is entirely T_Main).

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>
#include <xash3dpp/save/errors.hpp>

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
// Shared 5-int preamble (id, version, size, tokenCount, tokenSize) — the
// `.sav` container AND `.HL2` (client_state.hpp) both use this exact shape,
// differing only in the expected magic/version pair.
// ---------------------------------------------------------------------------

inline constexpr std::size_t k_five_int_preamble_bytes = 5 * sizeof( std::int32_t );
static_assert( k_five_int_preamble_bytes == 20, "id+version+size+tokenCount+tokenSize" );

struct FiveIntPreamble
{
    std::int32_t size        = 0; // data-region byte count (EXCLUDES tokens)
    std::int32_t token_count = 0;
    std::int32_t token_size  = 0;

    [[nodiscard]] std::size_t token_offset() const noexcept { return k_five_int_preamble_bytes; }
    [[nodiscard]] std::size_t data_offset() const noexcept
    {
        return k_five_int_preamble_bytes + static_cast<std::size_t>( token_size );
    }
};

// Parse + validate the 20-byte preamble at the front of a `.sav`/`.HL2` image.
// `expected_magic`/`expected_version` pin the caller's exact-match gate
// (SAVEGAME_HEADER/SAVEGAME_VERSION for `.sav`; SAVEGAME_HEADER/CLIENT_
// SAVEGAME_VERSION for `.HL2`).  Errors mirror parse_hl1_preamble:
//   • TruncatedBlock  — fewer than 20 bytes, or declared regions don't fit.
//   • BadMagic        — id != expected_magic.
//   • VersionMismatch — version != expected_version.
//   • CorruptHeader   — a negative/over-budget size/tokenCount/tokenSize
//     (the real legacy load path trusts these unchecked; SV_GetSaveComment's
//     own bounds are the rewrite's model — deep-dive "Untrusted-input
//     asymmetry").
[[nodiscard]] Result<FiveIntPreamble>
parse_five_int_preamble( std::span<const std::byte> image, std::int32_t expected_magic,
                         std::int32_t expected_version ) noexcept;

// ---------------------------------------------------------------------------
// pfnSaveGlobalState / pfnRestoreGlobalState seam (game-DLL-owned; save-
// boundary.md §Dependencies).  NULL == empty (no global-state blob written /
// nothing further to read past the GameHeader block) — documented per the
// task's "pfnSaveGlobalState blob seam ... null = empty" instruction.
// ---------------------------------------------------------------------------

class ISaveGlobalState
{
public:
    ISaveGlobalState() noexcept          = default;
    virtual ~ISaveGlobalState()          = default;
    ISaveGlobalState( const ISaveGlobalState & )            = delete;
    ISaveGlobalState &operator=( const ISaveGlobalState & ) = delete;

    // Called immediately after the GameHeader block is written, into the same
    // growing buffer (sv_save.c:1729-1730).  Append zero or more additional
    // named blocks via `sink`/`tokens`.  Mutating -> implementations assert
    // T_Main.
    [[nodiscard]] virtual Result<void>
    save_global_state( IFieldSink &sink, TokenTable &tokens ) noexcept = 0;
};

class IRestoreGlobalState
{
public:
    IRestoreGlobalState() noexcept          = default;
    virtual ~IRestoreGlobalState()          = default;
    IRestoreGlobalState( const IRestoreGlobalState & )            = delete;
    IRestoreGlobalState &operator=( const IRestoreGlobalState & ) = delete;

    // Called immediately after the GameHeader block is parsed (sv_save.c:
    // 1822), continuing the read cursor from where GameHeader left off.
    // `offset` positions into `data`; advance it past whatever was consumed.
    [[nodiscard]] virtual Result<void>
    restore_global_state( std::span<const std::byte> data, std::size_t &offset,
                          const TokenTable &tokens ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// Embedded-record framing (DirectoryCopy / DirectoryExtract).
// ---------------------------------------------------------------------------

// One `.HL?`-glob-matched file to embed (write side).  `name` is the bare
// filename (COM_FileWithoutPath — no directory component); `data` is the
// file's exact bytes.  @lifetime: caller — borrowed for the write_sav_
// container call only, the produced image copies out.
struct EmbeddedFile
{
    std::string_view           name{};
    std::span<const std::byte> data{};
};

// One extracted record (read side).  `name` is the embedded name VERBATIM
// (extension-blind — SAV-OQ-1 "Preserved quirk, load-bearing"); `data` is a
// borrowed view into the source image.  @lifetime: the image span passed to
// read_sav_container.
struct ExtractedRecord
{
    std::string name;
    std::span<const std::byte> data{};
};

// ---------------------------------------------------------------------------
// Container assembly.
// ---------------------------------------------------------------------------

// Root .sav header scalars (GAME_HEADER: mapName/comment/mapCount).
// `map_count` is NOT supplied here — write_sav_container derives it from
// `embedded_files.size()` (DirectoryCount is just `t->numfilenames`,
// sv_save.c:1723 — the write side counts what it is about to embed, it does
// not re-glob the disk; the file-backed wrapper below performs the actual
// `*.HL?` glob via directory_count and feeds the result in).
struct SavContainerParams
{
    std::string_view map_name{}; // sv.name (:1722)
    std::string_view comment{};  // pSaveComment (SaveBuildComment's output)
};

// Assembles a `.sav` byte image: GAME_HEADER block, then `global_state`'s
// blob (nullptr => nothing appended, the "null = empty" seam), token blob,
// then every `embedded_files[i]` as a DirectoryCopy-shaped record (260-byte
// zero-padded name + int32 size + bytes).  `buf`/`table`'s token table is
// reset() first (SaveClear, matching LevelStateWriter::write).  Pure — no
// filesystem I/O.
[[nodiscard]] Result<void>
write_sav_container( const SavContainerParams &params, ISaveGlobalState *global_state,
                     std::span<const EmbeddedFile> embedded_files, SaveBuffer &buf,
                     std::vector<std::byte> &out ) noexcept;

// The read-side result: the parsed GAME_HEADER plus every extracted embedded
// record (extension-blind — SAV-OQ-1).  `records.size()` may differ from
// `header.map_count` only if a record's declared size ran past the image
// (TruncatedBlock is returned in that case; records already parsed before
// the failure are NOT returned — mirrors the "abort the rest" partial-extract
// note in the deep-dive, but the rewrite discards the partial result instead
// of leaving anything on disk half-written).
struct SavContainerResult
{
    GameHeader                   header{};
    std::vector<ExtractedRecord> records; // @pre-reserved: header.map_count (sized once per parse; cold — per load op, not per-frame, Q-13)
};

// Parses a `.sav` image: preamble -> token rebuild -> buf.load_from(data) ->
// GameHeader block (read_descriptor_block) -> `global_state->restore_global_
// state(...)` (skipped entirely if `global_state` is nullptr — mirrors
// SaveReadHeader's unconditional pfnRestoreGlobalState call being contingent
// on this component actually owning a global-state consumer, which it does
// not by default) -> DirectoryExtract-equivalent extraction loop over the
// REMAINDER of `image` (extension-blind; `header.map_count <= 0` extracts
// nothing rather than erroring — legacy's `for(i=0;i<fileCount;i++)` with a
// non-positive fileCount is zero iterations, not a fault).
[[nodiscard]] Result<void>
read_sav_container( std::span<const std::byte> image, IRestoreGlobalState *global_state,
                    SaveBuffer &buf, SavContainerResult &out ) noexcept;

// ---------------------------------------------------------------------------
// SAV-OQ-1 door helper — parses a future `.HLX` side-block's self-describing
// header (format.hpp HlxSideBlockHeader).  Door-keep only: nothing in this
// slice calls this outside its own round-trip test (no producer ships).
// ---------------------------------------------------------------------------
[[nodiscard]] Result<HlxSideBlockHeader> parse_hlx_header( std::span<const std::byte> data ) noexcept;

// ---------------------------------------------------------------------------
// File-backed wrappers (thin I/O; the "Round via filesystem handles" half of
// the deliverable).  Both build the path as `xash::save::k_default_save_
// directory + name + ".sav"` (mirrors sv_save.c:1735/2216).
// ---------------------------------------------------------------------------

[[nodiscard]] Result<void>
write_sav_file( ::xash::filesystem::Filesystem &fs, std::string_view save_name,
                const SavContainerParams &params, ISaveGlobalState *global_state,
                std::span<const EmbeddedFile> embedded_files, SaveBuffer &buf ) noexcept;

// Returns nullopt (Ok) if the file does not exist (mirrors SV_LoadGame's
// silent `FS_FileExists` guard, sv_save.c:2151-2152) rather than a SaveError —
// a missing save is a normal "no such slot" outcome, not corruption.
[[nodiscard]] Result<std::optional<SavContainerResult>>
load_sav_file( ::xash::filesystem::Filesystem &fs, std::string_view save_name,
              IRestoreGlobalState *global_state, SaveBuffer &buf ) noexcept;

} // namespace xash::save
