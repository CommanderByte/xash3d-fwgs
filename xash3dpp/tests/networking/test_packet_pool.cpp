// xash3dpp — PacketPool tests

#include <xash3dpp/private/networking/packet_pool.hpp>

#include "../test_helpers.hpp"

#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

static void test_initial_state()
{
    PacketPool pool;
    CHECK_EQ( static_cast<int>( pool.in_use() ), 0 );
    CHECK_EQ( static_cast<int>( pool.capacity() ),
              static_cast<int>( PacketPool::slot_count ) );
}

static void test_acquire_release()
{
    PacketPool pool;
    {
        auto s = pool.acquire();
        CHECK( s.has_value() );
        CHECK( s->valid() );
        CHECK( s->bytes().size() == PacketPool::slot_capacity );
        CHECK_EQ( static_cast<int>( pool.in_use() ), 1 );
    }
    CHECK_EQ( static_cast<int>( pool.in_use() ), 0 );
}

static void test_unique_slots()
{
    PacketPool pool;
    auto a = pool.acquire();
    auto b = pool.acquire();
    CHECK( a.has_value() && b.has_value() );
    CHECK( a->index() != b->index() );
    CHECK( a->bytes().data() != b->bytes().data() );
}

static void test_exhaust_pool()
{
    PacketPool pool;
    std::vector<PacketSlot> held;
    for( std::size_t i = 0; i < PacketPool::slot_count; ++i )
    {
        auto s = pool.acquire();
        CHECK( s.has_value() );
        held.push_back( std::move( *s ) );
    }
    CHECK_EQ( static_cast<int>( pool.in_use() ),
              static_cast<int>( PacketPool::slot_count ) );
    auto extra = pool.acquire();
    CHECK( !extra.has_value() );

    held.clear();
    CHECK_EQ( static_cast<int>( pool.in_use() ), 0 );
    // Pool is reusable after release.
    auto after = pool.acquire();
    CHECK( after.has_value() );
}

static void test_move_transfers_ownership()
{
    PacketPool pool;
    auto a = pool.acquire();
    CHECK( a.has_value() );
    CHECK_EQ( static_cast<int>( pool.in_use() ), 1 );
    PacketSlot moved = std::move( *a );
    CHECK( moved.valid() );
    CHECK( !a->valid() );
    CHECK_EQ( static_cast<int>( pool.in_use() ), 1 );
    // Drop the move target by releasing manually.
    moved.release();
    CHECK_EQ( static_cast<int>( pool.in_use() ), 0 );
}

static void test_set_length_resizes_span()
{
    PacketPool pool;
    auto s = pool.acquire();
    CHECK( s.has_value() );
    s->set_length( 64 );
    CHECK_EQ( static_cast<int>( s->bytes().size() ), 64 );
    // Excessive length is ignored.
    s->set_length( PacketPool::slot_capacity * 2 );
    CHECK_EQ( static_cast<int>( s->bytes().size() ), 64 );
}

int main()
{
    std::printf( "test_packet_pool\n" );
    RUN_TEST( test_initial_state );
    RUN_TEST( test_acquire_release );
    RUN_TEST( test_unique_slots );
    RUN_TEST( test_exhaust_pool );
    RUN_TEST( test_move_transfers_ownership );
    RUN_TEST( test_set_length_resizes_span );
    std::printf( "test_packet_pool: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
