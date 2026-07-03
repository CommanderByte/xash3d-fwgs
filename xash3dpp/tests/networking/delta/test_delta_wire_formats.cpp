// xash3dpp — delta wire-format framing tests (Xash mark-bit vs GoldSrc masks)
// Legacy reference: engine/common/net_encode.c Delta_WriteField loop /
// Delta_WriteGSFields / Delta_ParseGSFields.
//
// Golden buffers are hand-derived; MessageBuf packs LSB-first (first written
// bit = bit 0 of byte 0).  Derivations documented per test.

#include <xash3dpp/networking/delta.hpp>
#include <xash3dpp/networking/message_buf.hpp>
#include <xash3dpp/private/networking/delta/delta_types.hpp>
#include <xash3dpp/private/networking/delta/wire_format.hpp>

#include "../../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using namespace xash::networking;
using namespace xash::networking::delta;

static int g_pass = 0, g_fail = 0;

namespace
{

// Ten unsigned byte fields at offsets 0..9.
struct TenBytes
{
    std::uint8_t vals[10] {};
};

std::array<DeltaField, 10> make_ten_byte_fields()
{
    std::array<DeltaField, 10> fields{};
    static const char *names[10] =
    { "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9" };

    for( int i = 0; i < 10; ++i )
    {
        fields[ i ].name       = names[ i ];
        fields[ i ].offset     = i;
        fields[ i ].size       = 1;
        fields[ i ].flags      = k_dt_byte;
        fields[ i ].multiplier = 1.0f;
        fields[ i ].post_multiplier = 1.0f;
        fields[ i ].bits       = 8;
    }
    return fields;
}

[[nodiscard]] std::uint8_t byte_at( const MessageBuf &msg, std::size_t i )
{
    return std::to_integer<std::uint8_t>( msg.data()[ i ] );
}

} // namespace

// -- Xash mark-bit golden -----------------------------------------------------
// 3 fields, from {1,2,3} to {5,2,9}: middle unchanged.
// Stream: 1, 0x05(8), 0, 1, 0x09(8)  == 19 bits
//   byte0 bits 0-7: [1 | 0x05 LSB-first(1,0,1,0,0,0,0)] -> 0x0B
//   byte1 bits 8-15: [payload-bit7=0, mark2=0, mark3=1, 0x09 low5(1,0,0,1,0)]
//     bit10=1, bit11=1, bit14=1 -> 0x4C
//   byte2 bits 16-18: 0x09 high bits (0,0,0) -> 0x00

static void test_xash_mark_bit_golden()
{
    const auto ten = make_ten_byte_fields();
    const std::span<const DeltaField> fields{ ten.data(), 3 };

    TenBytes from{}, to{};
    from.vals[0] = 1; from.vals[1] = 2; from.vals[2] = 3;
    to.vals[0]   = 5; to.vals[1]   = 2; to.vals[2]   = 9;

    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };

    const std::size_t changes =
        xash_delta_wire_format().write_fields( msg, fields, &from, &to, 0.0 );

    CHECK_EQ( changes, 2u );
    CHECK_EQ( msg.num_bits_written(), 19u );
    CHECK_EQ( byte_at( msg, 0 ), 0x0Bu );
    CHECK_EQ( byte_at( msg, 1 ), 0x4Cu );
    CHECK_EQ( byte_at( msg, 2 ), 0x00u );

    TenBytes decoded{};
    msg.reset();
    xash_delta_wire_format().read_fields( msg, fields, &from, &decoded, 0.0 );
    CHECK_EQ( decoded.vals[0], 5u );
    CHECK_EQ( decoded.vals[1], 2u ); // copied from `from`
    CHECK_EQ( decoded.vals[2], 9u );
}

// -- GoldSrc group-mask golden --------------------------------------------------
// 10 fields, changes at idx 1 (val 7) and idx 9 (val 3).
// masks: byte0 = 0x02 (bit1), byte1 = 0x02 (bit9 -> bit1 of group 1)
// count c = (9>>3)+1 = 2  (last changed group + 1, intermediate groups sent)
// Stream: c(3 bits: 0,1,0), 0x02(8), 0x02(8), 0x07(8), 0x03(8) == 35 bits
//   byte0: [0,1,0 | 0x02 low5(0,1,0,0,0)] -> bit1 + bit4 = 0x12
//   byte1: [0x02 high3(0,0,0) | 0x02 low5(0,1,0,0,0)] -> bit12 = 0x10
//   byte2: [0x02 high3(0,0,0) | 0x07 low5(1,1,1,0,0)] -> bits19,20,21 = 0x38
//   byte3: [0x07 high3(0,0,0) | 0x03 low5(1,1,0,0,0)] -> bits27,28 = 0x18
//   byte4: [0x03 high3(0,0,0)] -> 0x00

static void test_goldsrc_group_mask_golden()
{
    const auto ten = make_ten_byte_fields();
    const std::span<const DeltaField> fields{ ten.data(), ten.size() };

    TenBytes from{}, to{};
    to.vals[1] = 7;
    to.vals[9] = 3;

    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };

    const std::size_t changes =
        goldsrc_delta_wire_format().write_fields( msg, fields, &from, &to, 0.0 );

    CHECK_EQ( changes, 2u );
    CHECK_EQ( msg.num_bits_written(), 3u + 2u * 8u + 2u * 8u );
    CHECK_EQ( byte_at( msg, 0 ), 0x12u );
    CHECK_EQ( byte_at( msg, 1 ), 0x10u );
    CHECK_EQ( byte_at( msg, 2 ), 0x38u );
    CHECK_EQ( byte_at( msg, 3 ), 0x18u );
    CHECK_EQ( byte_at( msg, 4 ), 0x00u );

    TenBytes decoded{};
    msg.reset();
    goldsrc_delta_wire_format().read_fields( msg, fields, &from, &decoded, 0.0 );
    for( int i = 0; i < 10; ++i )
        CHECK_EQ( decoded.vals[ i ], to.vals[ i ] );
}

// -- GoldSrc zero-change: 3 bits only -------------------------------------------

static void test_goldsrc_zero_change()
{
    const auto ten = make_ten_byte_fields();
    const std::span<const DeltaField> fields{ ten.data(), ten.size() };

    TenBytes from{};
    from.vals[4] = 44;
    TenBytes to = from;

    std::array<std::byte, 16> buf{};
    MessageBuf msg{ buf };

    const std::size_t changes =
        goldsrc_delta_wire_format().write_fields( msg, fields, &from, &to, 0.0 );

    CHECK_EQ( changes, 0u );
    CHECK_EQ( msg.num_bits_written(), 3u );
    CHECK_EQ( byte_at( msg, 0 ), 0x00u );

    TenBytes decoded{};
    msg.reset();
    goldsrc_delta_wire_format().read_fields( msg, fields, &from, &decoded, 0.0 );
    CHECK_EQ( decoded.vals[4], 44u );
}

// -- inactive fields are masked out in both formats -------------------------------

static void test_inactive_masked_in_both_formats()
{
    auto ten = make_ten_byte_fields();
    TenBytes from{}, to{};
    to.vals[0] = 11; // changed but will be marked inactive
    to.vals[2] = 22;

    ten[0].inactive = true;
    const std::span<const DeltaField> fields{ ten.data(), 3 };

    {
        std::array<std::byte, 16> buf{};
        MessageBuf msg{ buf };
        const std::size_t changes =
            xash_delta_wire_format().write_fields( msg, fields, &from, &to, 0.0 );
        CHECK_EQ( changes, 1u ); // only vals[2]

        TenBytes decoded{};
        msg.reset();
        xash_delta_wire_format().read_fields( msg, fields, &from, &decoded, 0.0 );
        CHECK_EQ( decoded.vals[0], 0u );  // suppressed
        CHECK_EQ( decoded.vals[2], 22u );
    }
    {
        std::array<std::byte, 16> buf{};
        MessageBuf msg{ buf };
        const std::size_t changes =
            goldsrc_delta_wire_format().write_fields( msg, fields, &from, &to, 0.0 );
        CHECK_EQ( changes, 1u );

        TenBytes decoded{};
        msg.reset();
        goldsrc_delta_wire_format().read_fields( msg, fields, &from, &decoded, 0.0 );
        CHECK_EQ( decoded.vals[0], 0u );
        CHECK_EQ( decoded.vals[2], 22u );
    }
}

// -- factory dispatch ---------------------------------------------------------------

static void test_factory_dispatch()
{
    const IDeltaWireFormat &gs = delta_wire_format_for( DeltaTableSet::GoldSrc );
    const IDeltaWireFormat &xs = delta_wire_format_for( DeltaTableSet::Xash );

    CHECK_STREQ( gs.name(), "goldsrc" );
    CHECK_STREQ( xs.name(), "xash" );
    CHECK( &gs != &xs );
    CHECK( &gs == &goldsrc_delta_wire_format());
    CHECK( &xs == &xash_delta_wire_format());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_xash_mark_bit_golden );
    RUN_TEST( test_goldsrc_group_mask_golden );
    RUN_TEST( test_goldsrc_zero_change );
    RUN_TEST( test_inactive_masked_in_both_formats );
    RUN_TEST( test_factory_dispatch );

    std::printf( "delta_wire_formats: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
