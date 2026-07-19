#pragma once
// xash3dpp — `.HL3` entity patch (Chunk 8, slice S8.5).
//
// The simplest sub-format: no header, no magic, no version — a plain int32
// count followed by that many int32 ENTITYTABLE slot indices (NOT edict
// indices) that left the level (FENTTABLE_REMOVED).
//
// Legacy reference: engine/server/sv_save.c :1014-1046 (EntityPatchWrite),
// :1056-1077 (EntityPatchRead).  Loader integration point: S8.4's
// LevelStateLoader row guard already honors FENTTABLE_REMOVED once
// apply_entity_patch() sets it (level_state_loader.hpp Phase 1 row guard:
// "classname && size && !REMOVED").
//
// @thread-safety: apply_entity_patch mutates the borrowed EntityTable ->
// asserts T_Main.  write_entity_patch/read_entity_patch are pure functions
// over caller data -> assert-free.  The file-backed wrappers do filesystem
// I/O, T_Main-only.

#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/save/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }

namespace xash::save {

// EntityPatchWrite (sv_save.c:1014-1046): one int32 per row with
// FENTTABLE_REMOVED set, in table order.  Pure — no filesystem I/O.
[[nodiscard]] std::vector<std::byte> write_entity_patch( const EntityTable &table ) noexcept;

// EntityPatchRead's byte-parse half (sv_save.c:1056-1077): plain
// count+indices, no bounds implied by the format itself.  TruncatedBlock if
// the declared count runs past the image.  Pure — no filesystem I/O, and does
// NOT touch an EntityTable (see apply_entity_patch for that half).
[[nodiscard]] Result<std::vector<std::int32_t>>
read_entity_patch( std::span<const std::byte> image ) noexcept;

// Applies the parsed indices to `table`: `table.row(idx).flags =
// FENTTABLE_REMOVED` — a PLAIN ASSIGNMENT, not SetBits (sv_save.c:1069, the
// exact legacy quirk: this clobbers any other flag bits already set on that
// row).  Deviation from legacy: an out-of-range index is rejected
// (CorruptHeader) rather than legacy's unchecked `pTable[entityId]` write —
// reject-gracefully, matching the SAV-OQ-1/S8.2-S8.4 precedent.
[[nodiscard]] Result<void>
apply_entity_patch( EntityTable &table, std::span<const std::int32_t> indices ) noexcept;

// ---------------------------------------------------------------------------
// File-backed wrappers.  Path: `k_default_save_directory + level + ".HL3"`.
// ---------------------------------------------------------------------------

[[nodiscard]] Result<void>
write_hl3_file( ::xash::filesystem::Filesystem &fs, std::string_view level,
                const EntityTable &table ) noexcept;

// Returns an EMPTY index list (Ok) if the file does not exist — mirrors
// EntityPatchRead's silent `FS_Open == NULL -> return` (sv_save.c:1062-1063,
// the normal "first visit to this level, nothing was ever removed" case).
[[nodiscard]] Result<std::vector<std::int32_t>>
load_hl3_file( ::xash::filesystem::Filesystem &fs, std::string_view level ) noexcept;

} // namespace xash::save
