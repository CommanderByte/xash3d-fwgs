// xash3dpp — LagQueue tests

#include <xash3dpp/networking/lag_queue.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstring>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_empty_queue()
{
    LagQueue q;
    CHECK( q.empty() );
    CHECK( !q.try_dequeue( 1000 ).has_value() );
}

static void test_enqueue_dequeue_respects_delay()
{
    LagQueue q;
    const NetAddress peer = NetAddress::loopback_v4( 27015 );
    const std::byte payload[] = { std::byte{ 0x11 }, std::byte{ 0x22 }, std::byte{ 0x33 } };

    CHECK( q.enqueue( /*now*/ 100, /*delay*/ 50, peer, payload ) );
    CHECK_EQ( static_cast<int>( q.size() ), 1 );

    // Too early.
    CHECK( !q.try_dequeue( 100 ).has_value() );
    CHECK( !q.try_dequeue( 149 ).has_value() );

    // On the dot.
    auto out = q.try_dequeue( 150 );
    CHECK( out.has_value() );
    CHECK_EQ( static_cast<int>( out->data.size() ), 3 );
    CHECK( std::memcmp( out->data.data(), payload, sizeof( payload ) ) == 0 );
    CHECK( out->peer == peer );
    CHECK( q.empty() );
}

static void test_fifo_order_under_constant_delay()
{
    LagQueue q;
    const NetAddress peer = NetAddress::loopback_v4( 0 );
    const std::byte a[] = { std::byte{ 0xAA } };
    const std::byte b[] = { std::byte{ 0xBB } };
    const std::byte c[] = { std::byte{ 0xCC } };
    CHECK( q.enqueue( 100, 10, peer, a ) );
    CHECK( q.enqueue( 101, 10, peer, b ) );
    CHECK( q.enqueue( 102, 10, peer, c ) );

    auto p1 = q.try_dequeue( 200 );
    auto p2 = q.try_dequeue( 200 );
    auto p3 = q.try_dequeue( 200 );
    CHECK( p1 && p2 && p3 );
    CHECK_EQ( std::to_integer<int>( p1->data[0] ), 0xAA );
    CHECK_EQ( std::to_integer<int>( p2->data[0] ), 0xBB );
    CHECK_EQ( std::to_integer<int>( p3->data[0] ), 0xCC );
}

static void test_rejects_empty_payload()
{
    LagQueue q;
    NetAddress peer{};
    CHECK( !q.enqueue( 0, 0, peer, {} ) );
    CHECK( q.empty() );
}

static void test_clear()
{
    LagQueue q;
    NetAddress peer{};
    const std::byte b[] = { std::byte{ 1 } };
    CHECK( q.enqueue( 0, 0, peer, b ) );
    CHECK( q.enqueue( 0, 0, peer, b ) );
    q.clear();
    CHECK( q.empty() );
}

int main()
{
    std::printf( "test_lag_queue\n" );
    RUN_TEST( test_empty_queue );
    RUN_TEST( test_enqueue_dequeue_respects_delay );
    RUN_TEST( test_fifo_order_under_constant_delay );
    RUN_TEST( test_rejects_empty_payload );
    RUN_TEST( test_clear );
    std::printf( "test_lag_queue: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
