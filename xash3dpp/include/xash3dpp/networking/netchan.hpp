#pragma once
// xash3dpp — Netchan: reliable + unreliable + fragmented channel (Layer 3)
// Legacy reference: engine/common/net_chan.c, engine/common/netchan.h
//
// Netchan owns one peer's reliable-queue / unreliable-stream / file-stream
// state.  It sits on top of the Layer 2 wire-encoding helpers (split-packet
// fragmentation, OOB framing, optional compression) and is the seam through
// which the host (server: per-client; client: single instance) sends and
// receives all in-session traffic.
//
// Design constraints (see docs/architecture/networking/README.md):
//   * No globals; one Netchan per peer.  Caller-synchronised.
//   * No exceptions, no RTTI.  Errors are sticky overflow flags or bool returns.
//   * Pool-backed fragment storage via the parent NetworkContext's
//     PoolHandle("networking"); Netchan itself does not own the pool.
//   * Wire format is decided by the injected IProtocolDriver (Xash vs.
//     GoldSrc split framing, qport presence, etc.).  No #ifdef in Netchan.
//   * stats() reports Tier-1 atomics on NetworkingStats owned by the parent
//     NetworkContext.

#include <xash3dpp/memory/memory.hpp>            // PoolHandle
#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/networking/errors.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/networking/networking.hpp>   // SocketKind
#include <xash3dpp/networking/protocol_driver.hpp>
#include <xash3dpp/networking/stats.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace xash::networking {

// ---------------------------------------------------------------------------
// Stream index: legacy MAX_STREAMS = 2 (normal data, file download).
// ---------------------------------------------------------------------------

enum class FragStream : std::uint8_t
{
    Normal = 0,  // FRAG_NORMAL_STREAM — reliable game messages
    File   = 1,  // FRAG_FILE_STREAM   — downloads (resource transfer)
};

// ---------------------------------------------------------------------------
// FragSize — which buffer-sizing query the host wants answered.  Drives the
// IBlockSizeProvider callback (matches legacy `fragsize_t`).
// ---------------------------------------------------------------------------

enum class FragSize : std::uint8_t
{
    Fragment,    // single fragment payload bytes
    Split,       // split-packet body bytes
    Unreliable,  // unreliable tail payload bytes
};

// ---------------------------------------------------------------------------
// IBlockSizeProvider — host-supplied callback for per-channel fragment sizing.
// Legacy `pfnBlockSize` callback.  Returns 0 to mean "use default".
// ---------------------------------------------------------------------------

struct IBlockSizeProvider
{
    virtual ~IBlockSizeProvider() = default;
    [[nodiscard]] virtual int block_size( FragSize mode ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// NetchanFlags — bitfield matching legacy NETCHAN_USE_MUNGE / _BZIP2 / etc.
// ---------------------------------------------------------------------------

struct NetchanFlags
{
    bool use_munge   { false };
    bool use_bzip2   { false };
    bool use_lzss    { false }; // mutually exclusive with use_bzip2
    // Note: wire framing (Xash vs. GoldSrc qport / split header / frag
    // offset widths) is decided by IProtocolDriver::split_format() — never
    // mirrored here.  See docs/architecture/networking/protocol-driver.md.
};

// ---------------------------------------------------------------------------
// NetchanConfig — passed to Netchan::setup().  All fields except `driver`
// and `block_size_provider` may be defaulted.
// ---------------------------------------------------------------------------

struct NetchanConfig
{
    SocketKind                sock                  { SocketKind::Client };
    NetAddress                remote_address        {};
    std::uint16_t             qport                 { 0 };
    NetchanFlags              flags                 {};
    IProtocolDriver          *driver                { nullptr }; // required
    IBlockSizeProvider       *block_size_provider   { nullptr }; // required
    xash::memory::PoolHandle  pool                  {};          // required
        // ^ owned by the parent NetworkContext (PoolHandle("networking")).
        //   Netchan does not create or destroy the pool; setup() rejects
        //   an invalid handle the same way it rejects null callbacks.
    double                    rate                  { 9999.0 };
        // ^ bytes/second cap used by can_packet / update_choke.  Matches
        //   legacy DEFAULT_RATE.  Set <= 0 to disable bandwidth choking.
};

// ---------------------------------------------------------------------------
// Netchan — one logical reliable channel to a peer.
//
// Lifecycle:
//   Netchan c;                       // zero-state; not usable yet
//   c.setup(config);                 // arm; resets all sequence state
//   c.write_reliable(bytes);         // queue reliable payload
//   c.transmit(unreliable_payload);  // assemble + frame + send-out callback
//   c.process(incoming_datagram);    // demux + ack handling
//   c.clear();                       // flush all queued state
//
// Threading: caller-synchronised; not internally thread-safe.
// ---------------------------------------------------------------------------

class Netchan
{
public:
    Netchan() noexcept;
    ~Netchan();

    Netchan( const Netchan & )            = delete;
    Netchan &operator=( const Netchan & ) = delete;

    Netchan( Netchan && ) noexcept;
    Netchan &operator=( Netchan && ) noexcept;

    // ---- Lifecycle --------------------------------------------------------

    // Resets all state and arms the channel against the configured peer.
    // Returns false if `config.driver` or `config.block_size_provider` is
    // null.  After a successful setup, is_active() returns true.
    [[nodiscard]] bool setup( const NetchanConfig &config ) noexcept;

    // Flush all reliable + fragment queues; preserve identity (sock, peer,
    // qport, driver).  Equivalent to legacy Netchan_Clear.
    void clear() noexcept;

    [[nodiscard]] bool is_active() const noexcept;

    // ---- Reliable / fragment queue input ---------------------------------

    // Append `bytes` to the pending reliable message.  Returns false on
    // overflow (caller's reliable buffer too small).  Equivalent to writing
    // into legacy `chan->message`.
    [[nodiscard]] bool write_reliable( std::span<const std::byte> bytes ) noexcept;

    // Queue a generic in-memory blob for fragmented transmission on the
    // requested stream.  Equivalent to Netchan_CreateFragments.
    [[nodiscard]] Result<void> create_fragments( FragStream stream,
                                                 std::span<const std::byte> payload ) noexcept;

    // Queue a file-stream payload from an in-memory buffer.  Equivalent to
    // Netchan_CreateFileFragmentsFromBuffer.  `filename` is sent in the
    // remote file-header for save-on-disk semantics.
    [[nodiscard]] Result<void> create_file_fragments_from_buffer(
        std::string_view filename,
        std::span<const std::byte> payload ) noexcept;

    // ---- Transmit / process ----------------------------------------------

    // Assemble pending reliable + fragment + unreliable payloads into one
    // wire datagram and hand it to the caller via `out`.  Writes nothing
    // and returns 0 if the bandwidth choke is active.  Equivalent to
    // Netchan_TransmitBits with `unreliable` providing the tail payload.
    //
    // Returns the number of bytes written into `out`, or NetError::Overflow
    // if `out` is too small.
    [[nodiscard]] Result<std::size_t> transmit(
        std::span<const std::byte> unreliable,
        std::span<std::byte>       out ) noexcept;

    // Convenience overload: unreliable payload measured in bits, matching
    // the legacy Netchan_TransmitBits signature.  Calls transmit() above
    // after byte-aligning.
    [[nodiscard]] Result<std::size_t> transmit_bits(
        std::span<const std::byte> unreliable,
        std::size_t                length_in_bits,
        std::span<std::byte>       out ) noexcept;

    // Demux an incoming wire datagram into `msg` (post-header payload).
    // Updates incoming/outgoing sequence/ack state, processes reliable
    // acks, and accumulates any fragment payload.  Returns false on stale,
    // duplicate, or malformed packets (which are silently dropped).
    [[nodiscard]] bool process( std::span<const std::byte> datagram,
                                MessageBuf                 &msg ) noexcept;

    // ---- Receive-side fragment readiness ---------------------------------

    // True iff any incoming stream has a fully-assembled message ready to
    // be consumed via copy_normal_fragments / copy_file_fragments.
    [[nodiscard]] bool incoming_ready() const noexcept;

    // Move the assembled normal-stream fragments into `out`.  Returns the
    // number of bytes written, or 0 if no normal-stream message is ready.
    [[nodiscard]] Result<std::size_t> copy_normal_fragments(
        std::span<std::byte> out ) noexcept;

    // Move the assembled file-stream fragments into `out`.  Outputs the
    // filename into `filename_out` (caller-owned, NUL-terminated).
    [[nodiscard]] Result<std::size_t> copy_file_fragments(
        std::span<std::byte> out,
        std::span<char>      filename_out ) noexcept;

    // ---- Bandwidth / choke -----------------------------------------------

    // Returns true if the channel is allowed to send a packet now.  If
    // `choke` is false the choke is bypassed (loopback / OOB).  The caller
    // supplies the current time (same convention as LagQueue and
    // update_choke) — Netchan never reads a clock itself.
    [[nodiscard]] bool can_packet( double now_seconds, bool choke ) const noexcept;

    // Push the cleartime forward to throttle outgoing bandwidth.  Called by
    // the host frame tick to enforce the per-channel `rate` cap.
    void update_choke( double now_seconds, std::size_t bytes_sent ) noexcept;

    // ---- Accessors --------------------------------------------------------

    [[nodiscard]] const NetAddress  &remote_address()      const noexcept;
    [[nodiscard]] SocketKind         sock()                const noexcept;
    [[nodiscard]] std::uint16_t      qport()               const noexcept;
    [[nodiscard]] std::uint32_t      incoming_sequence()   const noexcept;
    [[nodiscard]] std::uint32_t      outgoing_sequence()   const noexcept;
    [[nodiscard]] double             last_received()       const noexcept;
    [[nodiscard]] double             connect_time()        const noexcept;
    [[nodiscard]] double             rate()                const noexcept;
    [[nodiscard]] IProtocolDriver   *driver()              const noexcept;

    // Pending reliable queue size, in bits.  Zero when there is no
    // reliable payload waiting to be sent.  Mirrors legacy
    // `chan->reliable_length`.
    [[nodiscard]] std::size_t        reliable_length_bits() const noexcept;

    // Number of pending fragment buffers queued for the given outgoing
    // stream (sum across all batches).  Zero when the stream is idle.
    // Used by telemetry and by tests; the legacy engine exposes the same
    // information via `chan->waitlist[stream]`.
    [[nodiscard]] std::size_t        pending_fragments( FragStream stream ) const noexcept;

    // Optional Tier-2 instrumentation.  Owned by the parent NetworkContext;
    // bound here at setup() time.  May be nullptr if the parent did not
    // wire stats.
    void                            bind_stats( NetworkingStats *stats ) noexcept;
    [[nodiscard]] NetworkingStats  *stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xash::networking
