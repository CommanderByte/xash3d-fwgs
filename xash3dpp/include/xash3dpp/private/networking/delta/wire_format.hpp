#pragma once
// xash3dpp — IDeltaWireFormat: pluggable delta field-framing seam (Layer 4)
// Legacy reference: engine/common/net_encode.c —
//   Xash path:    Delta_WriteField/Delta_ReadField loops (per-field mark bit)
//   GoldSrc path: Delta_WriteGSFields/Delta_ParseGSFields (byte-group masks)
//
// The two formats are SIBLINGS (Q-14: group-mask vs mark-bit is an algorithm
// divergence, not a policy flag), selected from the already-public
// IProtocolDriver::delta_tables() value.  A future wire format is one new
// sibling TU plus one case in delta_wire_format_for() — table management and
// struct-codec call sites stay untouched.
//
// Selection axis documentation lives in the networking boundary spec
// (docs/boundaries/networking-boundary.md, Delta encoder section).
//
// Formats only READ field descriptors; custom-encode activation happens in
// the table layer before these are called.

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/networking/protocol_driver.hpp> // DeltaTableSet

#include <cstddef>
#include <span>

namespace xash::networking { class MessageBuf; }

namespace xash::networking::delta {

struct IDeltaWireFormat
{
    virtual ~IDeltaWireFormat() = default;

    // Identifier for diagnostics; static string.
    [[nodiscard]] virtual const char *name() const noexcept = 0;

    // Serialise every changed field of `to` relative to `from`.
    // Returns the number of changed fields written (callers use it for
    // no-change rollback decisions).
    [[nodiscard]] virtual std::size_t write_fields(
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, const void *to, double timebase ) const noexcept = 0;

    // Deserialise one struct: changed fields from the stream, unchanged
    // fields copied over from `from`.
    virtual void read_fields(
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, void *to, double timebase ) const noexcept = 0;
};

// Static sibling instances (defined in wire_format_xash.cpp /
// wire_format_goldsrc.cpp).
[[nodiscard]] const IDeltaWireFormat &xash_delta_wire_format() noexcept;
[[nodiscard]] const IDeltaWireFormat &goldsrc_delta_wire_format() noexcept;

// Map the protocol driver's table-set identity to its wire format.
// Extend this switch when a new DeltaTableSet value is introduced.
[[nodiscard]] inline const IDeltaWireFormat &
delta_wire_format_for( DeltaTableSet set ) noexcept
{
    return set == DeltaTableSet::GoldSrc ? goldsrc_delta_wire_format()
                                         : xash_delta_wire_format();
}

} // namespace xash::networking::delta
