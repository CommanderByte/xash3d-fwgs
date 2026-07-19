#pragma once
// xash3dpp — save/restore error codes and Result<T> alias (Chunk 8, slice S8.1)
// @thread-safety: pure types (SaveError enum + Result<T> alias) — no shared state
// Legacy reference: engine/server/sv_save.c — the legacy load path is
// crash-on-hostile-input (no bounds checks, sv_save.c:927-939); the rewrite
// rejects gracefully with a typed SaveError per the Chunk 8 "reject-gracefully"
// adjudication (save-boundary.md §Adjudicated deviations, §Uncertainties).
//
// This mirrors the networking/content Result<T> precedent
// (include/xash3dpp/networking/errors.hpp): a subsystem-local error enum plus a
// std::expected alias.  Do NOT call .value() — only .has_value()/operator* (Q-5).

#include <cstdint>
#include <expected>

namespace xash::save {

// ---------------------------------------------------------------------------
// SaveError — typed failure modes for the save/restore codec.
// ---------------------------------------------------------------------------

enum class SaveError : std::uint32_t
{
    VersionMismatch,  // on-disk version tag != frozen SAVEGAME/CLIENT version (sv_save.c:2317-2347)
    BadMagic,         // FOURCC id field is not VALV / JSAV (sv_save.c:2317)
    CorruptHeader,    // header self-declared counts out of legal range (tokenCount/tokenSize/size)
    TokenOverflow,    // token hash table is full — no free slot after a full probe (sv_save.c CSaveRestoreBuffer)
    TruncatedBlock,   // a block/blob ended before the declared/expected byte count was read
    BadFieldRecord,   // a per-field record's declared size is inconsistent with the buffer
    BufferExhausted,  // a write would exceed the working buffer's bounded capacity
    IoError,          // the filesystem I/O wrapper's read/write/rename/remove call failed (Chunk 8, slice S8.5)
    TransitionBroken, // a landmark changelevel could not complete: the back-connection to the
                      // previous map is missing, or an adjacent level's entity table could not be
                      // rewritten (Chunk 8, slice S8.6).  Legacy raises Host_Error for both
                      // (sv_save.c:1999-2001, 2013-2014); the orchestrator (server-core, S8.7)
                      // owns the host-level fatal — save surfaces the typed failure and stops.
};

// ---------------------------------------------------------------------------
// Result<T> — success-or-SaveError alias (std::expected<T, SaveError>).
// ---------------------------------------------------------------------------

template<typename T>
using Result = std::expected<T, SaveError>;

} // namespace xash::save
