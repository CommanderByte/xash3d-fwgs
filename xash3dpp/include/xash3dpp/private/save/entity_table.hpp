#pragma once
// xash3dpp — ENTITYTABLE build/read layer (Chunk 8, slice S8.2).
//
// Legacy reference: engine/server/sv_save.c :390-405 (InitEntityTable),
// :434-443 (EdictFromTable — the clamp quirk), :130-137 (gEntityTable
// descriptor), :967-979 (ParseSaveTables — read-side per-row ETABLE-block
// shape), :1492,1541-1559 (SaveGameState — write-side per-row usage),
// eiface.h:306-316 (ENTITYTABLE). Byte layout: deep-dive
// "ENTITYTABLE (ETABLE) serialization" + "Per-field record encoding".
//
// FIELD_STRING classname note: `ENTITYTABLE::classname` is a raw `string_t`
// (int) handle into the engine's global string pool — a resource this
// save-target component does not own (save-boundary.md §Dependencies: the
// string pool is server-core, `SV_GetString`/`SV_MakeString`/`SV_AllocString`).
// Wiring `classname` to a live string pool is server-core orchestration
// deferred to a later slice (S8.3 is the writer; the task explicitly
// excludes it here). This slice therefore owns the classname TEXT directly
// (`set_classname`/`classname()`, parallel to `row()`) so serialize()/
// deserialize() are fully testable standalone, matching the SAV-OQ-2
// "load path as pure parse helpers" shape. `row(i).classname` (the raw
// string_t) is left untouched by serialize()/deserialize() and is always
// 0 after init()/deserialize() — a later slice's string-pool bridge maps
// classname_text(i) into a live string_t before/after these calls.
//
// CORRECTED 2026-07-19 (S8.2 parity audit — was UNCOMMITTED, fixed pre-commit):
// classname's on-wire VALUE payload is the raw null-terminated TEXT bytes
// (`size = strlen + 1`), NOT a 2-byte token-table index — the token table
// carries only field/block NAME records (SV_GetSaveComment's hand-parse,
// sv_save.c:2390-2436, proves values live inline in the payload). Per the
// HL-SDK `CSave::WriteFields` DataEmpty convention, a field whose in-memory
// bytes are all zero is OMITTED from the stream entirely (int32 fields:
// zero-test over 4 bytes; classname: omitted when its text is empty/unset)
// — the per-row block header's field count is therefore the number of
// fields ACTUALLY emitted after this skip, not the fixed descriptor size
// (`k_entity_table_desc.size()` == 5). See serialize()/deserialize() below
// for the exact per-row shape.
//
// Builds on S8.1: TokenTable / IFieldSink / write_block_header /
// next_field_record (private/save/{token_table,field_sink,save_buffer}.hpp).
//
// @thread-safety: T_Main-only, asserted on every mutating entry point
// (init/set_classname/serialize/deserialize), matching save-boundary.md
// §Threading and the S8.1 TokenTable/SaveBuffer precedent. Pure accessors
// (row() const, classname(), edict_from(), count()) are assert-free by
// design, matching TokenTable::find()/token_at().

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/token_table.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/eiface.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::save {

class EntityTable
{
public:
    EntityTable() noexcept = default;

    // Not pool-owned and no self-referencing storage (two plain vectors) ->
    // compiler-generated copy/move are correct as-is (QJ "value aggregate"),
    // left undeclared here.

    // InitEntityTable (sv_save.c:390-405): (re)size to `count` rows. Legacy
    // Mem_Calloc zero-fills the whole array, then sets `id = i` and
    // `pent = SV_EdictNum(i)` per row — everything else (location/size/flags/
    // classname) stays at its Mem_Calloc zero. This component has no edict-
    // arena dependency (save-boundary.md §Dependencies: the arena is
    // server-core), so `pent` is left NULL here rather than resolved; a
    // later caller with arena access sets `row(i).pent` explicitly. `id`,
    // location/size/flags (0), and classname (cleared) match legacy exactly.
    // Mutating -> asserts T_Main.
    void init( std::size_t count ) noexcept;

    [[nodiscard]] std::size_t count() const noexcept { return rows_.size(); }

    // Direct row access — mirrors `&pSaveData->pTable[i]`. The non-const
    // overload is the escape hatch a later slice (S8.3) uses to set `pent`/
    // location/size/flags from the live edict arena / write cursor; it is an
    // accessor (vends a reference), not a mutation itself, so it is
    // assert-free, matching SaveBuffer::tokens()'s non-const accessor.
    [[nodiscard]] ::xash::abi::ENTITYTABLE       &row( std::size_t index ) noexcept;
    [[nodiscard]] const ::xash::abi::ENTITYTABLE &row( std::size_t index ) const noexcept;

    // Save-owned classname TEXT for `index` — see class doc "FIELD_STRING
    // classname note". Independent of the row's raw `string_t classname`
    // handle. Mutating -> asserts T_Main.
    void set_classname( std::size_t index, std::string_view text ) noexcept;
    [[nodiscard]] std::string_view classname( std::size_t index ) const noexcept;

    // EdictFromTable (sv_save.c:434-443): `entityIndex = bound(0, entityIndex,
    // tableCount - 1)` — a CLAMP (public/xash3d_mathlib.h:141 `bound()`,
    // `(num >= min) ? (num < max ? num : max) : min`), NOT a modulo: an
    // out-of-range index saturates to the nearest valid row (0 for negative,
    // count()-1 for too-large), it does not wrap. Deviation: when
    // tableCount == 0, legacy's `bound(0, i, -1)` evaluates to -1 and
    // `pTable[-1]` is an out-of-bounds read (UB) — this rewrite returns
    // nullptr instead (errors.hpp "reject-gracefully" policy; same
    // disposition as the SV_SpawnServer-leak / GetSaveComment-NULL-deref
    // adjudications in save-boundary.md). Pure read -> assert-free.
    [[nodiscard]] ::xash::abi::edict_t *edict_from( int index ) const noexcept;

    // Per-row ETABLE wire block: write_block_header(tokens,"ETABLE",actualCount)
    // then one field record per NON-EMPTY field, in gEntityTable order
    // (id/location/size/flags as int32; classname as FIELD_STRING). HL-SDK
    // `CSave::WriteFields` DataEmpty semantics: a field whose in-memory bytes
    // are ALL ZERO is omitted from the stream entirely (int32 fields: zero
    // test over the 4 bytes; classname: omitted when its text is empty/
    // unset) — `actualCount` is the number of fields actually emitted AFTER
    // this skip, not the fixed descriptor size (`k_entity_table_desc.size()`
    // == 5). A field's NAME token is interned only at the moment it is
    // written, so a skipped field's name is never inserted into `tokens` for
    // that row. classname's VALUE payload is the raw bytes of the text
    // INCLUDING the trailing '\0' (`size = strlen + 1`) — the token table
    // carries only field/block NAME records, never FIELD_STRING values (see
    // class doc "CORRECTED 2026-07-19"). One block per row, `count()` rows
    // written in order. Mutating (tokens/sink) -> asserts T_Main.
    [[nodiscard]] Result<void> serialize( IFieldSink &sink, TokenTable &tokens ) const noexcept;

    // Inverse of serialize(): reads exactly `count()` ETABLE blocks from
    // `data` starting at `*offset` (the table must already be sized via
    // init() to the expected row count — mirrors ParseSaveTables calling
    // InitEntityTable before its read loop), advancing `offset` past the
    // consumed bytes. Field records are matched by NAME (case-insensitive),
    // not position, so a foreign/reordered writer's blocks still parse — an
    // unrecognized field name inside a block is skipped (forward-compat).
    // A field OMITTED from the stream (the DataEmpty skip, see serialize())
    // leaves the destination row's existing value untouched rather than
    // resetting it to zero — this is why the caller MUST init() the table
    // (setting `id = row index` per row) before calling deserialize(): a
    // row whose true `id` is 0 (e.g. row 0, where the init() default already
    // is 0) round-trips correctly only because deserialize() overlays onto
    // the pre-initialized row instead of starting from a fresh zero value.
    // classname's payload must end in '\0' (BadFieldRecord otherwise); a
    // payload without a trailing NUL is rejected rather than silently
    // truncated. `pent` is never populated from disk (matches legacy: pent
    // is excluded from the wire format and reconstructed later from the
    // live edict arena). Mutating (rows_/classnames_) -> asserts T_Main,
    // matching TokenTable::rebuild()'s "parse a wire blob into owned state"
    // precedent (SAV-OQ-2's "no DLL dependency" is a property of this call
    // chain regardless — it never touches the game DLL).
    [[nodiscard]] Result<void>
    deserialize( std::span<const std::byte> data, std::size_t &offset, const TokenTable &tokens ) noexcept;

private:
    // @pre-reserved: count (sized once in init(); cold — per save/load op,
    // not per-frame, Q-13; mirrors TokenTable's own slots_/abi_ptrs_ note).
    std::vector<::xash::abi::ENTITYTABLE> rows_;
    // @lifetime: EntityTable — classname text, parallel to rows_ (see class
    // doc "FIELD_STRING classname note").
    std::vector<std::string>              classnames_;
};

} // namespace xash::save
