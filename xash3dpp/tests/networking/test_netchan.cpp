// xash3dpp — Netchan smoke test (Layer 3 stub coverage)
// Verifies the public API surface compiles and the default-constructed +
// setup() lifecycle behaves correctly.  All real reliable/unreliable/
// fragment behaviour is // TODO(Chunk N) and will get its own tests.

#include <xash3dpp/networking/netchan.hpp>

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/limits.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <vector>

static int g_pass = 0, g_fail = 0;

using namespace xash::networking;

namespace {

// RAII wrapper so each test can request a fresh pool and have it freed at
// scope exit, mirroring the parent NetworkContext lifecycle.
struct ScopedPool
{
    xash::memory::PoolHandle handle = xash::memory::create_pool( "test_netchan" );

    ScopedPool() noexcept = default;
    ~ScopedPool() { xash::memory::destroy_pool( handle ); }
    ScopedPool( const ScopedPool & )            = delete;
    ScopedPool &operator=( const ScopedPool & ) = delete;
};

// Minimal stub implementations so we can drive setup() through a happy path.

struct StubDriver final : IProtocolDriver
{
    const char   *name()        const noexcept override { return "Stub"; }
    SplitFormat   split_format() const noexcept override { return SplitFormat::Xash; }
    DeltaTableSet delta_tables() const noexcept override { return DeltaTableSet::Xash; }
};

struct StubBlockSize final : IBlockSizeProvider
{
    int block_size( FragSize ) noexcept override { return 0; }
};

// Returns a fixed fragment chunk size, configurable per-test.
struct FixedBlockSize final : IBlockSizeProvider
{
    int fragment_size { 0 };
    int block_size( FragSize mode ) noexcept override
    {
        return mode == FragSize::Fragment ? fragment_size : 0;
    }
};

} // namespace

static void test_default_construction_is_inactive()
{
    Netchan c;
    CHECK( !c.is_active() );
}

static void test_setup_requires_driver()
{
    Netchan c;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = nullptr;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    CHECK( !c.setup( cfg ) );
    CHECK( !c.is_active() );
}

static void test_setup_requires_block_size_provider()
{
    Netchan c;
    StubDriver d;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = nullptr;
    cfg.pool                 = pool.handle;
    CHECK( !c.setup( cfg ) );
    CHECK( !c.is_active() );
}

static void test_setup_requires_pool()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    // cfg.pool intentionally left as k_null_pool.
    CHECK( !c.setup( cfg ) );
    CHECK( !c.is_active() );
}

static void test_successful_setup_arms_channel()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.sock                 = SocketKind::Server;
    cfg.qport                = 0x1234;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;

    CHECK( c.setup( cfg ) );
    CHECK( c.is_active() );
    CHECK( c.sock() == SocketKind::Server );
    CHECK_EQ( static_cast<int>( c.qport() ), 0x1234 );
    CHECK( c.driver() == &d );

    // Outgoing sequence starts at 1, incoming at 0 (legacy invariant).
    CHECK_EQ( static_cast<int>( c.outgoing_sequence() ), 1 );
    CHECK_EQ( static_cast<int>( c.incoming_sequence() ), 0 );
}

static void test_clear_preserves_identity()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.qport                = 7;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    c.clear();

    // clear() does not deactivate; identity is retained.
    CHECK( c.is_active() );
    CHECK_EQ( static_cast<int>( c.qport() ), 7 );
    CHECK( c.driver() == &d );
}

static void test_stub_methods_return_not_initialised()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    std::array<std::byte, 64> out{};
    std::array<std::byte, 1>  payload{ std::byte{ 0 } };

    // All transmission paths are stubs returning NetError::NotInitialised
    // until Chunk 7+ fills them in.
    auto r1 = c.transmit( payload, out );
    CHECK( !r1.has_value() );
    CHECK( r1.error() == NetError::NotInitialised );

    auto r2 = c.create_fragments( FragStream::Normal, payload );
    CHECK( !r2.has_value() );

    // Receive side: not ready until process() ingests fragments.
    CHECK( !c.incoming_ready() );

    // can_packet() now takes (now_seconds, choke); fresh channel with
    // cleartime=0 must allow a send at any positive time.
    CHECK( c.can_packet( 1.0, true ) );
}

static void test_write_reliable_inactive_returns_false()
{
    Netchan c;
    std::array<std::byte, 4> bytes{};
    CHECK( !c.write_reliable( bytes ) );
}

static void test_write_reliable_appends_and_tracks_length()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ), 0 );

    // Empty write is a no-op success.
    std::array<std::byte, 0> empty{};
    CHECK( c.write_reliable( empty ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ), 0 );

    std::array<std::byte, 4> a{ std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4} };
    CHECK( c.write_reliable( a ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ), 32 );

    // Appends accumulate.
    std::array<std::byte, 3> b{ std::byte{5}, std::byte{6}, std::byte{7} };
    CHECK( c.write_reliable( b ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ), 56 );

    // clear() resets the reliable length.
    c.clear();
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ), 0 );
}

static void test_write_reliable_rejects_overflow()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    // Fill the reliable queue close to its cap and confirm overflow is
    // rejected without disturbing the accumulated payload.
    constexpr std::size_t cap = xash::limits::net_max_payload;

    std::vector<std::byte> big( cap - 4u, std::byte{ 0xAB } );
    CHECK( c.write_reliable( big ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ),
              static_cast<int>( ( cap - 4u ) * 8u ) );

    // 5-byte append would push us over the cap by one byte → refused.
    std::array<std::byte, 5> overflow{};
    CHECK( !c.write_reliable( overflow ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ),
              static_cast<int>( ( cap - 4u ) * 8u ) );

    // Exactly-fits write still succeeds.
    std::array<std::byte, 4> just_fits{};
    CHECK( c.write_reliable( just_fits ) );
    CHECK_EQ( static_cast<int>( c.reliable_length_bits() ),
              static_cast<int>( cap * 8u ) );
}

static void test_can_packet_inactive_returns_false()
{
    Netchan c;
    CHECK( !c.can_packet( 0.0, true ) );
    CHECK( !c.can_packet( 0.0, false ) );
}

static void test_can_packet_bypasses_choke_for_loopback_and_oob()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    cfg.remote_address       = NetAddress::loopback_v4( 27015 );
    cfg.rate                 = 100.0; // small rate so choke would otherwise matter
    REQUIRE( c.setup( cfg ) );

    // Burn the choke artificially with a big send.
    c.update_choke( 0.0, 10000 );

    // Loopback always allowed regardless of choke state.
    CHECK( c.can_packet( 0.5, true ) );

    // choke=false bypasses too, even for non-loopback peers.
    NetchanConfig cfg2 = cfg;
    cfg2.remote_address      = NetAddress::any_v4( 27015 );
    cfg2.remote_address.addr.v4[0] = 10; // pretend public address
    Netchan c2;
    REQUIRE( c2.setup( cfg2 ) );
    c2.update_choke( 0.0, 10000 );
    CHECK( c2.can_packet( 0.5, false ) );
}

static void test_update_choke_advances_cleartime()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    cfg.remote_address       = NetAddress::any_v4( 27015 );
    cfg.remote_address.addr.v4[0] = 10; // non-loopback
    cfg.rate                 = 1000.0; // 1000 bytes/sec → 1ms per byte
    REQUIRE( c.setup( cfg ) );

    // Send 100 bytes at t=1.0.  cleartime should advance to
    // 1.0 + (100 + 28) / 1000 = 1.128.
    c.update_choke( 1.0, 100 );

    // Just before the cleartime cap, choke blocks.
    CHECK( !c.can_packet( 1.05, true ) );

    // After the cleartime cap, choke clears.
    CHECK( c.can_packet( 1.20, true ) );
}

static void test_update_choke_no_rate_is_noop()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    cfg.remote_address       = NetAddress::any_v4( 27015 );
    cfg.remote_address.addr.v4[0] = 10;
    cfg.rate                 = 0.0; // disabled
    REQUIRE( c.setup( cfg ) );

    c.update_choke( 1.0, 100000 );
    // No rate ⇒ cleartime never advances, so choke always passes.
    CHECK( c.can_packet( 1.05, true ) );
}

static void test_create_fragments_inactive_rejected()
{
    Netchan c;
    std::array<std::byte, 4> payload{};
    auto r = c.create_fragments( FragStream::Normal, payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::NotInitialised );
}

static void test_create_fragments_empty_is_noop()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 256;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    std::array<std::byte, 0> empty{};
    auto r = c.create_fragments( FragStream::Normal, empty );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 0 );
}

static void test_create_fragments_rejects_bad_chunk_size()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 0; // provider misbehaves
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    std::array<std::byte, 4> payload{};
    auto r = c.create_fragments( FragStream::Normal, payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::InvalidArgument );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 0 );
}

static void test_create_fragments_splits_evenly()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 100;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    // 300 bytes / 100 per fragment = exactly 3 fragments.
    std::vector<std::byte> payload( 300u, std::byte{ 0x7E } );
    auto r = c.create_fragments( FragStream::Normal, payload );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 3 );

    // File stream is independent.
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::File ) ), 0 );
}

static void test_create_fragments_splits_with_remainder()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 100;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    // 250 bytes → 3 fragments (100, 100, 50).
    std::vector<std::byte> payload( 250u, std::byte{ 0x42 } );
    auto r = c.create_fragments( FragStream::Normal, payload );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 3 );

    // A second batch on the same stream accumulates.
    std::vector<std::byte> payload2( 50u, std::byte{ 0x00 } );
    auto r2 = c.create_fragments( FragStream::Normal, payload2 );
    CHECK( r2.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 4 );

    // clear() drains both queues.
    c.clear();
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 0 );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::File ) ), 0 );
}

static void test_create_fragments_rejects_oversize_payload()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 1024;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    // One byte over net_max_payload should be refused as Overflow.
    std::vector<std::byte> huge( xash::limits::net_max_payload + 1u, std::byte{ 0xCC } );
    auto r = c.create_fragments( FragStream::Normal, huge );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::Overflow );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 0 );
}

static void test_stats_binding()
{
    Netchan c;
    StubDriver d;
    StubBlockSize bs;
    ScopedPool pool;
    NetworkingStats stats;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    CHECK( c.stats() == nullptr );
    c.bind_stats( &stats );
    CHECK( c.stats() == &stats );
}

int main()
{
    test_default_construction_is_inactive();
    test_setup_requires_driver();
    test_setup_requires_block_size_provider();
    test_setup_requires_pool();
    test_successful_setup_arms_channel();
    test_clear_preserves_identity();
    test_stub_methods_return_not_initialised();
    test_write_reliable_inactive_returns_false();
    test_write_reliable_appends_and_tracks_length();
    test_write_reliable_rejects_overflow();
    test_can_packet_inactive_returns_false();
    test_can_packet_bypasses_choke_for_loopback_and_oob();
    test_update_choke_advances_cleartime();
    test_update_choke_no_rate_is_noop();
    test_create_fragments_inactive_rejected();
    test_create_fragments_empty_is_noop();
    test_create_fragments_rejects_bad_chunk_size();
    test_create_fragments_splits_evenly();
    test_create_fragments_splits_with_remainder();
    test_create_fragments_rejects_oversize_payload();
    test_stats_binding();

    std::printf( "test_netchan: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
