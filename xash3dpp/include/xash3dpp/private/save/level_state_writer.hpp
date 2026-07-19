#pragma once
// xash3dpp — SaveGameState `.HL1` per-level writer core (Chunk 8, slice S8.3).
//
// Assembles a complete `.HL1` level-state byte image (magic/version/counts
// preamble + token blob + ETABLE region + data region) into an owned vector,
// WITHOUT any filesystem I/O — pure state->bytes.  File/container I/O is S8.5's
// concern (save-boundary.md §Dependencies: filesystem is sibling-scope).
//
// Legacy reference: engine/server/sv_save.c :1469-1619 (SaveGameState — this
// slice's core), :854-958 (LoadSaveData / GetClientDataSize — the on-disk
// envelope this writer must produce), :715-733 (SaveInit), :390-405
// (InitEntityTable), :792-814 (StoreHashTable).  Byte layout: deep-dive
// "`.HL1` top-level byte layout (sv_save.c:1592-1603)".
//
// Derived `.HL1` layout (file order; every element cited to sv_save.c):
//   int32 id          = SAVEFILE_HEADER "VALV"                        :1590,1593
//   int32 version     = SAVEGAME_VERSION 0x0071                       :1589,1594
//   int32 size        = ETABLE + data byte count (pSaveData->size)    :1597
//   int32 tableCount  = entity count (pSaveData->tableCount)          :1598
//   int32 tokenCount  = token table slot count                       :1599
//   int32 tokenSize   = flattened token blob byte count              :1600
//   byte  tokens[tokenSize]                                           :1601
//   byte  etable[tableSize]   (ETABLE blocks x tableCount)            :1602
//   byte  data[dataSize]      (Save Header, ADJACENCY x N,            :1603
//                              LIGHTSTYLE x M, then per-entity data)
// The three regions are WRITTEN in the buffer as data -> ETABLE (StoreHashTable
// then flattens the tokens), but the FILE reorders them tokens -> ETABLE -> data
// (sv_save.c:1568-1574, 1601-1603).  `size` is the ETABLE+data total; tableSize
// is NOT stored on disk (recomputed at load by parsing tableCount ETABLE
// blocks).  ETABLE.location for each entity is the byte offset into the DATA
// region (deep-dive), so it is measured from the data-region base (buffer
// offset 0 here).
//
// @thread-safety: T_Main-only, asserted on the mutating entry point
// (LevelStateWriter::write), matching the S8.1/S8.2 codec precedent.
// parse_hl1_preamble is a pure parse over a caller span -> assert-free.

#include <xash3dpp/private/save/descriptor_codec.hpp> // EdictIndexFn
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/eiface.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::save {

// The 6-int32 `.HL1` counts preamble (sv_save.c:1590-1600): id, version, size,
// tableCount, tokenCount, tokenSize — six `FS_Write(&x, sizeof(int))` calls.
inline constexpr std::size_t k_hl1_preamble_bytes = 6 * sizeof( std::int32_t );
static_assert( k_hl1_preamble_bytes == 24, "id+version+size+tableCount+tokenCount+tokenSize" );

// One lightstyle to serialize (sv_save.c:1529-1538): the caller passes only the
// styles it wants written; the writer skips any with an empty `pattern`
// (legacy `if(!sv.lightstyles[i].pattern[0]) continue;`) so `lightStyleCount`
// == the number of non-empty entries, matching the header count (:1513-1517).
struct LightStyleInput
{
    int              index = 0;   // sv.lightstyles[] slot (SAVE_LIGHTSTYLE.index, :1536)
    std::string_view pattern{};   // sv.lightstyles[i].pattern (:1534)
    float            time = 0.0f; // sv.lightstyles[i].time (:1535)
};

// The pfnSave stand-in (sv_save.c:1555) — one call per VALID entity row, in
// table order.  The implementation appends this entity's serialized field
// records to `sink` (interning any field names in `tokens`), exactly as the
// game DLL's DispatchSave does.  `table_index` IS pSaveData->currentIndex
// (sv_save.c:1549) — the engine sets it before the call so DispatchSave can
// index pTable.  The writer records ENTITYTABLE.location before the call and
// computes .size from the buffer growth afterwards (DispatchSave:
// `pTable->size = pSaveData->size - pTable->location`), so the callback owns the
// entity payload bytes and nothing else.  Return a SaveError to abort the save.
//
// DEFERRED (game-DLL bridge slice): the real pfnSave takes a SAVERESTOREDATA*
// whose `.time` is the FIELD_TIME rebase basis (pSaveData->time = header.time,
// sv_save.c:1522) and whose `.currentIndex` is set as above.  This stand-in
// receives table_index directly and a framing sink; wiring the ABI-window
// projection (SaveBuffer::to_abi with currentIndex/time set) into a real
// game-DLL pfnSave call is out of S8.3 scope.  Engine-owned blocks are unaffected
// (the Save Header is the only FIELD_TIME carrier and uses basis 0, :1520).
class IEntitySaver
{
public:
    IEntitySaver() noexcept = default;
    virtual ~IEntitySaver() = default;

    IEntitySaver( const IEntitySaver & )            = delete;
    IEntitySaver &operator=( const IEntitySaver & ) = delete;

    [[nodiscard]] virtual Result<void>
    save_entity( std::size_t table_index, const ::xash::abi::edict_t *edict,
                 IFieldSink &sink, TokenTable &tokens ) noexcept = 0;
};

// Narrow input set (P-5) for one `.HL1` image.  The header scalars mirror the
// SAVE_HEADER fields the engine fills at sv_save.c:1498-1510; `connections` /
// `lightstyles` / `edicts` / `classnames` / `saver` are the variable regions.
// @lifetime: caller — every span/pointer member is BORROWED and must outlive the
// LevelStateWriter::write() call it is passed to; the produced image copies out.
struct LevelStateParams
{
    // --- SAVE_HEADER scalars (sv_save.c:1498-1509) ---
    int              skill_level = 0;   // (int)skill.value (:1498)
    float            time        = 0.0f; // svgame.globals->time (:1501)
    std::string_view map_name{};        // sv.name (:1502)
    std::string_view sky_name{};        // sv_skyname.string (:1503)
    int              sky_color_r = 0, sky_color_g = 0, sky_color_b = 0; // (:1504-1506)
    float            sky_vec_x = 0.0f, sky_vec_y = 0.0f, sky_vec_z = 0.0f; // (:1507-1509)

    // --- Adjacency list (sv_save.c:1525-1526); connectionCount = size() ---
    std::span<const ::xash::abi::LEVELLIST> connections{};

    // --- Lightstyles (sv_save.c:1529-1538); empty-pattern entries skipped ---
    std::span<const LightStyleInput> lightstyles{};

    // --- Entity region (sv_save.c:1492, 1545-1572) ---
    // One edict pointer per ENTITYTABLE row (numEntities); the writer copies
    // each into row.pent BEFORE the pfnSave loop (save-boundary.md pent bullet),
    // then FL_CLIENT tagging + SV_IsValidEdict screening read it.
    std::span<::xash::abi::edict_t *const> edicts{};
    // Per-entity classname TEXT, string-pool-resolved by the caller (server-core
    // owns the string pool — save-boundary.md §Dependencies; S8.2 note).  Length
    // must match `edicts`.  A valid entity's classname is stored on its ETABLE
    // row (DispatchSave sets pTable->classname); an entry may be empty.
    std::span<const std::string_view> classnames{};
    IEntitySaver *saver = nullptr; // pfnSave stand-in (:1555)

    // --- FIELD_EDICT resolver for ADJACENCY.pentLandmark (deferred; see
    //     descriptor_codec.hpp).  NULL in S8.3 (landmark edicts are NULL). ---
    EdictIndexFn edict_index     = nullptr;
    void        *edict_index_ctx = nullptr;
};

// Parsed `.HL1` counts preamble + derived region offsets.  `size` is the ETABLE
// + data total (sv_save.c:1597); the ETABLE/data split is NOT stored (recomputed
// at load), so `data_offset` cannot be derived from the preamble alone — the
// reader must consume `table_count` ETABLE blocks from `etable_offset()` first.
struct Hl1Preamble
{
    std::int32_t size        = 0;
    std::int32_t table_count = 0;
    std::int32_t token_count = 0;
    std::int32_t token_size  = 0;

    [[nodiscard]] std::size_t token_offset() const noexcept { return k_hl1_preamble_bytes; }
    [[nodiscard]] std::size_t etable_offset() const noexcept
    {
        return k_hl1_preamble_bytes + static_cast<std::size_t>( token_size );
    }
};

// Parse + validate the 24-byte counts preamble at the front of a `.HL1` image
// (sv_save.c:917-931 reads it; the rewrite adds SV_GetSaveComment-style bounds
// checks the real load path lacks — deep-dive "Untrusted-input asymmetry",
// save-boundary.md "reject-gracefully").  Errors:
//   • TruncatedBlock  — fewer than 24 bytes (or declared regions run past `image`).
//   • BadMagic        — id != SAVEFILE_HEADER ("VALV").
//   • VersionMismatch — version != SAVEGAME_VERSION (0x0071).
//   • CorruptHeader   — a negative/over-budget size/tableCount/tokenCount/tokenSize.
[[nodiscard]] Result<Hl1Preamble>
parse_hl1_preamble( std::span<const std::byte> image ) noexcept;

// Assembles the `.HL1` byte image from `params` into `out` (cleared first).  The
// borrowed SaveBuffer is the SAVERESTOREDATA working buffer (its token table is
// shared across every block); it is reset() on entry so the writer is reusable.
// The borrowed EntityTable is (re)sized to params.edicts.size() and populated
// (pent/location/size/flags + classname text) as a side effect — it holds the
// ETABLE rows this image encodes.
class LevelStateWriter
{
public:
    // @lifetime: caller — `buf` and `table` are borrowed and must outlive every
    // write() call; the produced image (in `out`) is independent of both.
    LevelStateWriter( SaveBuffer &buf, EntityTable &table ) noexcept
        : buf_( &buf ), table_( &table )
    {
    }

    [[nodiscard]] Result<void>
    write( const LevelStateParams &params, std::vector<std::byte> &out ) noexcept;

private:
    SaveBuffer  *buf_;   // @lifetime: caller
    EntityTable *table_; // @lifetime: caller
};

} // namespace xash::save
