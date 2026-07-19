#pragma once
// xash3dpp — landmark-transition machinery (Chunk 8, slice S8.6).
//
// The entity-transfer half of a smooth landmark changelevel: after the NEW map
// is spawned, entities that were in the PVS of the crossed landmark are carried
// over from every ADJACENT saved level (including the level we came FROM, which
// brings the player).  This slice ports:
//
//   • EntityInSolid              (sv_save.c:476-489) — the post-transfer
//     stuck-outside-world pruning test (solid-center world-contents probe).
//   • CreateEntityTransitionList (sv_save.c:1836-1922) — the create_world==false
//     edict-recreation + the two per-entity restore branches (global-entity
//     merge, and the moveable/landmark transfer) driven by a levelMask.
//   • LoadAdjacentEnts           (sv_save.c:1931-2015) — the per-connection loop:
//     load each adjacent `.HL1`, apply its `.HL3` patch, compute the landmark
//     offset + levelMask, run CreateEntityTransitionList, rewrite the `.HL3`
//     patch when entities moved, and the foundprevious hard-fail contract.
//   • The FDECAL_USE_LANDMARK decal-position rebase math (sv_save.c:1239-1240
//     write, :1349-1350 read) as a pure hook — LIVE decal application stays
//     deferred (RestoreDecal / LoadClientState are Chunk-12 wiring).
//
// This is the create_world==false companion to S8.4's LevelStateLoader (which
// owns the LoadGameState create_world==true path).  Server WIRING — the real
// ITransitionRestorer / ISolidTestProvider bodies (edict arena, pfnRestore,
// SV_FindGlobalEntity, SV_PointContents), the pfnParmsChangeLevel level-list
// injection, and the `.HL1`/`.HL3` file I/O — is S8.7; this slice builds the
// exact save-side CONTRACT against seams.
//
// Legacy reference is cited per-declaration below; ordering hazards:
// save-boundary.md §Quirks (FIELD_TIME adjacent-transfer row `pSaveData->time =
// sv.time`, the hard-fail missing-back-connection row, the post-transfer pruning
// row, the global-entity merge row) + deep-dive §7.
//
// @thread-safety: T_Main-only, asserted on the mutating entry points
// (AdjacentTransfer::run, create_entity_transition_list) — save is entirely
// T_Main (save-boundary.md §Threading).  The pure math helpers (landmark_origin,
// compute_landmark_offset, compute_transition_mask, entity_in_solid, the decal
// rebases) are assert-free by design, matching the S8.1-S8.5 parse-helper
// precedent.

#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/level_state_loader.hpp> // RestoreCreateKind, RestoreConfig, LevelState, LevelStateLoader
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/save/errors.hpp>

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/abi/eiface.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xash::save {

// ---------------------------------------------------------------------------
// SaveVec3 — value-semantic 3-vector for the pure landmark/decal math.
// ---------------------------------------------------------------------------
// The ABI's vec3_t is `float[3]` (abi_types.hpp), which is neither returnable
// nor assignable by value; the transition math is small, hot-path-free (per
// changelevel, not per-frame — Q-13), and far cleaner to unit-test as a value
// type.  Converts from/to vec3_t only at the ABI boundary (LEVELLIST.vec-
// LandmarkOrigin, SaveDecalEntry.position).
struct SaveVec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] bool operator==( const SaveVec3 & ) const noexcept = default;
};

// legacy: common/protocol.h:155 — a decal on a bmodel WITHOUT an origin-brush,
// stored in absolute world space, so its position must be landmark-rebased on a
// transition (contrast an origin-brush decal, stored relative to its entity).
inline constexpr int k_fdecal_use_landmark = 0x02;

// ---------------------------------------------------------------------------
// Landmark offset math (LandmarkOrigin, sv_save.c:452-466).
// ---------------------------------------------------------------------------

// LandmarkOrigin (sv_save.c:452-466): the vecLandmarkOrigin of the FIRST
// connection whose landmarkName matches `landmark_name`, or {0,0,0} if none.
// Legacy uses Q_strcmp — a CASE-SENSITIVE match on the landmark name (contrast
// the CASE-INSENSITIVE Q_stricmp used for MAP names everywhere else in the
// transition, preserved in compute_transition_mask below).
[[nodiscard]] SaveVec3
landmark_origin( std::span<const ::xash::abi::LEVELLIST> connections,
                 std::string_view landmark_name ) noexcept;

// The vecLandmarkOffset a game DLL adds to FIELD_POSITION_VECTOR fields during
// an adjacent-level pfnRestore (sv_save.c:1976-1978):
//   offset = landmark_origin(new_level) - landmark_origin(adjacent_level)
// i.e. how far the shared landmark moved between the two maps' coordinate
// frames.  Applied via pSaveData->vecLandmarkOffset inside the game-DLL restore.
[[nodiscard]] SaveVec3
compute_landmark_offset( std::span<const ::xash::abi::LEVELLIST> new_level_connections,
                         std::span<const ::xash::abi::LEVELLIST> adjacent_connections,
                         std::string_view landmark_name ) noexcept;

// ---------------------------------------------------------------------------
// levelMask construction (LoadAdjacentEnts, sv_save.c:1972-1988).
// ---------------------------------------------------------------------------

// Build the levelMask for ONE adjacent level (the `flags` local in
// LoadAdjacentEnts):
//   • FENTTABLE_PLAYER (bit 31) iff this adjacent level IS the level we came
//     from (`is_old_level`) — brings the player across (sv_save.c:1980-1981).
//   • BIT(idx) (bits 0..15) for every connection slot `idx` in the ADJACENT
//     level's own level list whose mapName == `new_map_name` (sv.name), the
//     EntryInTable while-loop (sv_save.c:1983-1988) — an entity carries over iff
//     it was in the PVS of a landmark that leads to the just-spawned map.
// Connection indices are bounded by k_max_level_connections (16), so BIT(idx)
// never collides with the semantic flags (FENTTABLE_GLOBAL bit 28 /
// FENTTABLE_MOVEABLE bit 29 / FENTTABLE_REMOVED bit 30 / FENTTABLE_PLAYER
// bit 31); a slot at/above bit 31 is defensively dropped (cannot occur for a
// well-formed, loader-bounded connection list).
[[nodiscard]] int
compute_transition_mask( std::span<const ::xash::abi::LEVELLIST> adjacent_connections,
                         std::string_view new_map_name, bool is_old_level ) noexcept;

// ---------------------------------------------------------------------------
// Decal landmark-offset hook (RestoreDecal / SaveClientState / LoadClientState).
// LIVE decal application is deferred (client_state.cpp already flags the
// FDECAL_USE_LANDMARK rebase as this slice's concern); these are the pure math
// halves the S8.7 / Chunk-12 decal wiring calls.
// ---------------------------------------------------------------------------

// Write side (SaveClientState, sv_save.c:1239-1240): a saved decal's absolute
// position has the landmark offset SUBTRACTED, but ONLY when the save is a
// landmark transition (`use_landmark`) AND the decal is FDECAL_USE_LANDMARK.
// Otherwise the position is returned unchanged.
[[nodiscard]] SaveVec3
decal_offset_for_save( SaveVec3 position, SaveVec3 landmark_offset, bool use_landmark,
                       int decal_flags ) noexcept;

// Read side (LoadClientState, sv_save.c:1349-1350): the inverse — the landmark
// offset is ADDED back under the same gate.  save(load(p)) == p for the same
// offset/flags (the round-trip the transition relies on).
[[nodiscard]] SaveVec3
decal_offset_for_load( SaveVec3 position, SaveVec3 landmark_offset, bool use_landmark,
                       int decal_flags ) noexcept;

// ---------------------------------------------------------------------------
// EntityInSolid (sv_save.c:476-489).
// ---------------------------------------------------------------------------

// The world-contents probe seam.  EntityInSolid tests whether an entity's
// bounding-box CENTER sits inside CONTENTS_SOLID after a transfer; the actual
// test is a world trace (SV_PointContents) owned by server-core, so it crosses
// this seam.  `group_mask` is pent->v.groupinfo — legacy stashes it in
// svs.groupmask before the trace (sv_save.c:486) so the contents query honours
// the entity's visibility group.
class ISolidTestProvider
{
public:
    ISolidTestProvider() noexcept                                = default;
    virtual ~ISolidTestProvider()                                = default;
    ISolidTestProvider( const ISolidTestProvider & )             = delete;
    ISolidTestProvider &operator=( const ISolidTestProvider & )  = delete;

    // SV_PointContents( point ) == CONTENTS_SOLID (sv_save.c:488), with
    // svs.groupmask == `group_mask` in force (:486).
    [[nodiscard]] virtual bool point_in_solid( const SaveVec3 &point, int group_mask ) noexcept = 0;
};

// EntityInSolid port (sv_save.c:476-489).  The MOVETYPE_FOLLOW-attached-to-a-
// client short-circuit (:482-483) is pure edict-field logic and is evaluated
// here; only the solid-center world-contents probe (:485-488) crosses the seam.
// Returns true when the entity's AABB center is in solid AND it is not a client-
// attached follower — i.e. the caller should suppress (FL_KILLME) it.
[[nodiscard]] bool entity_in_solid( const ::xash::abi::edict_t &pent, ISolidTestProvider &solid ) noexcept;

// ---------------------------------------------------------------------------
// Transition restore seam (CreateEntityTransitionList's per-entity work).
// ---------------------------------------------------------------------------

// The per-transition context handed to every restore call.  Mirrors the fields
// LoadAdjacentEnts sets on the adjacent level's SAVERESTOREDATA before
// CreateEntityTransitionList (sv_save.c:1970-1978):
struct TransitionContext
{
    // pSaveData->time = sv.time (sv_save.c:1970) — the FIELD_TIME rebase basis
    // a game DLL adds inside pfnRestore.  QUIRK: this is sv.time (the just-
    // spawned server's clock), NOT header.time rebased against it — the legacy
    // `// - header.time;` is commented out.  Contrast the FULL-restore path
    // (LevelStateLoader), which uses header.time.
    float    time_basis = 0.0f;
    // pSaveData->fUseLandmark = true (sv_save.c:1971).
    bool     use_landmark = true;
    // pSaveData->vecLandmarkOffset (sv_save.c:1976-1978) — added to
    // FIELD_POSITION_VECTOR fields inside the game-DLL restore.
    SaveVec3 landmark_offset{};
};

// The outcome of the global-entity merge pass (sv_save.c:1858-1888).
struct GlobalMergeResult
{
    // pfnRestore(pent, pSaveData, 1) > 0 (sv_save.c:1879): the DLL merged this
    // global's saved changes into the already-spawned instance.
    bool merged = false;
    // SV_FindGlobalEntity(classname, globalname) (sv_save.c:1872), non-null iff
    // a VALID already-spawned/local-restored global entity was found.  On a
    // FAILED merge the caller repoints the ETABLE row at this entity so decals
    // on the global's brush model can find their parent (sv_save.c:1885-1886);
    // null means leave the row's pent as-is.
    ::xash::abi::edict_t *repoint_target = nullptr;
};

// CreateEntityTransitionList's per-entity work, split by branch.  Reuses S8.4's
// RestoreCreateKind / RestoreConfig (level_state_loader.hpp) for the shared
// create-phase vocabulary.  A single fake implements the whole seam in tests.
class ITransitionRestorer
{
public:
    ITransitionRestorer() noexcept                                 = default;
    virtual ~ITransitionRestorer()                                 = default;
    ITransitionRestorer( const ITransitionRestorer & )            = delete;
    ITransitionRestorer &operator=( const ITransitionRestorer & ) = delete;

    // CreateEntitiesInRestoreList per-row creation, create_world==FALSE
    // (sv_save.c:1440-1453).  The port has already applied the create_world==
    // false row guard (`classname && size`, REMOVED NOT excluded — the `||
    // !create_world` term, :1428) and the `active = (flags & levelMask)` gate
    // (:1431); this is only called for a row that is BOTH guarded-in AND active.
    // No World kind occurs (worldspawn requires create_world; a row.id==0 falls
    // to Named, :1451-1453).  For Client the seam applies the residual
    // SV_IsValidEdict(ed) check and returns nullptr for an invalid client slot
    // (:1448); for Named an arena-exhaustion returns nullptr (legacy leaves pent
    // NULL).  Returns the created edict (or nullptr).
    [[nodiscard]] virtual ::xash::abi::edict_t *
    create_entity( RestoreCreateKind kind, int id, std::string_view classname ) noexcept = 0;

    // Physint override (svgame.physFuncs.pfnCreateEntitiesInRestoreList,
    // sv_save.c:1417-1419) — wholesale-replaces the built-in create loop.  A
    // restorer that owns creation populates every guarded row's `pent` (honouring
    // create_world==false + levelMask) and returns true; the default returns
    // false so the port runs its built-in contract.  DEFERRED body (game-DLL/
    // physint bridge, S8.7) — only the seam exists here.
    [[nodiscard]] virtual bool
    create_entities_override( EntityTable &table, int level_mask, const RestoreConfig &config ) noexcept
    {
        (void)table;
        (void)level_mask;
        (void)config;
        return false;
    }

    // The MOVEABLE / landmark transfer pass — pfnRestore(pent, pSaveData, 0),
    // shouldPrecache==0 (sv_save.c:1894).  The port has positioned the
    // [location, location+size) window and set currentIndex==table_index; `ctx`
    // carries the time basis + landmark offset the DLL applies.  Returns the
    // pfnRestore int: < 0 tells the port to FL_KILLME the edict (:1896); >= 0 is
    // a normal transfer (the port then runs the EntityInSolid pruning).
    [[nodiscard]] virtual int
    transfer_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                     std::span<const std::byte> entity_data, const TokenTable &tokens,
                     const TransitionContext &ctx ) noexcept = 0;

    // The GLOBAL-entity merge pass (sv_save.c:1858-1888): pre-read the entity's
    // ENTVARS (classname + globalname), SV_FindGlobalEntity, then pfnRestore(pent,
    // pSaveData, 1), shouldPrecache==1 — the DLL merges into the matching already-
    // spawned global rather than spawning a duplicate.  Same window/currentIndex/
    // ctx positioning as transfer_entity.  Returns whether the merge succeeded
    // and the repoint target (see GlobalMergeResult); the port owns the FL_KILLME
    // + table-repoint bookkeeping on failure.
    [[nodiscard]] virtual GlobalMergeResult
    merge_global_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                         std::span<const std::byte> entity_data, const TokenTable &tokens,
                         const TransitionContext &ctx ) noexcept = 0;

    // SV_FreeOldEntities() (sv_save.c:1917) — reap entities UTIL_Remove'd as a
    // side effect of a pfnRestore, called after EACH processed row.  Pure server-
    // core arena bookkeeping with no save-state effect; default no-op, hooked by
    // the S8.7 wiring.
    virtual void free_old_entities() noexcept {}
};

// ---------------------------------------------------------------------------
// CreateEntityTransitionList (sv_save.c:1836-1922).
// ---------------------------------------------------------------------------

// Drive one adjacent level's transfer given a computed `level_mask`.  Two
// legacy-ordered phases:
//   (1) CreateEntitiesInRestoreList(pSaveData, level_mask, create_world=false)
//       (:1845) — create a live edict for every row that is BOTH guarded-in
//       (`classname && size`; REMOVED rows fall through the guard but are
//       already inactive, see note) AND active (`row.flags & level_mask`), via
//       the restorer's create_entity (or the physint override).
//   (2) The restore loop (:1848-1919) — for every row with a valid pent AND
//       `row.flags & level_mask`:
//         • FENTTABLE_GLOBAL set  -> merge_global_entity; on merge movedCount++,
//           else repoint row.pent (if a valid target) + FL_KILLME the temp edict.
//         • otherwise             -> transfer_entity; on rc<0 FL_KILLME, else if
//           (not PLAYER && entity_in_solid) FL_KILLME, else row.flags =
//           FENTTABLE_REMOVED (PLAIN ASSIGN, clobbers) + movedCount++.
// A `.HL3`-patched row (apply_entity_patch set row.flags == FENTTABLE_REMOVED
// via a plain assign) is naturally inactive: bit 30 is not in level_mask and the
// clobber cleared every PVS bit, so `row.flags & level_mask == 0` — it is never
// created and never transferred, WITHOUT the create-guard needing to test
// REMOVED (which the create_world==false guard deliberately does not).
// Returns movedCount, or a SaveError for a STRUCTURALLY corrupt data window (an
// ETABLE row whose [location, location+size) falls outside the data region —
// reject-gracefully; legacy computes the pointer unchecked).  `state` must be a
// prior LevelStateLoader::load result over `table`.  Mutating -> asserts T_Main.
[[nodiscard]] Result<int>
create_entity_transition_list( LevelState &state, EntityTable &table,
                               ITransitionRestorer &restorer, ISolidTestProvider &solid,
                               int level_mask, const TransitionContext &ctx,
                               const RestoreConfig &config ) noexcept;

// ---------------------------------------------------------------------------
// LoadAdjacentEnts (sv_save.c:1931-2015) — AdjacentTransfer.
// ---------------------------------------------------------------------------

// The adjacent-level image + patch source/sink seam.  In legacy these are
// LoadSaveData / EntityPatchRead / EntityPatchWrite reaching the save directory
// by hand-built exact filename; the file I/O is S8.7 (save-boundary.md
// §Dependencies: filesystem is sibling-scope).  A missing file is a NORMAL
// outcome (nullopt), not an error, mirroring the legacy `FS_Open == NULL ->
// skip` guards.
class IAdjacentLevelSource
{
public:
    IAdjacentLevelSource() noexcept                                  = default;
    virtual ~IAdjacentLevelSource()                                  = default;
    IAdjacentLevelSource( const IAdjacentLevelSource & )             = delete;
    IAdjacentLevelSource &operator=( const IAdjacentLevelSource & )  = delete;

    // LoadSaveData(map) (sv_save.c:1963): the adjacent level's `.HL1` image, or
    // nullopt when the file does not exist — legacy's `if( pSaveData )` skips a
    // missing map silently (a connection whose level was never saved).
    [[nodiscard]] virtual std::optional<std::vector<std::byte>>
    load_level_image( std::string_view map_name ) noexcept = 0;

    // EntityPatchRead(map) (sv_save.c:1968, 1056-1077): the adjacent level's
    // `.HL3` patch image (raw int32 count + removed-row indices), or nullopt when
    // no `.HL3` exists (the common "nothing was ever removed from this level"
    // case).  Parsed via read_entity_patch + applied via apply_entity_patch.
    [[nodiscard]] virtual std::optional<std::vector<std::byte>>
    load_entity_patch( std::string_view map_name ) noexcept = 0;

    // EntityPatchWrite(map) (sv_save.c:1995, 1014-1046): rewrite the adjacent
    // level's `.HL3` with the CURRENT set of FENTTABLE_REMOVED rows (the
    // pre-existing removed rows unioned with the just-moved ones) after entities
    // transferred out.  Legacy failure is a Host_Error (transition-broken); a
    // returned SaveError propagates as SaveError::TransitionBroken.
    [[nodiscard]] virtual Result<void>
    store_entity_patch( std::string_view map_name, const EntityTable &table ) noexcept = 0;
};

// Narrow input set for one landmark transition (LoadAdjacentEnts args + the
// injected level-list + the sv.time / max_clients basis server-core owns).
// @lifetime: caller — every span/view member is BORROWED for the run() call.
struct AdjacentTransferParams
{
    // sv.name — the just-spawned map; the EntryInTable target that decides which
    // of an adjacent level's connections lead back here (sv_save.c:1985).
    std::string_view new_map_name{};
    // pOldLevel — the map we transitioned FROM: the foundprevious back-connection
    // (:1950/2013) and the FENTTABLE_PLAYER levelMask bit (:1980).
    std::string_view old_level{};
    // pLandmarkName (startspot) — the landmark whose two-frame origins give the
    // transfer offset (:1976-1977).
    std::string_view landmark_name{};
    // currentLevelData.levelList[0..connectionCount) — the NEW (just-spawned)
    // level's landmark connections, filled by pfnParmsChangeLevel (:1944); the
    // injected level-list the transition iterates.
    std::span<const ::xash::abi::LEVELLIST> new_level_connections{};
    // sv.time — the time-basis quirk (:1970).
    float sv_time = 0.0f;
    // svs.maxclients — the client-id upper bound the create dispatch uses.
    RestoreConfig config{};
};

// LoadAdjacentEnts (sv_save.c:1931-2015).  Iterates params.new_level_connections
// (dedup by mapName, case-insensitive, :1953-1961), and for each unique adjacent
// map that the `source` can supply: load its `.HL1`, apply its `.HL3` patch,
// compute the landmark offset + levelMask, run create_entity_transition_list,
// and — when entities moved — store_entity_patch back.  Tracks whether the old
// level appeared in the connection list; a MISSING back-connection is the hard-
// fail contract -> SaveError::TransitionBroken (legacy Host_Error, :2013-2014).
// The borrowed SaveBuffer + EntityTable are the per-map SAVERESTOREDATA working
// buffer + ETABLE (reused across connections; each load() re-inits them).
class AdjacentTransfer
{
public:
    // @lifetime: caller — `buf` and `table` are borrowed and must outlive every
    // run() call.
    AdjacentTransfer( SaveBuffer &buf, EntityTable &table ) noexcept
        : buf_( &buf ), table_( &table )
    {
    }

    // Mutating (drives the restorer/table across every adjacent level) -> asserts
    // T_Main.  DECAL move (LoadClientState, :2005) is NOT performed here — it is
    // the deferred live-decal application; only the offset math is exposed
    // (decal_offset_for_save/load above).
    [[nodiscard]] Result<void>
    run( const AdjacentTransferParams &params, IAdjacentLevelSource &source,
         ITransitionRestorer &restorer, ISolidTestProvider &solid ) noexcept;

private:
    SaveBuffer  *buf_;   // @lifetime: caller
    EntityTable *table_; // @lifetime: caller
};

} // namespace xash::save
