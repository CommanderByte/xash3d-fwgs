#pragma once
// xash3dpp — IProtocolDriver / IProtocolDriverRegistry: per-netchan game-protocol abstractions
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
    bool          is_reliable    { false };
    bool          is_split       { false };
    bool          is_oob         { false };
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

    // TODO(Chunk 4): write_packet_header / read_packet_header take a
    // MessageBuf parameter once the codec layer lands.
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
