#pragma once
// xash3dpp — client machinery: connection state, messaging, satellites (S9)
// Legacy reference: engine/server/sv_client.c (connection state machine,
// userinfo, drop/timeout), sv_game.c (SV_RegUserMsg :3480, pfnMessageBegin
// :2534 / pfnMessageEnd :2620, SV_Multicast :354), sv_filter.c / sv_log.c /
// sv_query.c satellites, plus the svgame user-message registry
// (server.h :301-337) and the svs.clients array (server.h :190-260).
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md.
//
// Q-2 (no globals): the legacy svs.clients / svgame.msg[] / sv.multicast /
// ban-filter / log file-scope state all folds into ONE ClientMachinery
// aggregate owned by ServerRuntime, so S9 adds a single member to the
// runtime (clean merge boundary with the concurrent S8 physics slice).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/message_buf.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::server {

struct EngineBridge; // fwd — messaging free fns reach it for arena/game/state
struct ServerRuntime;

// --- protocol / capacity constants (protocol.h / server.h, wire-frozen) -----
inline constexpr int         k_max_clients        = 32;   // 1<<MAX_CLIENT_BITS
inline constexpr int         k_max_user_messages  = 197;  // MAX_USER_MESSAGES
inline constexpr int         k_svc_bad            = 0;
inline constexpr int         k_svc_temp_entity    = 23;
inline constexpr int         k_svc_lastmsg        = 59;   // user msgs start here
inline constexpr int         k_max_usermsg_length = 2048; // MAX_USERMSG_LENGTH
inline constexpr std::size_t k_max_multicast      = 8192; // MAX_MULTICAST
inline constexpr std::size_t k_max_info_string    = 256;  // MAX_INFO_STRING
inline constexpr std::size_t k_max_serverinfo     = 512;  // MAX_SERVERINFO_STRING
inline constexpr int         k_protocol_version   = 49;   // PROTOCOL_VERSION
inline constexpr int         k_challenge_window_s = 5;    // CHALLENGE_WINDOW_SECONDS
inline constexpr std::size_t k_client_stage_bytes = 1024; // per-client reliable/dgram
                                                          // (S8's netchan owns
                                                          // the full-size buffers)

// multicast destinations (common/const.h :574-583) — wire/ABI-frozen.
enum : int
{
    k_msg_broadcast     = 0,
    k_msg_one           = 1,
    k_msg_all           = 2,
    k_msg_init          = 3,
    k_msg_pvs           = 4,
    k_msg_pas           = 5,
    k_msg_pvs_r         = 6,
    k_msg_pas_r         = 7,
    k_msg_one_unreliable = 8,
    k_msg_spec          = 9,
};

// server.h :84-98 — connection state machine (integer order is not wire, but
// the transition rules are behavioural contract).
enum class ClientState : int
{
    Free = 0,
    Connected,
    Spawning,
    Spawned,
    Zombie,
};

// --- user-message registry (svgame.msg[]) -----------------------------------

struct UserMessage
{
    char name[16] = {}; // sv_user_message_t::name
    int  number   = 0;  // wire number = svc_lastmsg + slot
    int  size     = -1; // -1 = variable
};

struct UserMessageRegistry
{
    // slot 0 reserved (svc_bad); registration scans [1, k_max_user_messages).
    UserMessage msgs[k_max_user_messages] = {};

    // SV_RegUserMsg: dedupe by name, bound size to [-1, MAX_USERMSG_LENGTH],
    // assign number = svc_lastmsg + slot.  Returns the wire number (svc_bad on
    // error).  Does NOT do the ss_active late-broadcast (caller's job).
    [[nodiscard]] int register_message( const char *name, int size ) noexcept;

    // pfnMessageBegin lookup by wire number → slot index, or 0 if not found.
    [[nodiscard]] int slot_for_number( int number ) const noexcept;
};

// pfnMessageBegin/End in-flight state (svgame.msg_*).
struct MessageState
{
    bool          started    = false;
    int           index      = 0;   // >0 user slot; <0 system (-msg_num); 0 none
    const char   *name       = "";
    int           dest       = 0;   // MSG_*
    int           size_index = -1;  // byte offset of reserved word, -1 = fixed
    int           realsize   = 0;   // bytes written since the command byte
    float         org[3]     = {};
    ::xash::abi::edict_t *ent = nullptr;
};

// --- ban filters (sv_filter.c) ----------------------------------------------

struct IdBan
{
    double endtime = 0.0;  // 0 = permanent; else host.realtime expiry
    char   id[64]  = {};
    bool   used    = false;
};

struct IpBan
{
    double                    endtime  = 0.0;
    ::xash::networking::NetAddress adr {};
    std::uint8_t              prefix   = 32;
    bool                      used     = false;
};

struct BanFilters
{
    static constexpr int k_max_id_bans = 64;
    static constexpr int k_max_ip_bans = 64;

    IdBan id_bans[k_max_id_bans] = {};
    IpBan ip_bans[k_max_ip_bans] = {};
};

// --- server log (sv_log.c) --------------------------------------------------

struct ServerLog
{
    bool active   = false; // svs.log.active — `log on|off`
    bool net_log  = false; // logaddress configured
    ::xash::networking::NetAddress net_address{};
    // File I/O is a filesystem-satellite seam; the format + UDP path are here.
};

// --- one client slot (sv_client_t subset) -----------------------------------

struct ServerClient
{
    ClientState state      = ClientState::Free;
    bool        fakeclient = false;
    bool        hltv       = false;
    int         userid     = 0;
    int         extensions = 0; // NET_EXT_* granted

    ::xash::networking::NetAddress adr{};
    std::uint16_t                  qport = 0;

    char userinfo[k_max_info_string]  = {};
    char physinfo[k_max_info_string]  = {};
    char useragent[k_max_info_string] = {};
    char name[32]        = {};
    char hashedcdkey[34] = {};

    double connection_started  = 0.0; // realtime of the connect
    double connecttime         = 0.0; // set on "begin"
    double last_received       = 0.0; // netchan last_received mirror
    double next_messagetime    = 0.0;
    double next_messageinterval = 0.05;

    ::xash::abi::edict_t *edict = nullptr;

    // Per-client staging buffers.  SV_Multicast appends reliable payloads to
    // `reliable` and unreliable to `datagram`; S8's frame loop drains them
    // into the per-slot netchan.  (XASH3DPP-STUB(S8-seam) — see snapshot note.)
    std::byte   reliable[k_client_stage_bytes] = {};
    std::byte   datagram[k_client_stage_bytes] = {};
    std::size_t reliable_bits = 0;
    std::size_t datagram_bits = 0;
};

// --- the aggregate (svs.clients + svgame.msg + sv.multicast + filters/log) ---

struct ClientMachinery
{
    ServerClient clients[k_max_clients] = {};
    int          maxclients = 0;  // mirror of svs.maxclients (set at spawn)
    int          g_userid   = 1;  // monotonic; NEVER reset per map

    char serverinfo[k_max_serverinfo] = {};
    char localinfo[k_max_serverinfo]  = {};

    // Messaging: registry + in-flight state + the multicast scratch buffer.
    UserMessageRegistry user_messages;
    MessageState        message;
    std::byte           multicast_buf[k_max_multicast] = {};
    // Bound to multicast_buf by clients_init(); ServerRuntime is address-stable
    // for its whole lifetime so the self-reference is safe (never moved).
    ::xash::networking::MessageBuf multicast;

    BanFilters filters;
    ServerLog  log;

    // host.realtime mirror — the host/frame loop updates this each tick
    // (XASH3DPP-STUB(S8-seam)); the connection + timeout logic reads it.
    double realtime = 0.0;

    std::uint16_t challenge_salt_seeded = 0; // 0 until clients_init seeds bufs
};

// Bind the multicast MessageBuf and clear per-slot staging.  Idempotent; the
// lifecycle calls it once the game DLL is up (load_progs).
void clients_init( ClientMachinery &cm ) noexcept;

// ===========================================================================
// Messaging (pfn-driven — free fns over the bridge; state in bridge.clients)
// ===========================================================================

// SV_RegUserMsg wrapper: register + (when the server is active) broadcast the
// new registration to clients.  Returns the wire number.
[[nodiscard]] int reg_user_msg( EngineBridge &bridge, const char *name,
                                int size ) noexcept;

// pfnMessageBegin / pfnMessageEnd (sv_game.c:2534 / :2620).
void message_begin( EngineBridge &bridge, int dest, int num,
                    const float *origin, ::xash::abi::edict_t *ent ) noexcept;
void message_end( EngineBridge &bridge ) noexcept;

// pfnWrite* family — append into the multicast buffer, tracking realsize.
void message_write_byte( ClientMachinery &cm, int v ) noexcept;
void message_write_char( ClientMachinery &cm, int v ) noexcept;
void message_write_short( ClientMachinery &cm, int v ) noexcept;
void message_write_long( ClientMachinery &cm, int v ) noexcept;
void message_write_angle( ClientMachinery &cm, float v ) noexcept;
void message_write_coord( ClientMachinery &cm, float v ) noexcept;
void message_write_string( ClientMachinery &cm, const char *s ) noexcept;
void message_write_entity( ClientMachinery &cm, int v ) noexcept;

// SV_Multicast (sv_game.c:354): route sv.multicast to the recipient set and
// clear it.  Returns the number of clients the payload was appended to.
// The PVS/PHS mask path is honoured when a world + origin are available; with
// no mask the legacy "NULL mask → visible" rule sends to every eligible slot.
int sv_multicast( EngineBridge &bridge, int dest, const float *origin,
                  ::xash::abi::edict_t *ent, bool usermessage,
                  bool filter ) noexcept;

// ===========================================================================
// Connection state machine (host/packet-driven — free fns over ServerRuntime)
// ===========================================================================

// OOB reply sink — the host wires Netchan_OutOfBandPrint here; tests capture
// the connectionless reply text.  @lifetime: caller (set on ClientMachinery
// through connect_* params).
struct IOobSink
{
    virtual ~IOobSink() = default;
    virtual void send_oob( const ::xash::networking::NetAddress &to,
                           const char *text ) noexcept = 0;
};

// SV_GetChallenge (sv_client.c:73): MD5(ip_bytes ‖ salt[64] ‖ time_window),
// first four bytes little-endian.  `ip_bytes` empty ⇒ loopback ⇒ 0.
[[nodiscard]] std::int32_t compute_challenge(
    const std::uint32_t salt[16],
    ::xash::networking::NetAddress from,
    std::uint32_t time_window ) noexcept;

// SV_CheckChallenge: accept the current OR previous 5-second window.
[[nodiscard]] bool check_challenge( const ServerRuntime &rt,
                                    ::xash::networking::NetAddress from,
                                    std::int32_t challenge ) noexcept;

// SV_ConnectionlessPacket dispatch (subset): tokenizes `text` (already past
// the -1 marker) and routes getchallenge / connect / ping.  Replies through
// `sink`.  Returns true when the packet was recognised.
bool handle_connectionless( ServerRuntime &rt,
                            ::xash::networking::NetAddress from,
                            const char *text, IOobSink &sink ) noexcept;

// SV_ConnectClient core (sv_client.c:295) after tokenizing: validate protocol
// + challenge, find/reuse a slot, init it, run pfnClientConnect, reply
// client_connect.  Returns the slot index, or -1 on rejection.
int connect_client( ServerRuntime &rt, ::xash::networking::NetAddress from,
                    int protocol, std::int32_t challenge, const char *protinfo,
                    const char *userinfo, IOobSink &sink ) noexcept;

// SV_ExecuteClientCommand (sv_client.c:3103) subset: dispatch a client
// stringcmd.  Handles new / spawn / begin / disconnect / setinfo directly and
// forwards everything else to the game's pfnClientCommand.  This drives the
// cs_connected → cs_spawning → cs_spawned walk (calling pfnClientConnect at
// "new" and pfnClientPutInServer at "spawn").  The signon/serverdata payload
// build is an S8/send seam (see the .cpp).
void execute_client_command( ServerRuntime &rt, ServerClient &cl,
                             const char *cmd, IOobSink &sink ) noexcept;

// SV_UserinfoChanged (sv_client.c:1805) — name fixups (trim / console /
// empty / dedupe) + rate/updaterate; then pfnClientUserInfoChanged.
void userinfo_changed( ServerRuntime &rt, ServerClient &cl ) noexcept;

// SV_DropClient (sv_client.c:577): pfnClientDisconnect (if spawned) → zombie.
void drop_client( ServerRuntime &rt, ServerClient &cl, bool crash ) noexcept;

// SV_CheckTimeouts (sv_main.c:489): zombie→free, connect/spawn timeouts.
void check_timeouts( ServerRuntime &rt ) noexcept;

// SV_FakeConnect (sv_client.c:474, pfnCreateFakeClient): default-userinfo bot
// in an empty slot, straight to cs_spawned.  Returns the edict or nullptr.
[[nodiscard]] ::xash::abi::edict_t *
fake_connect( ServerRuntime &rt, const char *netname ) noexcept;

// SV_ClientById-style helpers (used by pfnGetPlayerUserId etc.).
[[nodiscard]] ServerClient *client_for_edict( ClientMachinery &cm,
                                              const ::xash::abi::edict_t *ed ) noexcept;

// ===========================================================================
// Ban filters (sv_filter.c)
// ===========================================================================

// SV_CheckID (sv_filter.c:62): mutual-prefix match, lazy expiry prune.
[[nodiscard]] bool filter_check_id( ClientMachinery &cm, const char *id ) noexcept;
// banid: minutes 0 = permanent.
void filter_add_id( ClientMachinery &cm, float minutes, const char *id ) noexcept;
void filter_remove_id( ClientMachinery &cm, const char *id ) noexcept;

// SV_CheckIP (sv_filter.c:352): linear NET_CompareAdrByMask, expired skipped.
[[nodiscard]] bool filter_check_ip( ClientMachinery &cm,
                                    ::xash::networking::NetAddress adr ) noexcept;
// addip: minutes < 0.1 = permanent.  `cidr` 0 ⇒ full 32/128-bit host match.
void filter_add_ip( ClientMachinery &cm, float minutes,
                    ::xash::networking::NetAddress adr,
                    std::uint8_t cidr ) noexcept;
void filter_remove_ip( ClientMachinery &cm, ::xash::networking::NetAddress adr,
                       std::uint8_t cidr ) noexcept;

// ===========================================================================
// Server log (sv_log.c)
// ===========================================================================

// Log_Printf (sv_log.c:101): prefix each line with "MM/DD/YYYY - HH:MM:SS: ",
// emit to the UDP logaddress when net_log (even if !active), and to the
// console/file when active.  `out`/`out_size` receive the fully-formatted line
// (incl. prefix) for testing/echo; UDP send routes through `sink` when set.
void log_printf( ClientMachinery &cm, const char *text, char *out,
                 std::size_t out_size ) noexcept;

// ===========================================================================
// Query responders (sv_query.c / SV_Info / netinfo)
// ===========================================================================

// SV_Info (sv_client.c:864): the Xash `info` query answer built from live
// server state.  Writes the infostring into `out` (NUL-terminated) and returns
// its length.  `protocol` mismatch ⇒ "<hostname>: wrong version".
std::size_t query_info( ServerRuntime &rt, int protocol, char *out,
                        std::size_t out_size ) noexcept;

} // namespace xash::server
