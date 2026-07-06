// xash3dpp — delta per-field payload codec (Layer 4 core)
// Legacy reference: engine/common/net_encode.c lines ~1022-1509.
//
// PARITY-CRITICAL: every arithmetic conversion below (sign-extended reads
// into unsigned, float multiply-then-truncate, uint divide-by-float) mirrors
// the legacy C expressions.  Do not "clean up" the conversion chains — the
// wire format depends on their exact truncation behaviour.  Golden vectors
// in tests/networking/delta/test_delta_field_codec.cpp pin the bit output.

#include <xash3dpp/private/networking/delta/field_codec.hpp>

#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstdint>
#include <cstring>

namespace xash::networking::delta {

namespace {

// Field-relative typed loads.  Sign-extended loads are deliberately
// assigned into wider signed/unsigned ints exactly like the legacy casts.
template <typename T>
[[nodiscard]] T load_as( const void *base, int offset ) noexcept // compliance-allow(thread-assert): stateless delta codec — pure wire transform, no thread affinity
{
    T v;
    std::memcpy( &v, static_cast<const std::uint8_t *>( base ) + offset, sizeof( T ));
    return v;
}

template <typename T>
void store_as( void *base, int offset, T v ) noexcept
{
    std::memcpy( static_cast<std::uint8_t *>( base ) + offset, &v, sizeof( T ));
}

// Signed-payload serialisation switch (legacy MSG_Write/ReadSBitLong with
// iAlternateSign).  Sign-magnitude mirrors the GoldSrc branch exactly:
// 1 sign bit first, then |value| in bits-1 (legacy uses abs()).
void write_sbits( MessageBuf &msg, int value, int bits, SignEncoding sign ) noexcept // compliance-allow(thread-assert): stateless delta codec — pure wire transform, no thread affinity
{
    if( sign == SignEncoding::SignMagnitude )
    {
        msg.write_one_bit( value < 0 ? 1 : 0 );
        msg.write_ubit_long(
            static_cast<std::uint32_t>( value < 0 ? -value : value ), bits - 1 );
    }
    else
    {
        msg.write_sbit_long( value, bits );
    }
}

[[nodiscard]] int read_sbits( MessageBuf &msg, int bits, SignEncoding sign ) noexcept
{
    if( sign == SignEncoding::SignMagnitude )
    {
        const int negative = msg.read_one_bit();
        const int r = static_cast<int>( msg.read_ubit_long( bits - 1 ));
        return negative ? -r : r;
    }
    return msg.read_sbit_long( bits );
}

} // namespace

// ---------------------------------------------------------------------------
// clamp_integer_field — legacy Delta_ClampIntegerField (debug-only overflow
// warning omitted; the clamp itself is the observable behaviour).
// ---------------------------------------------------------------------------

int clamp_integer_field( int value, int signbit, int numbits ) noexcept
{
    if( numbits < 32 )
    {
        const int signbits = numbits - signbit;
        const int maxnum   = ( 1 << signbits ) - 1;

        if( value > maxnum )
            value = maxnum;
        else if( signbit && value < -maxnum - 1 )
            value = -maxnum - 1;
    }

    return value;
}

// ---------------------------------------------------------------------------
// compare_field — legacy Delta_CompareField
// ---------------------------------------------------------------------------

bool compare_field( const DeltaField &field, const void *from, const void *to ) noexcept
{
    const int signbit = ( field.flags & k_dt_signed ) ? 1 : 0;
    int       fromF   = 0;
    int       toF     = 0;

    XASH_ASSERT( from != nullptr );
    XASH_ASSERT( to != nullptr );

    if( field.inactive )
        return true;

    if( field.flags & k_dt_byte )
    {
        if( signbit )
        {
            fromF = load_as<std::int8_t>( from, field.offset );
            toF   = load_as<std::int8_t>( to, field.offset );
        }
        else
        {
            fromF = load_as<std::uint8_t>( from, field.offset );
            toF   = load_as<std::uint8_t>( to, field.offset );
        }

        if( !q_equal( field.multiplier, 1.0f ))
        {
            fromF = static_cast<int>( fromF * field.multiplier );
            toF   = static_cast<int>( toF * field.multiplier );
        }

        fromF = clamp_integer_field( fromF, signbit, field.bits );
        toF   = clamp_integer_field( toF, signbit, field.bits );
    }
    else if( field.flags & k_dt_short )
    {
        if( signbit )
        {
            fromF = load_as<std::int16_t>( from, field.offset );
            toF   = load_as<std::int16_t>( to, field.offset );
        }
        else
        {
            fromF = load_as<std::uint16_t>( from, field.offset );
            toF   = load_as<std::uint16_t>( to, field.offset );
        }

        if( !q_equal( field.multiplier, 1.0f ))
        {
            fromF = static_cast<int>( fromF * field.multiplier );
            toF   = static_cast<int>( toF * field.multiplier );
        }

        fromF = clamp_integer_field( fromF, signbit, field.bits );
        toF   = clamp_integer_field( toF, signbit, field.bits );
    }
    else if( field.flags & k_dt_integer )
    {
        if( signbit )
        {
            fromF = load_as<std::int32_t>( from, field.offset );
            toF   = load_as<std::int32_t>( to, field.offset );
        }
        else
        {
            // Legacy stores *(uint32_t*) into an `int` — implementation-
            // defined wrap preserved via the uint32_t -> int cast.
            fromF = static_cast<int>( load_as<std::uint32_t>( from, field.offset ));
            toF   = static_cast<int>( load_as<std::uint32_t>( to, field.offset ));
        }

        if( !q_equal( field.multiplier, 1.0f ))
        {
            fromF = static_cast<int>( fromF * field.multiplier );
            toF   = static_cast<int>( toF * field.multiplier );
        }

        fromF = clamp_integer_field( fromF, signbit, field.bits );
        toF   = clamp_integer_field( toF, signbit, field.bits );
    }
    else if( field.flags & ( k_dt_angle | k_dt_float ))
    {
        // don't convert floats to integers — compare raw bit patterns
        // (so -0.0f vs 0.0f counts as a change, exactly like legacy)
        fromF = load_as<std::int32_t>( from, field.offset );
        toF   = load_as<std::int32_t>( to, field.offset );
    }
    else if( field.flags & k_dt_timewindow_8 )
    {
        const float val_a = load_as<float>( from, field.offset );
        const float val_b = load_as<float>( to, field.offset );
        fromF = q_rint( val_a * 100.0 );   // double math, like legacy
        toF   = q_rint( val_b * 100.0 );
    }
    else if( field.flags & k_dt_timewindow_big )
    {
        const float val_a = load_as<float>( from, field.offset );
        const float val_b = load_as<float>( to, field.offset );
        fromF = q_rint( val_a * field.multiplier ); // float math, like legacy
        toF   = q_rint( val_b * field.multiplier );
    }
    else if( field.flags & k_dt_string )
    {
        const char *s1 = static_cast<const char *>( from ) + field.offset;
        const char *s2 = static_cast<const char *>( to ) + field.offset;
        toF = ::xash::utilities::strcmp( s1, s2 ); // 0 == equal, like legacy
    }

    return fromF == toF;
}

// ---------------------------------------------------------------------------
// write_field_payload — legacy Delta_WriteField_
// ---------------------------------------------------------------------------

void write_field_payload( MessageBuf &msg, const DeltaField &field, // compliance-allow(thread-assert): stateless delta codec — pure wire transform, no thread affinity
                          const void *to, double timebase,
                          SignEncoding sign ) noexcept
{
    const int signbit = ( field.flags & k_dt_signed ) ? 1 : 0;

    if( field.flags & ( k_dt_byte | k_dt_short | k_dt_integer ))
    {
        // Legacy loads into `uint iValue` (sign-extension wraps to a large
        // unsigned), optionally multiplies through float, clamps through
        // int, then writes signed or unsigned per the flag.
        std::uint32_t iValue;

        if( field.flags & k_dt_byte )
            iValue = signbit
                ? static_cast<std::uint32_t>( load_as<std::int8_t>( to, field.offset ))
                : load_as<std::uint8_t>( to, field.offset );
        else if( field.flags & k_dt_short )
            iValue = signbit
                ? static_cast<std::uint32_t>( load_as<std::int16_t>( to, field.offset ))
                : load_as<std::uint16_t>( to, field.offset );
        else
            iValue = signbit
                ? static_cast<std::uint32_t>( load_as<std::int32_t>( to, field.offset ))
                : load_as<std::uint32_t>( to, field.offset );

        if( !q_equal( field.multiplier, 1.0f ))
            iValue = static_cast<std::uint32_t>(
                static_cast<float>( iValue ) * field.multiplier );

        iValue = static_cast<std::uint32_t>( clamp_integer_field(
            static_cast<int>( iValue ), signbit, field.bits ));

        if( signbit )
            write_sbits( msg, static_cast<std::int32_t>( iValue ), field.bits, sign );
        else
            msg.write_ubit_long( iValue, field.bits );
    }
    else if( field.flags & k_dt_float )
    {
        const float flValue = load_as<float>( to, field.offset );
        int         iValue  = static_cast<int>(
            static_cast<double>( flValue ) * field.multiplier );

        iValue = clamp_integer_field( iValue, signbit, field.bits );

        if( signbit )
            write_sbits( msg, iValue, field.bits, sign );
        else
            msg.write_ubit_long( static_cast<std::uint32_t>( iValue ), field.bits );
    }
    else if( field.flags & k_dt_angle )
    {
        // NOTE: never applies multipliers to angle because the result may
        // be wrong on the client side (legacy comment preserved).
        msg.write_bit_angle( load_as<float>( to, field.offset ), field.bits );
    }
    else if( field.flags & k_dt_timewindow_8 )
    {
        const float flValue = load_as<float>( to, field.offset );
        int dt = q_rint(( timebase - flValue ) * 100.0 );
        dt = clamp_integer_field( dt, 1, field.bits ); // always signed
        write_sbits( msg, dt, field.bits, sign );
    }
    else if( field.flags & k_dt_timewindow_big )
    {
        const float flValue = load_as<float>( to, field.offset );
        int dt = q_rint(( timebase - flValue ) * field.multiplier );
        dt = clamp_integer_field( dt, 1, field.bits ); // always signed
        write_sbits( msg, dt, field.bits, sign );
    }
    else if( field.flags & k_dt_string )
    {
        const char *pStr = static_cast<const char *>( to ) + field.offset;
        (void)msg.write_string( pStr ); // overflow tracked by the buffer
    }
}

// ---------------------------------------------------------------------------
// read_field_payload — legacy Delta_ReadField_
// ---------------------------------------------------------------------------

void read_field_payload( MessageBuf &msg, const DeltaField &field,
                         void *to, double timebase,
                         SignEncoding sign ) noexcept
{
    const bool bSigned = ( field.flags & k_dt_signed ) != 0;

    XASH_ASSERT( field.multiplier != 0.0f );

    if( field.flags & ( k_dt_byte | k_dt_short | k_dt_integer ))
    {
        std::uint32_t iValue = bSigned
            ? static_cast<std::uint32_t>( read_sbits( msg, field.bits, sign ))
            : msg.read_ubit_long( field.bits );

        if( !q_equal( field.multiplier, 1.0f ))
            iValue = static_cast<std::uint32_t>(
                static_cast<float>( iValue ) / field.multiplier );

        if( !q_equal( field.post_multiplier, 1.0f ))
            iValue = static_cast<std::uint32_t>(
                static_cast<float>( iValue ) * field.post_multiplier );

        if( field.flags & k_dt_byte )
        {
            if( bSigned )
                store_as<std::int8_t>( to, field.offset,
                                       static_cast<std::int8_t>( iValue ));
            else
                store_as<std::uint8_t>( to, field.offset,
                                        static_cast<std::uint8_t>( iValue ));
        }
        else if( field.flags & k_dt_short )
        {
            if( bSigned )
                store_as<std::int16_t>( to, field.offset,
                                        static_cast<std::int16_t>( iValue ));
            else
                store_as<std::uint16_t>( to, field.offset,
                                         static_cast<std::uint16_t>( iValue ));
        }
        else
        {
            store_as<std::uint32_t>( to, field.offset, iValue );
        }
    }
    else if( field.flags & k_dt_float )
    {
        const std::uint32_t iValue = bSigned
            ? static_cast<std::uint32_t>( read_sbits( msg, field.bits, sign ))
            : msg.read_ubit_long( field.bits );

        float flValue = bSigned
            ? static_cast<float>( static_cast<int>( iValue ))
            : static_cast<float>( iValue );

        if( !q_equal( field.multiplier, 1.0f ))
            flValue /= field.multiplier;

        if( !q_equal( field.post_multiplier, 1.0f ))
            flValue *= field.post_multiplier;

        store_as<float>( to, field.offset, flValue );
    }
    else if( field.flags & k_dt_angle )
    {
        store_as<float>( to, field.offset, msg.read_bit_angle( field.bits ));
    }
    else if( field.flags & k_dt_timewindow_8 )
    {
        const int iValue = read_sbits( msg, field.bits, sign );
        const float flTime = static_cast<float>(
            ( timebase * 100.0 - iValue ) / 100.0 );
        store_as<float>( to, field.offset, flTime );
    }
    else if( field.flags & k_dt_timewindow_big )
    {
        const int iValue = read_sbits( msg, field.bits, sign );
        const float flTime = static_cast<float>(
            ( timebase * field.multiplier - iValue ) / field.multiplier );
        store_as<float>( to, field.offset, flTime );
    }
    else if( field.flags & k_dt_string )
    {
        char *pOut = static_cast<char *>( to ) + field.offset;
        (void)msg.read_string({ pOut, static_cast<std::size_t>( field.size ) });
    }
}

// ---------------------------------------------------------------------------
// copy_field — legacy Delta_CopyField
// ---------------------------------------------------------------------------

void copy_field( const DeltaField &field, const void *from, void *to ) noexcept
{
    const bool bSigned = ( field.flags & k_dt_signed ) != 0;

    if( field.flags & k_dt_byte )
    {
        if( bSigned )
            store_as<std::int8_t>( to, field.offset,
                                   load_as<std::int8_t>( from, field.offset ));
        else
            store_as<std::uint8_t>( to, field.offset,
                                    load_as<std::uint8_t>( from, field.offset ));
    }
    else if( field.flags & k_dt_short )
    {
        if( bSigned )
            store_as<std::int16_t>( to, field.offset,
                                    load_as<std::int16_t>( from, field.offset ));
        else
            store_as<std::uint16_t>( to, field.offset,
                                     load_as<std::uint16_t>( from, field.offset ));
    }
    else if( field.flags & k_dt_integer )
    {
        store_as<std::uint32_t>( to, field.offset,
                                 load_as<std::uint32_t>( from, field.offset ));
    }
    else if( field.flags & ( k_dt_float | k_dt_angle |
                             k_dt_timewindow_8 | k_dt_timewindow_big ))
    {
        store_as<float>( to, field.offset, load_as<float>( from, field.offset ));
    }
    else if( field.flags & k_dt_string )
    {
        (void)::xash::utilities::strncpy(
            static_cast<char *>( to ) + field.offset,
            static_cast<const char *>( from ) + field.offset,
            static_cast<std::size_t>( field.size ));
    }
    else
    {
        XASH_ASSERT( false );
    }
}

} // namespace xash::networking::delta
