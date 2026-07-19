#pragma once
// xash3dpp — TYPEDESCRIPTION-driven field-block codec (Chunk 8, slice S8.3).
//
// Reproduces the game-DLL field codec (HL-SDK CSave::WriteFields /
// CRestore::ReadFields) for the ENGINE-OWNED header structs the engine hands to
// `pfnSaveWriteFields`: SAVE_HEADER, LEVELLIST (ADJACENCY), SAVE_LIGHTSTYLE.
// In legacy those blocks are serialized by the game DLL exactly like entity
// data; S8.2 already reproduced this codec for the all-int ETABLE block
// (EntityTable::serialize).  This module generalises it to the richer field
// types the header structs use — FIELD_INTEGER / FIELD_FLOAT / FIELD_TIME /
// FIELD_CHARACTER (raw fixed-width arrays) / FIELD_VECTOR / FIELD_EDICT — so the
// on-disk bytes are byte-compatible with a GoldSrc/HL-SDK writer.
//
// Legacy reference: engine/server/sv_save.c :1521 (Save Header via
// pfnSaveWriteFields), :1526 (ADJACENCY), :1538 (LIGHTSTYLE); the per-field
// wire record + DataEmpty skip + block header are proven by SV_GetSaveComment's
// hand-parse (sv_save.c:2390-2436) and the deep-dive "Per-field record
// encoding".  The per-type value encoders mirror HL-SDK saverestore.cpp
// (CSave::WriteInt/WriteFloat/WriteTime/WriteData/WriteVector + the FIELD_EDICT
// entity-index conversion) and the gSizes[] DataEmpty widths.
//
// Codec facts (settled by prior parity gates — save-boundary.md, ledger #16-20):
//   • Field VALUES live inline in the payload; the token table carries only
//     field/block NAMES.  FIELD_CHARACTER writes the FULL fixed array width
//     (strncpy tail included), NOT strlen — contrast FIELD_STRING (strlen+1),
//     which the header structs never use.
//   • An all-zero field is SKIPPED (DataEmpty) and its name token is NOT
//     interned; the block header carries the POST-SKIP actual field count.
//   • DataEmpty tests `fieldSize * gSizes[fieldType]` in-memory bytes — the
//     same byte count the field record's payload occupies (except FIELD_EDICT,
//     whose 8-byte pointer is DataEmpty-tested over its low gSizes==4 bytes and
//     then encoded as a 4-byte entity index).
//
// @thread-safety: write_descriptor_block mutates `sink`/`tokens` -> asserts
// T_Main (matching write_block_header / IFieldSink).  read_descriptor_block is a
// pure overlay parse over a caller span + const TokenTable -> assert-free, so it
// stays usable standalone (SV_GetSaveComment-style), matching
// next_field_record / read_block_header / EntityTable::deserialize.

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/token_table.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/eiface.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xash::save {

// FIELD_EDICT resolver: converts a live edict pointer to the entity index the
// wire format stores (HL-SDK EntityIndex()).  Reverse resolution needs the
// server-core edict arena, which this save-target component does not own
// (save-boundary.md §Dependencies) — the caller injects it.  Only invoked for a
// NON-NULL edict (a NULL pointer is DataEmpty-skipped before the resolver is
// consulted), so the engine-owned blocks that carry no landmark edict
// (the S8.3 test path) never touch it.
using EdictIndexFn = int ( * )( const ::xash::abi::edict_t *edict, void *ctx ) noexcept;

// In-memory element width per FIELDTYPE (HL-SDK gSizes[]) — the DataEmpty span
// and, for the raw-copy types, the payload byte count.  Indexed by FIELDTYPE.
// Widths are the frozen 32-bit-origin sizes (e.g. FIELD_EDICT == sizeof(int),
// NOT sizeof(void*)); this is deliberate parity with the legacy codec.
[[nodiscard]] constexpr int field_element_size( ::xash::abi::FIELDTYPE t ) noexcept
{
    switch ( t )
    {
    case ::xash::abi::FIELD_FLOAT:
    case ::xash::abi::FIELD_STRING:
    case ::xash::abi::FIELD_ENTITY:
    case ::xash::abi::FIELD_CLASSPTR:
    case ::xash::abi::FIELD_EHANDLE:
    case ::xash::abi::FIELD_EVARS:
    case ::xash::abi::FIELD_EDICT:
    case ::xash::abi::FIELD_INTEGER:
    case ::xash::abi::FIELD_BOOLEAN:
    case ::xash::abi::FIELD_TIME:
    case ::xash::abi::FIELD_MODELNAME:
    case ::xash::abi::FIELD_SOUNDNAME:
        return 4;
    case ::xash::abi::FIELD_VECTOR:
    case ::xash::abi::FIELD_POSITION_VECTOR:
        return 12; // 3 * sizeof(float)
    case ::xash::abi::FIELD_SHORT:
        return 2;
    case ::xash::abi::FIELD_CHARACTER:
        return 1;
    case ::xash::abi::FIELD_POINTER:
    case ::xash::abi::FIELD_FUNCTION:
        return static_cast<int>( sizeof( void * ) );
    default:
        return 0;
    }
}

// Serialize `base` (a pointer to the C struct whose layout the descriptors
// describe) as a named field block: write_block_header with the post-skip
// actual field count, then one field record per NON-EMPTY field in descriptor
// order.  `time_basis` is subtracted from every FIELD_TIME element before
// encoding (HL-SDK WriteTime; 0.0f == no rebase, which is what the engine uses
// for the Save Header block, sv_save.c:1520).  `edict_index`/`edict_ctx` resolve
// FIELD_EDICT elements (may be null when no FIELD_EDICT field is populated).
// BadFieldRecord for an unsupported field type or (defensively) a non-null
// FIELD_EDICT with no resolver.
[[nodiscard]] Result<void>
write_descriptor_block( IFieldSink &sink, TokenTable &tokens, std::string_view block_name,
                        const void *base, std::span<const ::xash::abi::TYPEDESCRIPTION> fields,
                        float time_basis = 0.0f, EdictIndexFn edict_index = nullptr,
                        void *edict_ctx = nullptr ) noexcept;

// Inverse of write_descriptor_block: overlay one named block at `data[*offset]`
// onto the pre-initialized struct at `base`, advancing `offset` past the block.
// Field records are matched to descriptors by NAME (case-insensitive), so a
// reordered/foreign writer still parses and an unrecognized field name is
// skipped (forward-compat, matching EntityTable::deserialize).  A field OMITTED
// from the stream (DataEmpty skip) leaves the destination untouched (overlay
// model — the caller MUST pre-init `base`, e.g. value-initialize the struct).
// `time_basis` is ADDED back to every FIELD_TIME element (inverse of write).
// FIELD_EDICT is decoded to its int index but the destination pointer is left
// untouched (arena reconstruction is server-core, deferred — matches how the
// ETABLE reader leaves `pent` null).  CorruptHeader on a block-name mismatch;
// TruncatedBlock / BadFieldRecord on a malformed record or wrong payload width.
[[nodiscard]] Result<void>
read_descriptor_block( std::span<const std::byte> data, std::size_t &offset,
                       const TokenTable &tokens, std::string_view block_name, void *base,
                       std::span<const ::xash::abi::TYPEDESCRIPTION> fields,
                       float time_basis = 0.0f ) noexcept;

} // namespace xash::save
