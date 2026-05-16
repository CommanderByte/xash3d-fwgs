// xash3dpp — split-packet reassembler tests
// Exercises both Xash and GoldSrc fragment streams since the reassembler is
// format-agnostic past the decode step.

#include <xash3dpp/private/networking/split_packet.hpp>
#include <xash3dpp/private/networking/split_reassembler.hpp>

#include "../test_helpers.hpp"

#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

namespace
{

// Produce a stream of fragments for `payload` using the Xash producer, then
// decode each one back into a SplitFragmentInfo for feeding to the reassembler.
struct XashFragmentStream
{
    std::vector<std::vector<std::byte>> raw;
    std::vector<SplitFragmentInfo>      info;
};

XashFragmentStream make_xash_stream(
    std::span<const std::byte> payload, std::int32_t seq, std::size_t splitsize )
{
    XashFragmentStream s;
    SplitProducerXash  prod{ payload, seq, splitsize };
    while( !prod.exhausted() )
    {
        std::vector<std::byte> buf( splitsize );
        auto                   n = prod.next( buf );
        if( !n.has_value() || *n == 0 )
            break;
        buf.resize( *n );
        s.raw.push_back( std::move( buf ) );
    }
    for( auto &frag : s.raw )
    {
        auto decoded = decode_split_xash( frag );
        s.info.push_back( decoded.value() );
    }
    return s;
}

} // namespace

static void test_single_packet_roundtrip()
{
    std::vector<std::byte> payload( 800 );
    for( std::size_t i = 0; i < payload.size(); ++i )
        payload[i] = std::byte{ static_cast<std::uint8_t>( i & 0xFF ) };

    auto stream = make_xash_stream( payload, /*seq*/ 42, /*splitsize*/ 512 );
    CHECK( stream.info.size() >= 2 );

    SplitReassembler r;
    SplitReassembler::Ingested final_outcome{};
    for( const auto &frag : stream.info )
        final_outcome = r.ingest( frag );

    CHECK( final_outcome.outcome == SplitReassembler::Outcome::Complete );
    CHECK_EQ( static_cast<int>( final_outcome.assembled.size() ),
              static_cast<int>( payload.size() ) );
    CHECK( std::memcmp( final_outcome.assembled.data(),
                        payload.data(),
                        payload.size() ) == 0 );
}

static void test_out_of_order_delivery()
{
    std::vector<std::byte> payload( 1500, std::byte{ 0x7A } );
    auto stream = make_xash_stream( payload, /*seq*/ 1, /*splitsize*/ 512 );
    CHECK( stream.info.size() >= 3 );

    // Reverse the order.
    SplitReassembler r;
    SplitReassembler::Ingested final_outcome{};
    for( auto it = stream.info.rbegin(); it != stream.info.rend(); ++it )
        final_outcome = r.ingest( *it );

    CHECK( final_outcome.outcome == SplitReassembler::Outcome::Complete );
    CHECK_EQ( static_cast<int>( final_outcome.assembled.size() ),
              static_cast<int>( payload.size() ) );
    CHECK( std::memcmp( final_outcome.assembled.data(),
                        payload.data(),
                        payload.size() ) == 0 );
}

static void test_duplicate_fragment_returns_duplicate()
{
    std::vector<std::byte> payload( 1000, std::byte{ 0x05 } );
    auto stream = make_xash_stream( payload, /*seq*/ 2, /*splitsize*/ 512 );

    SplitReassembler r;
    auto first = r.ingest( stream.info[0] );
    CHECK( first.outcome == SplitReassembler::Outcome::Pending );
    auto dup = r.ingest( stream.info[0] );
    CHECK( dup.outcome == SplitReassembler::Outcome::Duplicate );
}

static void test_new_sequence_resets_state()
{
    std::vector<std::byte> p1( 1000, std::byte{ 0x11 } );
    std::vector<std::byte> p2( 1000, std::byte{ 0x22 } );
    auto s1 = make_xash_stream( p1, /*seq*/ 7, /*splitsize*/ 512 );
    auto s2 = make_xash_stream( p2, /*seq*/ 8, /*splitsize*/ 512 );

    SplitReassembler r;
    // Feed half of stream 1 then all of stream 2.
    r.ingest( s1.info[0] );
    SplitReassembler::Ingested final_outcome{};
    for( const auto &frag : s2.info )
        final_outcome = r.ingest( frag );

    CHECK( final_outcome.outcome == SplitReassembler::Outcome::Complete );
    CHECK_EQ( static_cast<int>( final_outcome.assembled.size() ),
              static_cast<int>( p2.size() ) );
}

static void test_discards_bad_fragment()
{
    SplitReassembler r;
    SplitFragmentInfo bad{};
    bad.sequence_number = 1;
    bad.packet_number   = 5;
    bad.packet_count    = 3; // number >= count
    auto out = r.ingest( bad );
    CHECK( out.outcome == SplitReassembler::Outcome::Discarded );
}

static void test_goldsrc_roundtrip()
{
    std::vector<std::byte> payload( 1200, std::byte{ 0xCC } );
    SplitProducerGoldSrc prod{ payload, /*seq*/ 99, /*splitsize*/ 600 };

    SplitReassembler r;
    SplitReassembler::Ingested final_outcome{};
    while( !prod.exhausted() )
    {
        std::vector<std::byte> frag( 600 );
        auto                   n = prod.next( frag );
        CHECK( n.has_value() && *n > 0 );
        frag.resize( *n );
        auto info = decode_split_goldsrc( frag );
        CHECK( info.has_value() );
        final_outcome = r.ingest( *info );
    }

    CHECK( final_outcome.outcome == SplitReassembler::Outcome::Complete );
    CHECK_EQ( static_cast<int>( final_outcome.assembled.size() ),
              static_cast<int>( payload.size() ) );
    CHECK( std::memcmp( final_outcome.assembled.data(),
                        payload.data(),
                        payload.size() ) == 0 );
}

int main()
{
    std::printf( "test_split_reassembler\n" );
    RUN_TEST( test_single_packet_roundtrip );
    RUN_TEST( test_out_of_order_delivery );
    RUN_TEST( test_duplicate_fragment_returns_duplicate );
    RUN_TEST( test_new_sequence_resets_state );
    RUN_TEST( test_discards_bad_fragment );
    RUN_TEST( test_goldsrc_roundtrip );
    std::printf( "test_split_reassembler: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
