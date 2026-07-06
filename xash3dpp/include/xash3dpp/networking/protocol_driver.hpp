#pragma once
// xash3dpp — IProtocolDriver / IProtocolDriverRegistry: per-netchan game-protocol abstractions
// @thread-safety: stateless const drivers — Safe-RO singletons (registry immutable after construction)
// Legacy reference: engine/common/net_ws.c (SPLITPACKET / SPLITPACKETGS framing choice).
//
// These interfaces are part of NetworkInitParams (public API).  Callers that
// register additional protocol drivers must implement IProtocolDriverRegistry.
//
// IProtocolDriver is the per-client compat seam per the Q-12 paradigm
// (per-subsystem policy, never engine-wide).  The default GoldSrcProtocolDriver
// is always linked; alternative drivers register via IProtocolDriverRegistry.

#include <xash3dpp/networking/errors.hpp>

#include <cstdint>

namespace xash::networking {

class MessageBuf; // fwd-decl — full header included by implementers

// ---------------------------------------------------------------------------
// SplitFormat — which on-wire SPLITPACKET framing a driver uses
// ---------------------------------------------------------------------------

enum class SplitFormat : std::uint8_t
{
    Xash,     // packet_id is short; high byte = packet_number, low = packet_count
    GoldSrc,  // packet_id is unsigned char; high nibble = number, low = count
};

// ---------------------------------------------------------------------------
// DeltaTableSet — identifier for the field-table set this driver expects
// ---------------------------------------------------------------------------

enum class DeltaTableSet : std::uint8_t
{
    GoldSrc,
    Xash,
};

// ---------------------------------------------------------------------------
// FrameMeta — decoded packet header metadata
// ---------------------------------------------------------------------------

struct FrameMeta
{
    std::uint32_t sequence       { 0 };
    std::uint32_t sequence_ack   { 0 };
    bool          is_reliable    { false }; // w1 high bit — sender carries reliable bytes
    bool          reliable_ack   { false }; // w2 high bit — sender acks our reliable_sequence
    bool          is_fragment    { false }; // w1 bit-30 — reliable-fragment block descriptors follow
    bool          is_split       { false };
    bool          is_oob         { false };
};

// ---------------------------------------------------------------------------
// PacketHeaderInput — channel-state snapshot handed to the driver when
// writing a netchan packet header.  Mirrors the fields the legacy engine
// packed into the w1/w2 sequence words plus the optional qport.
// ---------------------------------------------------------------------------

struct PacketHeaderInput
{
    std::uint32_t outgoing_sequence            { 0 };
    std::uint32_t incoming_sequence            { 0 };
    std::uint32_t incoming_reliable_sequence   { 0 }; // 0 or 1 (single-bit flag in legacy)
    std::uint16_t qport                        { 0 }; // ignored when sends_qport()==false
    bool          send_reliable                { false };
    bool          send_reliable_fragment       { false };
    bool          is_client                    { false }; // qport is written only on client sockets
};

// ---------------------------------------------------------------------------
// IProtocolDriver — per-netchan_t game-protocol policy
// ---------------------------------------------------------------------------

struct IProtocolDriver
{
    virtual ~IProtocolDriver() = default;

    // Identifier reported through stats and diagnostics; static string.
    [[nodiscard]] virtual const char *name() const noexcept = 0;

    [[nodiscard]] virtual SplitFormat   split_format() const noexcept = 0;
    [[nodiscard]] virtual DeltaTableSet delta_tables() const noexcept = 0;

    // Whether this protocol includes a qport word in the client→server
    // header.  Legacy Xash netchan path: true; GoldSrc gs_netchan path: false.
    [[nodiscard]] virtual bool          sends_qport() const noexcept = 0;

    // Write the netchan packet header (w1, w2, optional qport, optional
    // reliable-fragment block descriptors).  The driver advances the
    // MessageBuf write cursor; on overflow the buffer's overflow flag is
    // set and the driver returns NetError::Overflow.  Reliable-fragment
    // descriptors are written only when send_reliable_fragment is true;
    // the netchan must populate the per-stream fragment metadata via a
    // future write_reliable_fragment_descriptors() call (TODO).
    [[nodiscard]] virtual Result<void> write_packet_header(
        MessageBuf &out,
        const PacketHeaderInput &in ) noexcept = 0;

    // Read the netchan packet header from `in`, advancing its read cursor.
    // Returns the decoded FrameMeta or NetError::BufferTooSmall on truncation.
    //
    // `is_server_socket` must be true when the *caller* is a server-side
    // channel reading a client→server datagram (where the client wrote a
    // qport word), and false when the caller is a client-side channel
    // reading a server→client datagram (no qport present).  The qport
    // is consumed but not returned — the caller already identified the
    // channel via the connection table before calling process().
    [[nodiscard]] virtual Result<FrameMeta> read_packet_header(
        MessageBuf &in, bool is_server_socket ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// IProtocolDriverRegistry — factory registry for alternative drivers
// ---------------------------------------------------------------------------

struct IProtocolDriverRegistry
{
    virtual ~IProtocolDriverRegistry() = default;

    // Resolve a protocol number to a driver instance owned by the registry.
    // Returns nullptr if the protocol number is unknown.  The default GoldSrc
    // driver is always available via this lookup with the wire protocol
    // number 48 or 49.
    [[nodiscard]] virtual IProtocolDriver *resolve( std::uint16_t protocol ) noexcept = 0;
};

} // namespace xash::networking
