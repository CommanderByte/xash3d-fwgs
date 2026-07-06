// xash3dpp — GoldSrc delta wire format: byte-group change masks
// Legacy reference: engine/common/net_encode.c Delta_WriteGSFields /
// Delta_ParseGSFields.
//
// Stream layout per struct:
//   3 bits — count of mask bytes that follow.  Legacy computes this as
//            (index of last changed field >> 3) + 1, NOT the number of
//            non-zero mask bytes — intermediate all-zero mask bytes are
//            still transmitted.
//   N bytes — change masks, bit i%8 of byte i/8 set when field i changed
//   payloads of every changed field, in table order (no mark bits)
//
// The 3-bit count caps the wire at 7 mask bytes = 56 fields; the legacy
// bits[8] array can flag a 57th..64th field but the count then wraps on the
// wire.  Real GoldSrc tables never exceed 56 fields (clientdata_t is exactly
// 56); replicated as-is with a debug assert, per the boundary spec.

#include <xash3dpp/private/networking/delta/wire_format.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/private/networking/delta/field_codec.hpp>

#include <cstdint>

namespace xash::networking::delta {

namespace {

struct GoldSrcDeltaWireFormat final : IDeltaWireFormat
{
    [[nodiscard]] const char *name() const noexcept override { return "goldsrc"; }

    [[nodiscard]] std::size_t write_fields( // compliance-allow(thread-assert): stateless const wire-format singleton — Safe-RO by construction (networking-threading.md)
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, const void *to, double timebase ) const noexcept override
    {
        std::uint8_t  bits[ ::xash::limits::net_delta_gs_mask_bytes ] = {};
        std::uint32_t c           = 0;
        std::size_t   num_changes = 0;

        // 3-bit count can express at most 7 mask bytes -> 56 fields.
        XASH_ASSERT( fields.size() <= ( ::xash::limits::net_delta_gs_mask_bytes - 1 ) * 8 );

        // Bound i>>3 to the mask array: legacy indexes bits[8] unchecked,
        // which is only safe because no GS-path table exceeds 56 fields.
        constexpr std::size_t k_mask_bytes = ::xash::limits::net_delta_gs_mask_bytes;

        for( std::size_t i = 0; i < fields.size() && ( i >> 3 ) < k_mask_bytes; ++i )
        {
            if( !compare_field( fields[ i ], from, to ))
            {
                const std::size_t b = i >> 3;
                bits[ b ] |= static_cast<std::uint8_t>( 1u << ( i & 7 ));
                c = static_cast<std::uint32_t>( b + 1 );
                ++num_changes;
            }
        }

        msg.write_ubit_long( c, k_gs_group_count_bits );
        for( std::uint32_t i = 0; i < c; ++i )
            msg.write_byte( bits[ i ] );

        for( std::size_t i = 0; i < fields.size() && ( i >> 3 ) < k_mask_bytes; ++i )
        {
            if( bits[ i >> 3 ] & ( 1u << ( i & 7 )))
                write_field_payload( msg, fields[ i ], to, timebase,
                                     SignEncoding::SignMagnitude );
        }

        return num_changes;
    }

    std::size_t read_fields(
        MessageBuf &msg, std::span<const DeltaField> fields,
        const void *from, void *to, double timebase ) const noexcept override
    {
        constexpr std::size_t k_mask_bytes = ::xash::limits::net_delta_gs_mask_bytes;
        std::uint8_t bits[ k_mask_bytes ] = {};
        std::size_t  num_changes = 0;

        XASH_ASSERT( fields.size() <= ( k_mask_bytes - 1 ) * 8 );

        const std::uint32_t c = msg.read_ubit_long( k_gs_group_count_bits );
        for( std::uint32_t i = 0; i < c; ++i )
            bits[ i ] = msg.read_byte();

        for( std::size_t i = 0; i < fields.size(); ++i )
        {
            // Fields past the mask array are unreachable on real tables
            // (<= 56 fields); treat them as unchanged rather than reading
            // out of bounds like the legacy bits[8] indexing would.
            const bool changed = ( i >> 3 ) < k_mask_bytes
                                 && ( bits[ i >> 3 ] & ( 1u << ( i & 7 )));
            if( changed )
            {
                read_field_payload( msg, fields[ i ], to, timebase,
                                    SignEncoding::SignMagnitude );
                ++num_changes;
            }
            else
            {
                copy_field( fields[ i ], from, to );
            }
        }

        return num_changes;
    }
};

} // namespace

const IDeltaWireFormat &goldsrc_delta_wire_format() noexcept
{
    static const GoldSrcDeltaWireFormat s_format;
    return s_format;
}

} // namespace xash::networking::delta
