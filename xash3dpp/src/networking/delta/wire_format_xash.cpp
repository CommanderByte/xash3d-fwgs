// xash3dpp — Xash delta wire format: one mark bit per field
// Legacy reference: engine/common/net_encode.c Delta_WriteField/Delta_ReadField
// loop shape (as used by MSG_WriteDeltaEntity and friends).
//
// Stream layout per field, in table order:
//   1 bit  — 0: unchanged (receiver copies from `from`)
//            1: changed   (payload follows, encoded by field_codec)

#include <xash3dpp/private/networking/delta/wire_format.hpp>

#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/field_codec.hpp>

namespace xash::networking::delta {

namespace {

struct XashDeltaWireFormat final : IDeltaWireFormat
{
    [[nodiscard]] const char *name() const noexcept override { return "xash"; }

    [[nodiscard]] std::size_t write_fields( // compliance-allow(thread-assert): stateless const wire-format singleton — Safe-RO by construction (networking-threading.md)
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, const void *to, double timebase ) const noexcept override
    {
        std::size_t num_changes = 0;

        for( const DeltaField &field : fields )
        {
            if( compare_field( field, from, to ))
            {
                msg.write_one_bit( 0 ); // unchanged
                continue;
            }

            msg.write_one_bit( 1 ); // changed
            write_field_payload( msg, field, to, timebase );
            ++num_changes;
        }

        return num_changes;
    }

    std::size_t read_fields(
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, void *to, double timebase ) const noexcept override
    {
        std::size_t num_changes = 0;

        for( const DeltaField &field : fields )
        {
            if( msg.read_one_bit())
            {
                read_field_payload( msg, field, to, timebase );
                ++num_changes;
            }
            else
            {
                copy_field( field, from, to );
            }
        }

        return num_changes;
    }
};

} // namespace

const IDeltaWireFormat &xash_delta_wire_format() noexcept
{
    static const XashDeltaWireFormat s_format;
    return s_format;
}

} // namespace xash::networking::delta
