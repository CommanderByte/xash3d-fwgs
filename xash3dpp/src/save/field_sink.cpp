// xash3dpp — save/restore field-record framing implementation (Chunk 8, S8.1).
// Wire record: short size, short token idx, payload (field_sink.hpp), LE.

#include <xash3dpp/private/save/field_sink.hpp>

#include <xash3dpp/private/save/save_buffer.hpp>

#include <xash3dpp/core/thread_role.hpp>

namespace xash::save {

namespace
{
// Decode a little-endian 16-bit value from a 2-byte span.
[[nodiscard]] std::uint16_t read_u16_le( std::span<const std::byte> b ) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
        ( static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) );
}
} // namespace

Result<FieldRecord>
next_field_record( std::span<const std::byte> data, std::size_t &offset ) noexcept
{
    // Header: short size, short token idx.
    if ( offset + k_field_record_header_bytes > data.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    const std::uint16_t field_size = read_u16_le( data.subspan( offset, 2 ) );
    const std::uint16_t token_idx  = read_u16_le( data.subspan( offset + 2, 2 ) );

    const std::size_t payload_off = offset + k_field_record_header_bytes;
    if ( payload_off + field_size > data.size() )
        return std::unexpected( SaveError::BadFieldRecord ); // oversize/inconsistent

    FieldRecord rec;
    rec.token_idx = token_idx;
    rec.payload   = data.subspan( payload_off, field_size );

    offset = payload_off + field_size; // advance past the whole record
    return rec;
}

Result<void>
SaveBufferSink::write_field_record( std::uint16_t token_idx,
                                    std::span<const std::byte> payload ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // The size field is a 16-bit short; a larger payload is unrepresentable.
    if ( payload.size() > 0xFFFFu )
        return std::unexpected( SaveError::BadFieldRecord );

    // Atomicity: reject up front if the whole record would not fit, so a
    // partial record is never left in the buffer.
    if ( buf_->cursor() + k_field_record_header_bytes + payload.size() > buf_->capacity() )
        return std::unexpected( SaveError::BufferExhausted );

    // short size, short token idx, payload (all LE via SaveBuffer primitives).
    if ( auto r = buf_->write_i16( static_cast<std::int16_t>(
             static_cast<std::uint16_t>( payload.size() ) ) );
         !r )
        return r;
    if ( auto r = buf_->write_i16( static_cast<std::int16_t>( token_idx ) ); !r )
        return r;
    return buf_->write_bytes( payload );
}

} // namespace xash::save
