#pragma once
// xash3dpp — delta encoder public types (Layer 4)
// Legacy reference: engine/common/net_encode.h
//
// The delta encoder serialises differences between two instances of a frozen
// SDK struct (entity_state_t, usercmd_t, ...) into a bit stream.  Which wire
// framing is used (Xash per-field mark bits vs. GoldSrc byte-group masks) is
// selected via IProtocolDriver::delta_tables() — see
// private/networking/delta/wire_format.hpp.
//
// This header carries the value types shared across the delta TUs and the
// game-DLL-facing token types.  The DeltaTables lifecycle class is added by
// the table-management layer (see docs/boundaries/networking-boundary.md,
// Delta encoder section).

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>

namespace xash::filesystem { class Filesystem; }

namespace xash::abi {
struct entity_state_t;
struct clientdata_t;
struct usercmd_t;
struct weapon_data_t;
struct event_args_t;
struct movevars_t;
} // namespace xash::abi

namespace xash::networking {

class MessageBuf;

// ---------------------------------------------------------------------------
// DeltaStructId — identity of one delta description table.
// Wire-frozen: the enumerator order IS the legacy DT_* table index sent in
// svc_deltatable's 4-bit tableIndex field.  Never reorder.
// ---------------------------------------------------------------------------

enum class DeltaStructId : std::uint8_t
{
    Event = 0,         // DT_EVENT_T                "event_t"
    Movevars,          // DT_MOVEVARS_T             "movevars_t"
    Usercmd,           // DT_USERCMD_T              "usercmd_t"
    ClientData,        // DT_CLIENTDATA_T           "clientdata_t"
    WeaponData,        // DT_WEAPONDATA_T           "weapon_data_t"
    EntityState,       // DT_ENTITY_STATE_T         "entity_state_t"
    EntityStatePlayer, // DT_ENTITY_STATE_PLAYER_T  "entity_state_player_t"
    CustomEntityState, // DT_CUSTOM_ENTITY_STATE_T  "custom_entity_state_t"
    Count,
};

// ---------------------------------------------------------------------------
// DeltaEntityKind — which entity table an entity delta uses.
// Matches legacy DELTA_ENTITY / DELTA_PLAYER / DELTA_STATIC ("don't change
// order!").  Static entities skip the custom-encode callback.
// ---------------------------------------------------------------------------

enum class DeltaEntityKind : std::uint8_t
{
    Entity = 0,
    Player,
    Static,
};

// ---------------------------------------------------------------------------
// DeltaField — one runtime field descriptor (legacy `delta_t`).
//
// Game DLLs receive a DeltaField* array as the opaque token in their
// registered encode callback and mutate it ONLY through the engine's
// find/set/unset helpers.  A DLL that dereferences legacy `delta_s*`
// directly would see a different layout — documented quirk, revisited by
// the Chunk 6 ABI shim audit (see networking-boundary.md).
// ---------------------------------------------------------------------------

struct DeltaField
{
    const char   *name            { nullptr }; // points at static field-info data
    int           offset          { 0 };       // byte offset inside the SDK struct
    int           size            { 0 };       // bytes; bounds check for DT_STRING
    std::uint32_t flags           { 0 };       // DT_* bits (private delta_types.hpp)
    float         multiplier      { 1.0f };
    float         post_multiplier { 1.0f };    // DEFINE_DELTA_POST decode scale
    int           bits            { 0 };       // wire width
    bool          inactive        { false };   // unset by custom-encode request
};

// ---------------------------------------------------------------------------
// DeltaEncodeFn — game-DLL conditional-encode callback (legacy pfnDeltaEncode).
// Registered via the table manager; called before every write with all fields
// re-activated, may deactivate fields via set/unset helpers.  Raw function
// pointer so the future enginefuncs_t shim forwards without adaptation.
// `noexcept` deliberately absent: DLL function pointers are not noexcept-typed.
// ---------------------------------------------------------------------------

using DeltaEncodeFn = void ( * )( DeltaField *fields,
                                  const std::uint8_t *from,
                                  const std::uint8_t *to );

// ---------------------------------------------------------------------------
// IBaselineResolver — client-side baseline lookup for read_delta_entity.
// Parameterises the legacy clgame.static_entities / cls.packet_entities /
// cl.instanced_baseline global reads: the wire carries a signed 7-bit
// baseline offset; the client subsystem resolves it to an entity state.
// Return nullptr to keep the caller-supplied `from` (legacy out-of-range
// behaviour).
// ---------------------------------------------------------------------------

struct IBaselineResolver
{
    virtual ~IBaselineResolver() = default;

    // @lifetime: engine — returned state must outlive the read call.
    [[nodiscard]] virtual const ::xash::abi::entity_state_t *
    resolve( std::int32_t baseline_offset, DeltaEntityKind kind ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// Entity codec parameter structs (Q-4 named-params pattern)
// ---------------------------------------------------------------------------

struct WriteDeltaEntityParams
{
    bool            force      { false };            // baseline/forced full update
    DeltaEntityKind kind       { DeltaEntityKind::Entity };
    double          timebase   { 0.0 };
    int             baseline   { 0 };                // signed instanced-baseline index
    int             max_edicts { 0 };                // legacy GI->max_edicts bound
};

struct ReadDeltaEntityParams
{
    int               number    { 0 };               // already read by the dispatcher
    DeltaEntityKind   kind      { DeltaEntityKind::Entity };
    double            timebase  { 0.0 };
    int               max_entities { 0 };            // legacy clgame.maxEntities bound
    IBaselineResolver *baselines { nullptr };        // optional; @lifetime: engine
};

// ---------------------------------------------------------------------------
// DeltaStats — three-tier observability (see debug-stats-design.md).
// Tier-1 counters are always-on relaxed atomics; Tier-2 compiles in with
// XASH_STATS profiling builds.
// ---------------------------------------------------------------------------

struct DeltaStats
{
    std::atomic<std::uint64_t> tables_parsed   { 0 }; // script + wire descriptors applied
    std::atomic<std::uint64_t> structs_encoded { 0 };
    std::atomic<std::uint64_t> structs_decoded { 0 };

#if XASH_STATS
    std::atomic<std::uint64_t> fields_changed_written { 0 };
    std::atomic<std::uint64_t> fields_changed_read    { 0 };
    std::atomic<std::uint64_t> rollbacks              { 0 }; // no-change seek-backs
#endif
};

// ---------------------------------------------------------------------------
// DeltaTables — owns the eight runtime delta description tables.
// Legacy equivalent: the process-global dt_info[] plus Delta_Init /
// Delta_InitClient / Delta_Shutdown / Delta_AddEncoder / Delta_*Field.
//
// Ownership: constructed by the consumer (server per game instance; client
// once per session).  NOT owned by NetworkContext.  The server re-runs
// init() at every map spawn (legacy quirk 13); the game DLL then re-registers
// its encoders.  Caller-synchronised; stats() is safe from any thread.
// ---------------------------------------------------------------------------

class DeltaTables
{
public:
    DeltaTables() noexcept;
    ~DeltaTables();

    DeltaTables( const DeltaTables & )            = delete;
    DeltaTables &operator=( const DeltaTables & ) = delete;

    // Move supported; defined in delta_tables.cpp where Impl is complete.
    DeltaTables( DeltaTables && ) noexcept;
    DeltaTables &operator=( DeltaTables && ) noexcept;

    // ---- Lifecycle --------------------------------------------------------

    // Server path (legacy Delta_Init): clear existing state, parse
    // "delta.lst" via `fs`, apply the built-in movevars fallback when the
    // script does not specify a movevars_t section.  Returns false (after
    // logging) when the script is missing or structurally invalid.
    [[nodiscard]] bool init( ::xash::filesystem::Filesystem &fs ) noexcept;

    // Same parse over an in-memory script (unit tests; also the seam
    // init() itself uses).  Applies the movevars fallback identically.
    [[nodiscard]] bool init_from_script( std::string_view script ) noexcept;

    // Client path (legacy Delta_InitClient): mark every table that already
    // has fields (received via descriptor messages) as initialised.
    void init_client() noexcept;

    // Legacy Delta_Shutdown: reset all tables, drop callbacks.  Idempotent.
    void clear() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept; // legacy delta_init

    // ---- Game-DLL hook (ABI shape mirrors enginefuncs_t, Chunk 6 shim) ----

    // Legacy Delta_AddEncoder.  Returns false (after logging) when no
    // initialised table carries `name` as its script encoder, or the table
    // is not marked for custom encoding.
    [[nodiscard]] bool register_encoder( const char *name, DeltaEncodeFn fn ) noexcept;

    // Legacy Delta_FindField / Delta_Set/UnsetField[ByIndex].  `fields` is
    // the opaque token handed to the game DLL callback (a table's field
    // array); helpers resolve the owning table by pointer identity.
    [[nodiscard]] int find_field( const DeltaField *fields,
                                  const char *fieldname ) const noexcept;
    void set_field( DeltaField *fields, const char *fieldname ) noexcept;
    void unset_field( DeltaField *fields, const char *fieldname ) noexcept;
    void set_field_by_index( DeltaField *fields, int field_number ) noexcept;
    void unset_field_by_index( DeltaField *fields, int field_number ) noexcept;

    // ---- Table-descriptor wire (table sync at connect) ---------------------
    //
    // Seam shift vs. legacy: net_encode.c wrote the svc_deltatable command
    // byte internally via MSG_BeginServerCmd; here the caller supplies the
    // raw command value (wire-identical, message-ID-agnostic — xash3dpp has
    // no svc_* enum yet).  See networking-boundary.md, Delta encoder section.

    // Legacy Delta_WriteDescriptionToClient: emits one descriptor message
    // (command byte + tableIndex/nameIndex/flags/bits/multipliers) per field
    // of every table.
    void write_description( MessageBuf &msg,
                            std::uint32_t svc_deltatable_cmd ) noexcept;

    // Legacy Delta_ParseTableField (Xash path; command byte already consumed
    // by the caller's dispatcher).  Applies one field descriptor; wipes all
    // local tables first when arriving over a live local-game setup (legacy
    // quirk).  Returns false only on a malformed/unknown table index.
    [[nodiscard]] bool parse_table_field( MessageBuf &msg ) noexcept;

    // Legacy Delta_ParseTableField_GS: one whole GoldSrc table description
    // (struct name string + field count + GS-framed goldsrc_delta_t records
    // through the immutable meta-table, DT_SIGNED_GS remapped).  Byte-aligns
    // the read cursor afterwards like legacy MSG_EndBitWriting.
    [[nodiscard]] bool parse_table_gs( MessageBuf &msg ) noexcept;

    // ---- Struct codecs (Xash mark-bit framing) ------------------------------
    //
    // Each mirrors its legacy MSG_* function including the no-change
    // rollback behaviours.  Command bytes are caller-supplied (see the
    // descriptor-wire note above).  All are caller-synchronised.

    // usercmd_t — client writes, server reads (legacy reversed direction).
    // Read normalises viewangles to [-180, 180] like COM_NormalizeAngles.
    void write_delta_usercmd( MessageBuf &msg, const ::xash::abi::usercmd_t *from,
                              const ::xash::abi::usercmd_t *to ) noexcept;
    void read_delta_usercmd( MessageBuf &msg, const ::xash::abi::usercmd_t *from,
                             ::xash::abi::usercmd_t *to ) noexcept;

    void write_delta_event( MessageBuf &msg, const ::xash::abi::event_args_t *from,
                            const ::xash::abi::event_args_t *to ) noexcept;
    void read_delta_event( MessageBuf &msg, const ::xash::abi::event_args_t *from,
                           ::xash::abi::event_args_t *to ) noexcept;

    // Writes the caller-supplied command byte first; on zero changes the
    // whole message (command byte included) is rolled back and false is
    // returned (legacy MSG_WriteDeltaMovevars).
    [[nodiscard]] bool write_delta_movevars(
        MessageBuf &msg, const ::xash::abi::movevars_t *from,
        const ::xash::abi::movevars_t *to,
        std::uint32_t svc_deltamovevars_cmd ) noexcept;
    void read_delta_movevars( MessageBuf &msg, const ::xash::abi::movevars_t *from,
                              ::xash::abi::movevars_t *to ) noexcept;

    // 1 "have clientdata" bit; zero changes rewrite it to a single 0 bit.
    void write_clientdata( MessageBuf &msg, const ::xash::abi::clientdata_t *from,
                           const ::xash::abi::clientdata_t *to,
                           double timebase ) noexcept;
    void read_clientdata( MessageBuf &msg, const ::xash::abi::clientdata_t *from,
                          ::xash::abi::clientdata_t *to, double timebase ) noexcept;

    // 1 bit + 6-bit weapon index; fully rolled back on zero changes.
    void write_weapon_data( MessageBuf &msg, const ::xash::abi::weapon_data_t *from,
                            const ::xash::abi::weapon_data_t *to,
                            double timebase, int index ) noexcept;
    void read_weapon_data( MessageBuf &msg, const ::xash::abi::weapon_data_t *from,
                           ::xash::abi::weapon_data_t *to, double timebase ) noexcept;

    // Entity states.  write: `to == nullptr` emits a remove message
    // (force selects removeType 2); returns false on a bad entity number
    // (legacy Host_Error).  read: returns false when the entity was
    // removed (to->number == -1 for a full server remove), true when a
    // state was parsed.
    [[nodiscard]] bool write_delta_entity(
        MessageBuf &msg, const ::xash::abi::entity_state_t *from,
        const ::xash::abi::entity_state_t *to,
        const WriteDeltaEntityParams &params ) noexcept;
    [[nodiscard]] bool read_delta_entity(
        MessageBuf &msg, const ::xash::abi::entity_state_t *from,
        ::xash::abi::entity_state_t *to,
        const ReadDeltaEntityParams &params ) noexcept;

    // Bit-count estimate for baseline selection (legacy Delta_TestBaseline).
    [[nodiscard]] int test_baseline( const ::xash::abi::entity_state_t *from,
                                     const ::xash::abi::entity_state_t *to,
                                     bool player, double timebase ) noexcept;

    // ---- GoldSrc batch codec (group-mask framing) ---------------------------
    //
    // Legacy Delta_Write/ReadGSFields.  Signed payloads use the GoldSrc
    // sign-magnitude layout; the caller owns message-level byte alignment
    // (legacy MSG_Start/EndBitWriting brackets).
    void write_gs_fields( MessageBuf &msg, DeltaStructId id,
                          const void *from, const void *to,
                          double timebase ) noexcept;
    void read_gs_fields( MessageBuf &msg, DeltaStructId id,
                         const void *from, void *to, double timebase ) noexcept;

    // ---- Introspection (engine-internal; used by codecs and tests) --------

    [[nodiscard]] bool table_initialized( DeltaStructId id ) const noexcept;
    [[nodiscard]] int  table_field_count( DeltaStructId id ) const noexcept;

    // Borrowed token for the table's field array (what the game DLL sees).
    // @lifetime: engine — valid until the next init()/clear().
    [[nodiscard]] DeltaField *table_fields( DeltaStructId id ) noexcept;

    // ---- Stats -------------------------------------------------------------

    [[nodiscard]] const DeltaStats &stats() const noexcept;

    struct Impl; // shared with the delta TUs (table_wire / delta_codec)

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::networking
