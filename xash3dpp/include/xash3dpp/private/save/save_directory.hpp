#pragma once
// xash3dpp — save-directory management (Chunk 8, slice S8.5).
//
// The save-slot housekeeping half of sv_save.c: counting/clearing the `*.HL?`
// scratch files, the quicksave/autosave rotation rename sequence, and the
// latest-save lookup used by the death-reload path.  All filesystem access
// routes through the EXISTING xash3dpp filesystem surface (filesystem/
// filesystem.hpp, sibling-owned) — this file only builds paths/patterns and
// drives that surface; the ORDERING logic (select_latest_save) is split out
// as a pure function over caller-supplied (name, time) pairs so it is
// testable without any real filesystem.
//
// Legacy reference: engine/server/sv_save.c :369-389 (DirectoryCount),
// :499-520 (ClearSaveDir), :592-646 (AgeSaveList — the exact rotation
// sequence, screenshot `.bmp` renames included per-pattern even though
// screenshot CONTENT is client-owned), :1721-1725 (SaveGameSlot's quick/
// autosave Q_stricmp exact-stem dispatch), :2241-2301 (SV_CompareFileTime /
// SV_GetLatestSave).
//
// @thread-safety: T_Main-only, asserted on every filesystem-touching entry
// point (save is entirely T_Main; save-boundary.md §Threading).
// select_latest_save is a pure function over caller data -> assert-free.

#include <xash3dpp/private/save/format.hpp> // k_default_save_directory

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }

namespace xash::save {

// ---------------------------------------------------------------------------
// Stem predicates (save-boundary.md Quirks: "Quicksave/autosave rotation
// naming ... is a DIFFERENT predicate than the comment-prefix classification"
// — Q_stricmp EXACT-stem here, vs. save_comment.hpp's Q_strstr substring
// check).  Pure, assert-free.
// ---------------------------------------------------------------------------

[[nodiscard]] bool is_quicksave_stem( std::string_view save_name ) noexcept;
[[nodiscard]] bool is_autosave_stem( std::string_view save_name ) noexcept;

// ---------------------------------------------------------------------------
// DirectoryCount (sv_save.c:369-389) — count files matching `pattern`
// (gamedir-only search, matching the legacy `FS_Search(pPath, true, true)`
// call).  Used by SaveGameSlot to fill GAME_HEADER.mapCount.
// ---------------------------------------------------------------------------
[[nodiscard]] std::size_t
directory_count( ::xash::filesystem::Filesystem &fs, std::string_view pattern ) noexcept;

// ---------------------------------------------------------------------------
// ClearSaveDir (sv_save.c:499-520) — delete every `*.HL?` scratch file in
// `dir` (the single-char wildcard also matches the SAV-OQ-1 `.HLX` reserved
// extension — see the "ClearSaveDir glob covers .HLX" test).
// ---------------------------------------------------------------------------
void clear_save_dir( ::xash::filesystem::Filesystem &fs,
                     std::string_view dir = k_default_save_directory ) noexcept;

// ---------------------------------------------------------------------------
// AgeSaveList (sv_save.c:592-646) — the exact rotation-rename sequence.
// Deletes `<dir><stem><count:02>.sav`/`.bmp` (the oldest slot), then shifts
// every remaining numbered slot up by one, finishing with the unnumbered
// `<dir><stem>.sav`/`.bmp` (if present) becoming `<stem>01`.  After this call
// returns, `<dir><stem>.sav` is free for the caller to write the new save
// into (matches SaveGameSlot's call ordering, sv_save.c:1739-1743).
// `count` is the caller-supplied aged-slot budget (GI->quicksave_aged_count /
// GI->autosave_aged_count in legacy — gameinfo-owned, out of save's scope,
// so it crosses as a plain parameter, P-5).  A no-op for count <= 0.
//
// DEFERRED CALLER OBLIGATION (Observation A, S8.5 parity audit 2026-07-19):
// legacy AgeSaveList also evicts each renamed/deleted `.bmp` thumbnail from
// the renderer's image cache (`GL_FreeImage`, sv_save.c:607,630) before the
// filesystem rename/delete — a CLIENT-side (renderer) capability outside
// this save-target component's scope (save-boundary.md §Dependencies: the
// renderer is sibling-scope, reached only through capability seams like
// client_state.hpp's IDecalListProvider).  This function does NOT perform
// that eviction; the Chunk-12 caller that owns the renderer image cache must
// invoke its own thumbnail-eviction hook (by the same old/new `.bmp` paths
// this function computes) alongside every call here.
// ---------------------------------------------------------------------------
void age_save_list( ::xash::filesystem::Filesystem &fs, std::string_view stem, int count,
                    std::string_view dir = k_default_save_directory ) noexcept;

// ---------------------------------------------------------------------------
// SV_GetLatestSave (sv_save.c:2241-2301) — split into a pure orderer
// (select_latest_save) and a thin filesystem-driven wrapper (latest_save).
// ---------------------------------------------------------------------------

struct SaveFileTimeEntry
{
    std::string_view                                    name{};
    std::optional<std::filesystem::file_time_type>       time{}; // nullopt == FS_FileTime <= 0 ("no match")
};

// Pure ordering (SV_CompareFileTime, sv_save.c:2240-2250): the entry with the
// STRICTLY greatest time wins; entries with no time are skipped; on a tie the
// FIRST entry seen at that time is kept (legacy's `< 0` strict-less compare
// never replaces on equal times).  Returns nullopt if no entry has a time.
[[nodiscard]] std::optional<std::string_view>
select_latest_save( std::span<const SaveFileTimeEntry> entries ) noexcept;

// Thin wrapper: globs `<dir>*.sav`, queries each match's FileTime via the
// filesystem, and applies select_latest_save.  Returns nullopt if the
// directory has no `.sav` files (mirrors SV_GetLatestSave's NULL return).
[[nodiscard]] std::optional<std::string>
latest_save( ::xash::filesystem::Filesystem &fs, std::string_view dir = k_default_save_directory ) noexcept;

} // namespace xash::save
