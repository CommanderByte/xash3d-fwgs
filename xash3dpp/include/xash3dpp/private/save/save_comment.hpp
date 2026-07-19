#pragma once
// xash3dpp — SV_GetSaveComment port + SaveBuildComment port (Chunk 8, slice
// S8.5).
//
// Two independent, standalone (no game-DLL dependency) hand-parses/builders:
//   • save_comment()       — the READ side: hand-decodes a `.sav` image's
//     GameHeader block to produce a UI comment, WITHOUT invoking the game DLL
//     (save-boundary.md "Preserved quirk": the save-list UI must work without
//     loading a level).  Reuses the S8.1/S8.2 parse helpers (read_block_
//     header/next_field_record/TokenTable) — this is exactly the "SV_
//     GetSaveComment-style standalone parse" those helpers were built for
//     (field_sink.hpp/token_table.hpp doc comments).
//   • build_save_comment()  — the WRITE side: the comment TEXT embedded into
//     GAME_HEADER.comment at save time (gTitleComments fallback table).
//
// Legacy reference: engine/server/sv_save.c :2302-2488 (SV_GetSaveComment),
// :229-304 (gTitleComments), :306-357 (SaveBuildComment).  CS_SIZE=64/
// CS_TIME=16/MAX_STRING=256 (common/xash3d_types.h).  Byte layout: deep-dive
// "SV_GetSaveComment hand-parse — output buffer layout".
//
// Deviations from legacy (both adjudicated, save-boundary.md §Uncertainties):
//   • Version-gate outcomes are a typed `SaveCommentStatus` enum, not a
//     hardcoded UI string — "the UI string is the caller's" per the task.
//   • The NULL-deref on a corrupted first field-block-name token (tokenSize
//     == 0 -> legacy's pTokenList is NULL) cannot occur here: TokenTable::
//     rebuild()/token_at() are bounds-safe by construction for an empty or
//     out-of-range token index (token_table.hpp), so a corrupted token index
//     resolves to an empty name (which then fails the "GameHeader" name
//     check gracefully via read_block_header -> CorruptHeader) instead of
//     crashing.  Reject-gracefully achieved by REUSE, not a special case.
//   • The GameHeader block's field-count is read as a full little-endian
//     int32 (via read_block_header), not legacy's accidental single-BYTE
//     read (`nNumberOfFields = (int)*pData` where pData is `char*`) — a
//     reader-defect fix, not a format change; identical result for every
//     field count this codec ever writes (GameHeader has <= 3 fields).
//
// @thread-safety: save_comment / build_save_comment are pure functions over
// caller-supplied data — assert-free by design (matching next_field_record /
// read_descriptor_block), so they remain usable standalone.  The file-backed
// wrapper save_comment_file does filesystem I/O — T_Main-only, asserted
// (save-boundary.md §Threading).

#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/save/errors.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xash::filesystem { class Filesystem; }

namespace xash::save {

// Output-buffer field widths (deep-dive "SV_GetSaveComment hand-parse —
// output buffer layout"; common/xash3d_types.h CS_SIZE=64/CS_TIME=16,
// MAX_STRING=256).  save-comment-local: no other save format uses these.
inline constexpr std::size_t k_comment_field_size = 64;  // CS_SIZE
inline constexpr std::size_t k_comment_time_size  = 16;  // CS_TIME
inline constexpr std::size_t k_comment_max_string = 256; // MAX_STRING

// ---------------------------------------------------------------------------
// save_comment — the read side.
// ---------------------------------------------------------------------------

enum class SaveCommentStatus : std::uint32_t
{
    Ok,                     // comment/date/time fully populated
    NotFound,                // the file does not exist (thin-wrapper only)
    OldVersionUnsupported,  // tag == 0x0065 ("<old version XASH unsupported>")
    OldVersion,             // tag < SAVEGAME_VERSION ("<old version>")
    InvalidVersion,         // tag > SAVEGAME_VERSION ("<invalid version>")
    MissingGameHeader,      // the first block's name != "GameHeader" ("<missing GameHeader>")
    UnknownVersion,         // mapName field never populated ("<unknown version>")
    MapInvalidVersion,      // IMapValidityChecker: MAP_INVALID_VERSION
    MapMissing,             // IMapValidityChecker: !MAP_IS_EXIST
};

// The parsed result.  Mirrors the legacy fixed-offset output buffer (deep-dive
// "output buffer layout": comment[0..64) desc, [64..80) date, [80..96) time,
// [96..160) desc+64) as TYPED FIELDS instead of a packed byte buffer — "the UI
// string is the caller's" (the task's framing); a caller that needs the exact
// legacy layout can concatenate description+date_string+time_string+
// description_ext itself.  Only `status` is populated for the early-return
// status values (OldVersion*/InvalidVersion/MissingGameHeader/NotFound); the
// rest stay default-constructed, matching legacy's early `return 0` before the
// description/timestamp code runs.
struct CommentInfo
{
    SaveCommentStatus status = SaveCommentStatus::Ok;
    std::string       map_name;
    std::string       description;     // comment[0..CS_SIZE) — quick/autosave-prefixed, capped
    std::string       date_string;     // comment[CS_SIZE..CS_SIZE+CS_TIME) — "%b%d %Y" (no space, legacy quirk; day zero-padded 2-digit, e.g. "Jan05")
    std::string       time_string;     // comment[CS_SIZE+CS_TIME..+CS_TIME) — "%H:%M"
    std::string       description_ext; // comment[CS_SIZE+2*CS_TIME..+CS_SIZE) — raw description[64..], capped
};

// The SV_MapIsValid seam (server-core-owned, out of save's dependency scope —
// save-boundary.md §Dependencies).  nullptr (the default) SKIPS the check
// entirely (proceed as if valid), matching the injected-seam nullptr-default
// idiom used throughout S8.3/S8.4 (EdictIndexFn, IEntityRestorer's override).
enum class MapValidity { Ok, InvalidVersion, Missing };

class IMapValidityChecker
{
public:
    IMapValidityChecker() noexcept          = default;
    virtual ~IMapValidityChecker()          = default;
    IMapValidityChecker( const IMapValidityChecker & )            = delete;
    IMapValidityChecker &operator=( const IMapValidityChecker & ) = delete;

    [[nodiscard]] virtual MapValidity check( std::string_view map_name ) noexcept = 0;
};

// The hand-parse core (sv_save.c:2302-2488).  `image` is a `.sav` file's raw
// bytes (NOT filesystem-backed — pure-bytes, memory-only testable).
// `save_name` is the save's file name/path, used ONLY for the quick/autosave
// comment-PREFIX classification (Q_strstr substring match — save-boundary.md
// Quirks: a DIFFERENT predicate than save_directory.hpp's Q_stricmp exact-
// stem rotation check).  `file_time_utc` is the save file's mtime, already
// converted to a UTC time_t by the caller (the PINNED timestamp path:
// filesystem FileTime -> clock_cast<system_clock> -> time_t happens in the
// thin wrapper below; the core takes the already-converted scalar so the
// date/time formatting is fixed-time_t testable).  `map_checker` is the
// optional MapValidity seam.
//
// Errors (structurally corrupt input — reject-gracefully, save-boundary.md):
//   • BadMagic       — the leading id != SAVEGAME_HEADER ("JSAV").
//   • TruncatedBlock — fewer bytes than the preamble/declared regions need.
//   • CorruptHeader  — tokenCount/tokenSize out of [0,budget] ("<corrupted
//     hashtable>" in legacy).
// Everything else legacy treats as a NORMAL (non-fatal) outcome — a version
// mismatch, a missing GameHeader block, or an unresolved map — surfaces as
// Ok(CommentInfo{status=<the specific case>}), never a SaveError.
[[nodiscard]] Result<CommentInfo>
save_comment( std::span<const std::byte> image, std::string_view save_name,
             std::optional<std::time_t> file_time_utc,
             IMapValidityChecker *map_checker = nullptr ) noexcept;

// Thin I/O wrapper: loads `path` via `fs`, converts its FileTime through the
// PINNED path (clock_cast<system_clock> -> time_t), and calls save_comment().
// A missing file returns Ok(CommentInfo{status=NotFound}) (mirrors "just not
// exist - clear comment", sv_save.c:2308-2312 — not a SaveError).
[[nodiscard]] Result<CommentInfo>
save_comment_file( ::xash::filesystem::Filesystem &fs, std::string_view path,
                   IMapValidityChecker *map_checker = nullptr ) noexcept;

// ---------------------------------------------------------------------------
// build_save_comment — the write side (SaveBuildComment, sv_save.c:306-357).
// ---------------------------------------------------------------------------

// gTitleComments (sv_save.c:229-304) — ordering is significant (first
// case-insensitive PREFIX match wins, Q_strnicmp over strlen(mapname)).
struct TitleComment
{
    std::string_view mapname;
    std::string_view titlename;
};

extern const std::array<TitleComment, 66> k_title_comments;

// Builds the world/level description text embedded in a new save's
// GAME_HEADER.comment (SaveBuildComment, sv_save.c:314-357).  Precedence:
// `dll_comment` (the optional DLL-provided `SV_SaveGameComment` export
// output — game-DLL-owned, deferred per SAV-OQ-3/Owned-state; empty ==
// "not present") > the first gTitleComments prefix match on `map_name` >
// `world_message` (svgame.edicts->v.message, string-pool text — server-core
// resolves it, save just receives the text) > `map_name` itself.  Formats
// "%-64.64s %02d:%02d" (left-justified/truncated to 64, then elapsed
// minutes:seconds from `sv_time_seconds` — NOT clock time, matching legacy's
// `sv.time / 60` / `fmod(sv.time, 60)`).
[[nodiscard]] std::string
build_save_comment( std::string_view map_name, std::string_view world_message,
                    std::string_view dll_comment, float sv_time_seconds ) noexcept;

} // namespace xash::save
