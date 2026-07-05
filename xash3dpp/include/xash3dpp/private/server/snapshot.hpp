#pragma once
// xash3dpp — server snapshot / baseline pipeline (Chunk 6, S9 completion)
// Legacy reference: engine/server/sv_init.c:459 SV_CreateBaseline,
// engine/server/sv_frame.c (SV_WriteEntitiesToClient :613 /
// SV_AddEntitiesToPacket :58 / SV_EmitPacketEntities :235 /
// SV_FindBestBaseline :184), engine/server/sv_game.c pfnCreateInstancedBaseline
// (:4425).  Deep dive: docs/legacy-survey/deep-dive-server-world-frame.md §5.
//
// The byte-exact entity_state_t delta codec already lives in networking
// (DeltaTables::write_delta_entity / test_baseline); this module only
// orchestrates baseline data, the per-client visible-entity gather, the shared
// circular packet-entity ring, and the per-client frames ring.  The game DLL
// fills each baseline via pfnCreateBaseline and each visible state via
// pfnAddToFullPack — there is no engine-side SV_FillEntityState (deep dive §5).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/eiface.hpp>          // entity_state_t, string_t, edict_t
#include <xash3dpp/abi/entity_state.hpp>    // entity_state_t, clientdata_t
#include <xash3dpp/abi/weaponinfo.hpp>      // weapon_data_t
#include <xash3dpp/networking/message_buf.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::server {

struct ServerRuntime; // fwd
struct ServerClient;  // fwd (clients.hpp) — the frames ring hangs off it

// --- protocol / capacity constants (netchan.h / protocol.h, wire-frozen) ----

// sv.instanced[] cap (netchan.h MAX_CUSTOM_BASELINES); the signed 7-bit wire
// baseline offset (-i-1) references these, so the count is bounded to 63 usable
// (index 0..62) with the 6-bit num_instanced field (sv_init.c:538).
inline constexpr int k_max_custom_baselines = 64;

// per-client circular-ring sizing (netchan.h:75-79).
inline constexpr int k_singleplayer_backup  = 16;  // SV_UPDATE_BACKUP (SP)
inline constexpr int k_multiplayer_backup   = 64;  // SV_UPDATE_BACKUP (MP)
inline constexpr int k_num_packet_entities  = 256; // NUM_PACKET_ENTITIES

// snapshot gather + wire widths (protocol.h:98-122).
inline constexpr int k_max_visible_packet_bits = 11;         // MAX_VISIBLE_PACKET_BITS
inline constexpr int k_max_visible_packet      = 1 << 11;    // MAX_VISIBLE_PACKET (2048)
inline constexpr int k_max_entity_bits         = 13;         // MAX_ENTITY_BITS
inline constexpr int k_last_edict              = 8191;       // MAX_EDICTS-1 terminator
inline constexpr int k_max_entnumber           = 99999;      // MAX_ENTNUMBER merge sentinel
inline constexpr int k_max_edicts_bytes        = 1024;       // (MAX_EDICTS+7)/8 dedup mask
inline constexpr int k_max_local_weapons       = 64;         // MAX_LOCAL_WEAPONS

// snapshot-visibility server flags (sv.hostflags; server.h:42-43).
inline constexpr int k_svf_skiplocalhost  = 1 << 0; // SVF_SKIPLOCALHOST
inline constexpr int k_svf_merge_visibility = 1 << 1; // SVF_MERGE_VISIBILITY

// svc opcodes emitted by the snapshot + per-frame datagram (protocol.h).
inline constexpr int k_svc_time                = 7;  // [float] server time
inline constexpr int k_svc_setangle            = 10; // [angle*3] absolute view
inline constexpr int k_svc_clientdata          = 15; // [...] clientdata blob
inline constexpr int k_svc_addangle            = 38; // [angle] mover turn add
inline constexpr int k_svc_packetentities      = 40;
inline constexpr int k_svc_deltapacketentities = 41;
inline constexpr int k_svc_choke               = 42; // choke marker

// One instanced baseline: a classname-keyed template state shared by every
// entity of that class (pfnCreateInstancedBaseline, sv_game.c:4425).
struct InstancedBaseline
{
    ::xash::abi::string_t       classname = 0;  // into the server string pool
    ::xash::abi::entity_state_t baseline  = {};
};

// legacy client_frame_t (server.h:187-197): one snapshot the client can delta
// against.  Field order mirrors the legacy struct; clientdata/weapondata are
// filled by the send sub-slice (SV_WriteClientdataToMessage).  Pool-allocated
// as a per-client ring of SV_UPDATE_BACKUP frames.
struct ClientFrame
{
    double                     senttime  = 0.0;
    float                      ping_time = 0.0f;
    ::xash::abi::clientdata_t  clientdata = {};
    ::xash::abi::weapon_data_t weapondata[k_max_local_weapons] = {};
    int                        num_entities = 0;
    int                        first_entity = 0; // index into packet_entities
};

// svs.baselines + sv.instanced + the circular packet_entities ring + the gather
// scratch.  Owned by ServerRuntime.  `baselines` is pool-allocated from
// game_pool at load_progs (sized max_edicts); the ring + gather scratch are
// (re)allocated in setup_clients (sized from maxclients/SV_UPDATE_BACKUP); the
// per-client frames rings hang off ServerClient.  All are freed wholesale in
// snapshot_shutdown before game_pool destruction.
struct SnapshotState
{
    ::xash::abi::entity_state_t *baselines      = nullptr; // [baseline_count]
    int                          baseline_count = 0;       // == GI->max_edicts

    InstancedBaseline instanced[k_max_custom_baselines] = {}; // sv.instanced
    int               num_instanced       = 0; // sv.num_instanced
    int               last_valid_baseline = 0; // sv.last_valid_baseline

    // svs.packet_entities — the shared circular ring every client frame indexes
    // into (sv_init.c:826).  next_client_entities is the monotonic write cursor
    // (reset only on a ring realloc or the 0x7FFFFFFE overflow guard).
    ::xash::abi::entity_state_t *packet_entities     = nullptr; // [num_client_entities]
    int                          num_client_entities = 0;
    int                          next_client_entities = 0;
    int                          ring_maxclients     = 0; // size key for realloc
    int                          update_backup       = 0; // SV_UPDATE_BACKUP
    int                          update_mask         = 0; // SV_UPDATE_BACKUP-1

    // SV_WriteEntitiesToClient's static sv_ents_t (gather scratch, reused per
    // client): the visible states before qsort + ring copy, and the per-edict
    // dedup bitmask that stops portal passes double-adding an entity.
    ::xash::abi::entity_state_t *gather_ents = nullptr;      // [k_max_visible_packet]
    std::uint8_t                 sended[k_max_edicts_bytes] = {};
};

// SV_LoadProgs baseline alloc (sv_game.c:5342, Z_Calloc entity_state_t *
// max_edicts).  Idempotent — no-op when already sized.  Returns false (logged)
// on allocation failure.
[[nodiscard]] bool snapshot_alloc_baselines( ServerRuntime &rt ) noexcept;

// SV_SetupClients ring alloc (sv_init.c:825-827): (re)allocate svs.packet_
// entities (maxclients * SV_UPDATE_BACKUP * NUM_PACKET_ENTITIES), the gather
// scratch, and each client slot's frames ring, picking SV_UPDATE_BACKUP from
// maxclients (SP 16 / MP 64).  Idempotent when the size key is unchanged;
// frees and reallocates (resetting the ring cursor) when maxclients changes.
// Returns false (logged) on allocation failure.
[[nodiscard]] bool snapshot_alloc_ring( ServerRuntime &rt ) noexcept;

// SV_SpawnServer reset (sv_init.c:995 memset baselines; instanced counters
// cleared by the per-level sv memset).  Does NOT touch the persistent ring.
void snapshot_reset( ServerRuntime &rt ) noexcept;

// SV_UnloadProgs snapshot free (sv_game.c:5194 Z_Free svs.baselines +
// svs.packet_entities + per-client frames).  MUST run before game_pool
// destruction — the pool asserts on outstanding allocations.
void snapshot_shutdown( ServerRuntime &rt ) noexcept;

// SV_CreateBaseline fill half (sv_init.c:459-508): per valid edict set
// number/entityType, call the game DLL's pfnCreateBaseline to fill the state,
// track last_valid_baseline; then pfnCreateInstancedBaselines.  The signon
// serialization half (sv_init.c:510-544) lands with the signon buffer in the
// send sub-slice; the SP/MP voice-codec write is an OQ-8 stub.
void create_baselines( ServerRuntime &rt ) noexcept;

// pfnCreateInstancedBaseline callback (engine_table, eiface.hpp:369): append a
// classname-keyed template to sv.instanced[]; returns the new index, or the cap
// when full (legacy silently ignores past MAX_CUSTOM_BASELINES).
int create_instanced_baseline( SnapshotState &snap, ::xash::abi::string_t classname,
                               const ::xash::abi::entity_state_t *baseline ) noexcept;

// SV_WriteEntitiesToClient (sv_frame.c:613): gather the entities visible to
// `cl` through the game DLL's pfnSetupVisibility + pfnAddToFullPack hooks, qsort
// them by number, copy into the shared ring recording cl's frame, then emit the
// svc_(delta)packetentities delta.  `frame_index` is the netchan outgoing
// sequence (the send sub-slice supplies netchan.outgoing_sequence); events and
// pings ride the same message in later sub-slices.
void write_entities_to_client( ServerRuntime &rt, ServerClient &cl, int frame_index,
                               ::xash::networking::MessageBuf &msg ) noexcept;

// SV_WriteClientdataToMessage (sv_frame.c:526): stamp cl's frame (senttime /
// ping_time), emit svc_choke / fixangle (svc_setangle | svc_addangle), fill the
// frame's clientdata via pfnUpdateClientData, then svc_clientdata + the delta
// (against frames[delta_sequence].clientdata, or null) and — when local weapons
// are enabled — the 64-slot weapondata deltas.  `frame_index` selects cl's frame
// (netchan outgoing sequence in the full send path).
void write_clientdata_to_message( ServerRuntime &rt, ServerClient &cl,
                                  int frame_index,
                                  ::xash::networking::MessageBuf &msg ) noexcept;

// SV_SendClientDatagram (sv_frame.c:685): assemble cl's per-frame unreliable
// datagram body into `msg` — svc_time + sv.time, then clientdata, then the
// entity delta, then the accumulated per-client `datagram` staging (cleared
// after).  The Netchan transmit is the S8↔S9 seam (the host frame loop drives
// it); this only builds the message.
void send_client_datagram( ServerRuntime &rt, ServerClient &cl, int frame_index,
                           ::xash::networking::MessageBuf &msg ) noexcept;

} // namespace xash::server
