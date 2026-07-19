#pragma once
// xash3dpp — LoadGameState `.HL1` per-level loader core (Chunk 8, slice S8.4).
//
// The read-side mirror of S8.3's LevelStateWriter: consumes a complete `.HL1`
// byte image and reconstructs the level state (Save Header, adjacency list,
// lightstyles, the ENTITYTABLE, and a positioned data region), then drives the
// legacy edict-recreation + per-entity restore CONTRACT against an injected
// seam.  Pure state<-bytes; no filesystem I/O (S8.5's concern — that includes
// EntityPatchRead/.HL3 and LoadClientState/.HL2, whose FENTTABLE_REMOVED bit
// this loader's row guard already honors once their setters land), no live server,
// edict arena, or game DLL (those cross the IEntityRestorer seam).
//
// Legacy reference: engine/server/sv_save.c :896-958 (LoadSaveData — fill the
// SAVERESTOREDATA buffer + BuildHashTable), :967-1004 (ParseSaveTables — the
// ETABLE read loop + pBaseData rebase + Save Header / ADJACENCY / LIGHTSTYLE
// reads), :1410-1460 (CreateEntitiesInRestoreList — the id-dispatched edict
// recreation + the physint override hook), :1628-1695 (LoadGameState — the
// slice's core: the per-entity pfnRestore loop with FIELD_TIME rebase via
// pSaveData->time == header.time, the pfnRestore<0 => FL_KILLME + pent=NULL
// disposition, and sv.time = header.time restored AFTER spawn).  Quirks: save-
// boundary.md §Quirks "FIELD_TIME rebasing" + "Edict recreation / entity
// transfer" + the pent bullet; byte layout: deep-dive "`.HL1` top-level byte
// layout".
//
// This is the LoadGameState path (create_world == true, levelMask == 0,
// sv_save.c:1661).  The adjacent-map CreateEntityTransitionList path
// (create_world == false + a levelMask, :1836-1930) is S8.6 — NOT implemented
// here; the driver is structured so that path is a later, additive seam.
//
// @thread-safety: T_Main-only, asserted on the mutating entry points
// (LevelStateLoader::load / restore_entities), matching the S8.1-S8.3 codec
// precedent.  The parse helpers it composes (parse_hl1_preamble, next_field_
// record, read_descriptor_block, EntityTable::deserialize) stay pure.

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_writer.hpp> // Hl1Preamble, parse_hl1_preamble
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/token_table.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/eiface.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xash::save {

// The id-dispatched restoration kind the loader computes for one ENTITYTABLE row
// (sv_save.c:1434-1453); the restorer materializes the edict.  Indices are NOT
// preserved for Named entities (SV_CreateNamedEntity(NULL, …) picks a free slot).
enum class RestoreCreateKind
{
    World,  // id == 0 (create_world): reuse edict 0 — SV_EdictNum(0)+SV_InitEdict (:1434-1438)
    Client, // 1 <= id <= maxclients: reuse the client edict at slot id (:1440-1449)
    Named,  // otherwise: SV_CreateNamedEntity(NULL, classname) — index not preserved (:1451-1453)
};

// Restore driver knobs.  `max_clients` is svs.maxclients — the client-id upper
// bound the id dispatch uses (sv_save.c:1440); server-core state, so injected.
struct RestoreConfig
{
    int max_clients = 1; // svs.maxclients (single-player save is the common case)
};

// The edict-recreation + per-entity restore seam — the read-side mirror of
// S8.3's IEntitySaver.  Server-core owns the edict arena and the game DLL owns
// pfnRestore; both cross this seam so the loader stays a pure save-target
// component (save-boundary.md §Dependencies).  The loader encodes the legacy
// CONTRACT (which kind to create, the FENTTABLE_PLAYER requirement + its
// mismatch handling, the SAVERESTOREDATA positioning, the pfnRestore<0
// disposition); the seam performs the arena/DLL mechanics.
class IEntityRestorer
{
public:
    IEntityRestorer() noexcept = default;
    virtual ~IEntityRestorer() = default;

    IEntityRestorer( const IEntityRestorer & )            = delete;
    IEntityRestorer &operator=( const IEntityRestorer & ) = delete;

    // CreateEntitiesInRestoreList per-row creation (sv_save.c:1434-1457).  The
    // loader has already applied the row guard (`classname && size && !REMOVED`,
    // :1428) and computed `kind` from row.id.  Returns the created edict, or
    // nullptr when creation does not occur — for Client, the restorer applies the
    // legacy `active && SV_IsValidEdict(ed)` gate (:1448) and returns nullptr for
    // an invalid slot; for Named an arena-exhaustion returns nullptr (legacy
    // leaves pent NULL).  `id` is the ENTITYTABLE.id (== the client slot for
    // Client).  `classname` is the row's save-owned classname text (the string
    // pool is server-core; the seam resolves it, sv_save.c:1438/1449/1453).
    [[nodiscard]] virtual ::xash::abi::edict_t *
    create_entity( RestoreCreateKind kind, int id, std::string_view classname ) noexcept = 0;

    // The per-entity restore — the pfnRestore stand-in (sv_save.c:1667-1674).
    // The loader has positioned `entity_data` = the bounded
    // [location, location+size) window into the data region (pSaveData->
    // pCurrentData = pBaseData + pTable->location, :1667), set `table_index` ==
    // pSaveData->currentIndex (:1669) and `time_basis` == header.time (the
    // FIELD_TIME rebase basis, pSaveData->time, :989/:1692 + the Quirks "FIELD_
    // TIME rebasing" rows).  `tokens` resolves field-NAME records (SAVERESTOREDATA
    // .pTokens).  Returns the pfnRestore int: a value < 0 tells the loader to set
    // FL_KILLME on the edict and null the table pointer (:1674-1678); >= 0 is a
    // normal restore.  The engine passes `shouldPrecache = 0` here (:1674) — the
    // full-restore path (contrast the global-merge path's `1`, :1875, S8.6).
    [[nodiscard]] virtual int
    restore_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                    std::span<const std::byte> entity_data, const TokenTable &tokens,
                    float time_basis ) noexcept = 0;

    // Physint override entry (sv_save.c:1417-1419): a game DLL may provide
    // `svgame.physFuncs.pfnCreateEntitiesInRestoreList`, which wholesale-REPLACES
    // the engine's per-row creation loop.  This is the rewrite's projection of
    // that hook: a restorer that owns creation populates every row's `pent` and
    // returns true (the loader then SKIPS its built-in create loop); the default
    // returns false so the loader runs the built-in contract.  DEFERRED: the
    // override BODY is a game-DLL/physint bridge, out of S8.4 scope — only the
    // seam entry exists here.  `create_world` is fixed true / `levelMask` fixed 0
    // for the LoadGameState path, so only `config` (max_clients) crosses.
    [[nodiscard]] virtual bool
    create_entities_override( EntityTable &table, const RestoreConfig &config ) noexcept
    {
        (void)table;
        (void)config;
        return false;
    }
};

// The typed parse result of LevelStateLoader::load — the read-side counterpart
// of the writer's LevelStateParams.  Owns the scalar/vector parse products and
// the rebuilt token table; borrows the populated ENTITYTABLE and the data buffer
// from the loader (they are the loader's SAVERESTOREDATA.pTable / working-buffer
// analogs, reused across load ops — mirroring how the writer borrows both).
// @lifetime: `table` / `buffer` (and any span from data_region()) alias the
// LevelStateLoader's borrowed EntityTable / SaveBuffer and are valid until the
// next load() on that loader (which re-inits the table and re-loads the buffer).
struct LevelState
{
    // --- Parsed SAVE_HEADER (sv_save.c:985) ---
    SaveHeader header{};

    // --- ADJACENCY list (sv_save.c:993-994): header.connection_count entries ---
    std::vector<::xash::abi::LEVELLIST> connections; // @pre-reserved: connection_count (sized once per load(); cold — per load op, not per-frame, Q-13)

    // --- LIGHTSTYLE list (sv_save.c:999-1003): header.light_style_count entries ---
    std::vector<SaveLightStyle> lightstyles; // @pre-reserved: light_style_count (sized once per load(); cold — per load op, not per-frame, Q-13)

    // --- Token table sized to the file's tokenCount (SaveInit, :939) and rebuilt
    //     from the token blob (BuildHashTable, :823-844).  optional because a
    //     TokenTable is non-movable and its slot count is unknown until the
    //     preamble is parsed — it is constructed in place during load(). ---
    std::optional<TokenTable> tokens;

    // --- Borrowed views (see @lifetime above) ---
    const EntityTable *table  = nullptr; // populated ENTITYTABLE (SAVERESTOREDATA.pTable)
    const SaveBuffer  *buffer = nullptr; // holds the size bytes: [ETABLE][data]

    // --- Data-region geometry (the ETABLE/data split is NOT stored on disk) ---
    // table_size: the recomputed ETABLE byte count == the data-region base offset
    //   within `buffer` (the pBaseData rebase past ETABLE, sv_save.c:981).
    // entity_data_offset: offset FROM the data-region base where the per-entity
    //   payloads begin — the end of the Save Header / ADJACENCY / LIGHTSTYLE
    //   blocks; equals the first valid entity's ENTITYTABLE.location.
    std::size_t table_size         = 0;
    std::size_t entity_data_offset = 0;

    // The DATA region (per-entity ENTITYTABLE.location is measured from its base,
    // so location indexes this span directly).  Empty if load() has not run.
    // @lifetime: LevelState.buffer.
    [[nodiscard]] std::span<const std::byte> data_region() const noexcept;
};

// Parses a `.HL1` image into a LevelState and drives the restore contract.  The
// borrowed SaveBuffer is the SAVERESTOREDATA working buffer (the size bytes are
// copied into it); the borrowed EntityTable is (re)sized to tableCount and
// populated as a side effect.  Both are the writer's exact borrows, so a single
// (buf, table) pair round-trips a save then a load.
class LevelStateLoader
{
public:
    // @lifetime: caller — `buf` and `table` are borrowed and must outlive every
    // load()/restore_entities() call and every LevelState that references them.
    LevelStateLoader( SaveBuffer &buf, EntityTable &table ) noexcept
        : buf_( &buf ), table_( &table )
    {
    }

    // LoadSaveData + ParseSaveTables (sv_save.c:896-958, 967-1004): parse `image`
    // into `out` — preamble, token rebuild, ETABLE region (recomputing tableSize
    // by consuming tableCount blocks), Save Header, ADJACENCY x connectionCount,
    // LIGHTSTYLE x lightStyleCount — leaving the data region positioned for
    // per-entity reads.  Does NOT create or restore entities (that is
    // restore_entities).  `out` is fully repopulated.  Errors: any SaveError from
    // parse_hl1_preamble / token rebuild / the block reads, plus CorruptHeader
    // for a negative/over-range header count.  Mutating -> asserts T_Main.
    [[nodiscard]] Result<void> load( std::span<const std::byte> image, LevelState &out ) noexcept;

    // CreateEntitiesInRestoreList + the LoadGameState spawn loop
    // (sv_save.c:1410-1460, 1664-1685) driven against `restorer`, create_world ==
    // true / levelMask == 0 (:1661).  Two phases in legacy order: (1) create every
    // edict (row.pent) — or defer wholesale to the physint override; then (2)
    // position each entity's data window and call pfnRestore.  On pfnRestore < 0
    // the loader sets FL_KILLME on the edict and nulls row.pent (:1676-1677).
    // Returns an error only for a STRUCTURALLY corrupt data region (an ETABLE row
    // whose location/size falls outside the data region — reject-gracefully;
    // legacy computes pBaseData+location with no bounds check); a per-entity kill
    // is a normal outcome, not an error.  NOTE: the caller sets sv.time ==
    // state.header.time AFTER this returns (:1692, restored after SpawnServer) —
    // surfaced via LevelState, applied by the later server-wiring slice.  `state`
    // must be the result of a prior load() on THIS loader.  Mutating -> asserts
    // T_Main.
    [[nodiscard]] Result<void>
    restore_entities( LevelState &state, IEntityRestorer &restorer,
                      const RestoreConfig &config ) noexcept;

private:
    SaveBuffer  *buf_;   // @lifetime: caller
    EntityTable *table_; // @lifetime: caller
};

} // namespace xash::save
