#pragma once
// xash3dpp — save/restore field-record framing seam (Chunk 8, slice S8.1).
// The engine-side FRAMING boundary (SAV-OQ-2), NOT the field-value codec: the
// per-field byte encoding (pfnSaveWriteFields/pfnSaveReadFields) stays game-DLL-
// owned (save-boundary.md §Dependencies, SAV-OQ-2).  This seam owns only the
// record boundaries and token-table indirection.
//
// Wire record (proven by SV_GetSaveComment's independent hand-parse,
// sv_save.c:2404-2436; deep-dive "Per-field record encoding"):
//     short fieldSize      (payload byte count)
//     short tokenIndex     (field NAME via the token table)
//     byte  payload[fieldSize]
// all little-endian, no alignment/padding between records.
//
// Two halves:
//   • IFieldSink / SaveBufferSink  — the WRITE side (writer split).  The
//     interface exists so a future debug-dump/snapshot consumer (P-2/P-4) can
//     provide a second IFieldSink without touching the primary save-write path.
//     SaveBufferSink is the only implementation shipped now (door-keep).
//   • next_field_record             — the READ side as a pure parse helper,
//     usable standalone with no game DLL (the SV_GetSaveComment mode).
//
// @thread-safety: IFieldSink/SaveBufferSink are T_Main-only, asserted (they
// mutate a SaveBuffer).  next_field_record is a pure function over a caller span
// — assert-free by design so it stays usable in a no-DLL parse.

#include <xash3dpp/save/errors.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xash::save {

class SaveBuffer; // fwd — SaveBufferSink borrows one

// Bytes on the wire before a field record's payload: short size + short token.
inline constexpr std::size_t k_field_record_header_bytes = 4;

// A parsed field record.  `payload` is a non-owning view into the source buffer.
struct FieldRecord
{
    std::uint16_t token_idx = 0;
    // @lifetime: the source buffer passed to next_field_record.
    std::span<const std::byte> payload{};
};

// Parse ONE field record from `data` starting at `offset`; on success advances
// `offset` past the record and returns it.  Pure (no thread assert):
//   • TruncatedBlock  — fewer than 4 header bytes remain.
//   • BadFieldRecord  — the record's declared size runs past the buffer end.
[[nodiscard]] Result<FieldRecord>
next_field_record( std::span<const std::byte> data, std::size_t &offset ) noexcept;

// ---------------------------------------------------------------------------
// Write side
// ---------------------------------------------------------------------------

// Pure-virtual framing sink.  A single field record is emitted per call; the
// caller has already resolved the field name to a token index.
class IFieldSink
{
public:
    IFieldSink() noexcept          = default;
    virtual ~IFieldSink()          = default;

    IFieldSink( const IFieldSink & )            = delete;
    IFieldSink &operator=( const IFieldSink & ) = delete;

    // Emit `short payload.size(), short token_idx, payload`.  Mutating side
    // effect -> implementations assert T_Main.
    [[nodiscard]] virtual Result<void>
    write_field_record( std::uint16_t token_idx,
                        std::span<const std::byte> payload ) noexcept = 0;
};

// The primary (and only, this slice) sink: frames records straight into a
// SaveBuffer.  A record is written atomically — a record that would not fit is
// rejected with BufferExhausted before any bytes are written.
class SaveBufferSink final : public IFieldSink
{
public:
    explicit SaveBufferSink( SaveBuffer &buf ) noexcept : buf_( &buf ) {}

    [[nodiscard]] Result<void>
    write_field_record( std::uint16_t token_idx,
                        std::span<const std::byte> payload ) noexcept override;

private:
    SaveBuffer *buf_; // @lifetime: caller — borrowed, must outlive this sink.
};

// ---------------------------------------------------------------------------
// Named field-block header (Chunk 8, slice S8.2)
// ---------------------------------------------------------------------------
//
// Every named field-block (`pfnSaveWriteFields`/`pfnSaveReadFields` call —
// "GameHeader", "Save Header", "ETABLE", "ADJACENCY", ...) begins with ONE
// record using the *exact same* short-size/short-token/payload shape as an
// ordinary field record: the token names the BLOCK (not a field), and the
// 4-byte payload is a little-endian int32 field count.  Reconstructed from
// SV_GetSaveComment's independent hand-parse of the .sav root GameHeader
// block, which reads this shape and then jumps straight to the first real
// field record (sv_save.c:2390-2407 header pair + fieldcount-skip target;
// deep-dive "Per-field record encoding").  A block header is therefore just
// write_field_record()/next_field_record() specialised to a 4-byte int32
// payload — no new wire shape, only a naming convention.
//
// `field_count` is the number of field records the caller is ABOUT TO WRITE
// for this block, not necessarily a fixed descriptor size: HL-SDK
// `CSave::WriteFields` DataEmpty semantics omit an all-zero/empty field from
// the stream entirely, so a caller (e.g. EntityTable::serialize) must pass
// the post-skip ACTUAL count it is about to emit, computed before calling
// this function — write_block_header itself has no opinion on why the count
// is what it is; it only frames the header record.
//
// @thread-safety: write_block_header mutates `tokens` and `sink` -> asserts
// T_Main (matching IFieldSink/TokenTable::insert).  read_block_header is a
// pure parse over a caller span and a const TokenTable -> assert-free, so it
// stays usable standalone (SV_GetSaveComment-style), matching
// next_field_record.

[[nodiscard]] Result<void>
write_block_header( IFieldSink &sink, TokenTable &tokens, std::string_view block_name,
                    std::int32_t field_count ) noexcept;

// Parses one block header at `data[*offset]`, advances `offset` past it, and
// validates the block's NAME token resolves (case-insensitively) to
// `expected_block_name`.  Errors:
//   • TruncatedBlock / BadFieldRecord — same as next_field_record.
//   • BadFieldRecord  — payload is not exactly 4 bytes (not an int32).
//   • CorruptHeader   — the block name does not match `expected_block_name`
//     (mirrors SV_GetSaveComment's own "<missing GameHeader>" name check).
[[nodiscard]] Result<std::int32_t>
read_block_header( std::span<const std::byte> data, std::size_t &offset,
                   const TokenTable &tokens, std::string_view expected_block_name ) noexcept;

} // namespace xash::save
