// xash3dpp — Netchan smoke test (Layer 3 stub coverage)
// Verifies the public API surface compiles and the default-constructed +
// setup() lifecycle behaves correctly.  All real reliable/unreliable/
// fragment behaviour is // TODO(Chunk N) and will get its own tests.

#include <xash3dpp/networking/netchan.hpp>
#include <xash3dpp/private/networking/protocol_driver_default.hpp>

#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/limits.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <string>
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
    bool          sends_qport() const noexcept override { return false; }

    Result<void> write_packet_header( MessageBuf &, const PacketHeaderInput & ) noexcept override
    {
        return {};
    }

    Result<FrameMeta> read_packet_header( MessageBuf &, bool ) noexcept override
    {
        return FrameMeta{};
    }
};

struct StubBlockSize final : IBlockSizeProvider
{
    int block_size( FragSize ) noexcept override { return 0; }
};

// Returns a fixed fragment chunk size, configurable per-test.
struct FixedBlockSize final : IBlockSizeProvider
{
    int fragment_size   { 0 };
    int unreliable_size { 0 };
    int block_size( FragSize mode ) noexcept override
    {
        switch( mode )
        {
        case FragSize::Fragment:   return fragment_size;
        case FragSize::Unreliable: return unreliable_size;
        default:                   return 0;
        }
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

    // Receive-side paths: no fragments have arrived, so incoming_ready()
    // must be false and the copy_* methods must return 0 bytes (success,
    // nothing to deliver) rather than an error.
    CHECK( !c.incoming_ready() );

    std::array<std::byte, 64> out_buf{};
    std::array<char, 16>      filename_out{};
    auto r_copy_normal = c.copy_normal_fragments( out_buf );
    CHECK( r_copy_normal.has_value() );
    if( r_copy_normal.has_value() ) CHECK_EQ( static_cast<int>( *r_copy_normal ), 0 );
    auto r_copy_file = c.copy_file_fragments( out_buf, filename_out );
    CHECK( r_copy_file.has_value() );
    if( r_copy_file.has_value() ) CHECK_EQ( static_cast<int>( *r_copy_file ), 0 );

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

static void test_create_file_fragments_inactive_rejected()
{
    Netchan c;
    std::array<std::byte, 4> payload{};
    auto r = c.create_file_fragments_from_buffer( "x.txt", payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::NotInitialised );
}

static void test_create_file_fragments_empty_payload_is_noop()
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
    auto r = c.create_file_fragments_from_buffer( "x.txt", empty );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::File ) ), 0 );
}

static void test_create_file_fragments_rejects_empty_filename()
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

    std::array<std::byte, 4> payload{};
    auto r = c.create_file_fragments_from_buffer( std::string_view{}, payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::InvalidArgument );
}

static void test_create_file_fragments_rejects_oversize_filename()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 4096;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    std::string too_long( xash::limits::net_max_filename, 'a' );
    std::array<std::byte, 4> payload{};
    auto r = c.create_file_fragments_from_buffer( too_long, payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::InvalidArgument );
}

static void test_create_file_fragments_rejects_filename_filling_chunk()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    // chunk so small the filename header alone fills it.
    bs.fragment_size = 8;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    std::array<std::byte, 4> payload{};
    // "abcdefgh" + NUL = 9 bytes > chunksize of 8.
    auto r = c.create_file_fragments_from_buffer( "abcdefgh", payload );
    CHECK( !r.has_value() );
    CHECK( r.error() == NetError::InvalidArgument );
}

static void test_create_file_fragments_splits_with_filename_header()
{
    Netchan c;
    StubDriver d;
    FixedBlockSize bs;
    bs.fragment_size = 16;
    ScopedPool pool;

    NetchanConfig cfg;
    cfg.driver               = &d;
    cfg.block_size_provider  = &bs;
    cfg.pool                 = pool.handle;
    REQUIRE( c.setup( cfg ) );

    // filename "f.bin" -> header = 6 bytes (5 + NUL)
    // first chunk payload cap = 16 - 6 = 10
    // payload = 32 bytes  ->  10 + 16 + 6  ->  3 fragments
    std::vector<std::byte> payload( 32u, std::byte{ 0xAB } );
    auto r = c.create_file_fragments_from_buffer( "f.bin", payload );
    CHECK( r.has_value() );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::File ) ), 3 );
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::Normal ) ), 0 );

    c.clear();
    CHECK_EQ( static_cast<int>( c.pending_fragments( FragStream::File ) ), 0 );
}

// ---------- transmit / transmit_bits --------------------------------------

namespace {

// Build a Netchan wired up to the real GoldSrc protocol driver so the
// transmit() wire layout can be inspected directly.
struct GoldSrcHarness
{
    Netchan        chan;
    FixedBlockSize bs;
    ScopedPool     pool;

    GoldSrcHarness() noexcept { bs.unreliable_size = 1024; }

    bool setup_client() noexcept
    {
        NetchanConfig cfg;
        cfg.sock                = SocketKind::Client;
        cfg.driver              = default_protocol_driver_registry().resolve( 48 );
        cfg.block_size_provider = &bs;
        cfg.pool                = pool.handle;
        return chan.setup( cfg );
    }
};

[[nodiscard]] std::uint32_t le_u32( std::span<const std::byte> b ) noexcept
{
    return  static_cast<std::uint32_t>( b[ 0 ] )
         | ( static_cast<std::uint32_t>( b[ 1 ] ) <<  8 )
         | ( static_cast<std::uint32_t>( b[ 2 ] ) << 16 )
         | ( static_cast<std::uint32_t>( b[ 3 ] ) << 24 );
}

} // namespace

static void test_transmit_inactive_returns_not_initialised()
{
    Netchan c;
    std::array<std::byte, 32> out{};
    auto r = c.transmit( {}, out );
    CHECK( !r.has_value() );
    if( !r.has_value() ) CHECK( r.error() == NetError::NotInitialised );
}

static void test_transmit_emits_eight_byte_header_when_idle()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    const std::uint32_t pre_seq = h.chan.outgoing_sequence();

    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit( {}, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 8 );

    const std::uint32_t w1 = le_u32( std::span<const std::byte>{ out.data(), 4 } );
    const std::uint32_t w2 = le_u32( std::span<const std::byte>{ out.data() + 4, 4 } );
    CHECK( ( w1 & 0x80000000u ) == 0u );
    CHECK( ( w1 & ~0xC0000000u ) == pre_seq );
    CHECK( w2 == 0u );

    CHECK_EQ( static_cast<int>( h.chan.outgoing_sequence() ),
              static_cast<int>( pre_seq + 1u ) );
}

static void test_transmit_appends_unreliable_payload()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    const std::array<std::byte, 4> tail{
        std::byte{ 0xDE }, std::byte{ 0xAD }, std::byte{ 0xBE }, std::byte{ 0xEF } };
    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit( tail, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 12 );
    CHECK( out[  8 ] == std::byte{ 0xDE } );
    CHECK( out[  9 ] == std::byte{ 0xAD } );
    CHECK( out[ 10 ] == std::byte{ 0xBE } );
    CHECK( out[ 11 ] == std::byte{ 0xEF } );
}

static void test_transmit_drops_unreliable_when_exceeds_cap()
{
    GoldSrcHarness h;
    h.bs.unreliable_size = 10; // 8 header + only 2 tail bytes fit
    REQUIRE( h.setup_client() );

    const std::array<std::byte, 8> tail{};
    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit( tail, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 8 ); // header only
}

static void test_transmit_with_reliable_sets_bit_and_clears_buf()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    const std::array<std::byte, 3> rel{
        std::byte{ 0x11 }, std::byte{ 0x22 }, std::byte{ 0x33 } };
    REQUIRE( h.chan.write_reliable( rel ) );
    CHECK_EQ( static_cast<int>( h.chan.reliable_length_bits() ), 24 );

    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit( {}, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 11 ); // 8 header + 3 reliable

    const std::uint32_t w1 = le_u32( std::span<const std::byte>{ out.data(), 4 } );
    CHECK( ( w1 & 0x80000000u ) != 0u );
    CHECK( out[  8 ] == std::byte{ 0x11 } );
    CHECK( out[  9 ] == std::byte{ 0x22 } );
    CHECK( out[ 10 ] == std::byte{ 0x33 } );

    // Reliable batch drained after transmit.
    CHECK_EQ( static_cast<int>( h.chan.reliable_length_bits() ), 0 );
}

static void test_transmit_overflows_on_tiny_buffer()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 4> tiny{};
    auto r = h.chan.transmit( {}, tiny );
    CHECK( !r.has_value() );
    if( !r.has_value() )
        CHECK( r.error() == NetError::Overflow );
}

static void test_transmit_bits_rounds_up_to_byte_boundary()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    const std::array<std::byte, 4> tail{
        std::byte{ 0xAA }, std::byte{ 0x05 }, std::byte{ 0xCC }, std::byte{ 0xDD } };
    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit_bits( tail, /*length_in_bits=*/11u, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 10 ); // 8 header + 2 tail bytes (11 bits → 2 bytes)
    CHECK( out[ 8 ] == std::byte{ 0xAA } );
    CHECK( out[ 9 ] == std::byte{ 0x05 } );
}

static void test_transmit_bits_zero_length_emits_header_only()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit_bits( {}, 0u, out );
    REQUIRE( r.has_value() );
    CHECK_EQ( static_cast<int>( *r ), 8 );
}

static void test_transmit_bits_rejects_too_few_bytes()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 2> tail{};
    std::array<std::byte, 64> out{};
    auto r = h.chan.transmit_bits( tail, /*length_in_bits=*/17u, out );
    CHECK( !r.has_value() );
    if( !r.has_value() )
        CHECK( r.error() == NetError::InvalidArgument );
}

// ---------- process -------------------------------------------------------

namespace {

// Build a synthetic GoldSrc datagram so process() has something to chew on.
struct CraftedHeader
{
    std::uint32_t w1 { 0 };
    std::uint32_t w2 { 0 };
};

void emit_le32( std::span<std::byte> dst, std::uint32_t v ) noexcept
{
    dst[ 0 ] = static_cast<std::byte>(   v        & 0xFFu );
    dst[ 1 ] = static_cast<std::byte>( ( v >>  8 ) & 0xFFu );
    dst[ 2 ] = static_cast<std::byte>( ( v >> 16 ) & 0xFFu );
    dst[ 3 ] = static_cast<std::byte>( ( v >> 24 ) & 0xFFu );
}

void craft_datagram( std::span<std::byte> out, const CraftedHeader &h ) noexcept
{
    emit_le32( out.subspan( 0, 4 ), h.w1 );
    emit_le32( out.subspan( 4, 4 ), h.w2 );
}

} // namespace

static void test_process_inactive_returns_false()
{
    Netchan c;
    std::array<std::byte, 8> dg{};
    MessageBuf m;
    CHECK( !c.process( dg, m ) );
}

static void test_process_empty_datagram_returns_false()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );
    MessageBuf m;
    CHECK( !h.chan.process( {}, m ) );
}

static void test_process_truncated_header_returns_false()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 4> tiny{}; // only 4 of 8 header bytes
    MessageBuf m;
    CHECK( !h.chan.process( tiny, m ) );
}

static void test_process_accepts_valid_header_and_updates_state()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 16> dg{};
    craft_datagram( dg, CraftedHeader{ /*w1=*/42u, /*w2=*/0u } );
    // 8 bytes of payload after the header.
    for( std::size_t i = 0; i < 8; ++i )
        dg[ 8u + i ] = static_cast<std::byte>( 0x80u + i );

    MessageBuf m;
    REQUIRE( h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_sequence() ),     42 );
    CHECK_EQ( static_cast<int>( h.chan.incoming_acknowledged() ), 0  );

    // Read cursor sits at byte 8 — the next read_byte() must yield 0x80.
    const auto first = m.read_byte();
    CHECK_EQ( static_cast<int>( first ), 0x80 );
}

static void test_process_drops_stale_sequence()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    std::array<std::byte, 8> dg{};
    craft_datagram( dg, CraftedHeader{ /*w1=*/100u, /*w2=*/0u } );
    MessageBuf m;
    REQUIRE( h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_sequence() ), 100 );

    // Older sequence — must be dropped, incoming_sequence unchanged.
    craft_datagram( dg, CraftedHeader{ /*w1=*/50u, /*w2=*/0u } );
    CHECK( !h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_sequence() ), 100 );

    // Same sequence — also dropped.
    craft_datagram( dg, CraftedHeader{ /*w1=*/100u, /*w2=*/0u } );
    CHECK( !h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_sequence() ), 100 );
}

static void test_process_tracks_reliable_ack_bit()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    // w2 high bit asserts: sender has acknowledged our reliable payload.
    std::array<std::byte, 8> dg{};
    craft_datagram( dg, CraftedHeader{
        /*w1=*/7u,
        /*w2=*/( 9u | 0x80000000u ) } );
    MessageBuf m;
    REQUIRE( h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_acknowledged() ), 9 );
    CHECK_EQ( static_cast<int>( h.chan.incoming_reliable_acknowledged() ), 1 );
}

static void test_process_flips_incoming_reliable_sequence_on_reliable_bit()
{
    GoldSrcHarness h;
    REQUIRE( h.setup_client() );

    CHECK_EQ( static_cast<int>( h.chan.incoming_reliable_sequence() ), 0 );

    std::array<std::byte, 8> dg{};
    craft_datagram( dg, CraftedHeader{
        /*w1=*/( 1u | 0x80000000u ),  // sender is shipping reliable bytes
        /*w2=*/0u } );
    MessageBuf m;
    REQUIRE( h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_reliable_sequence() ), 1 );

    // Second reliable packet — parity flips back to 0.
    craft_datagram( dg, CraftedHeader{
        /*w1=*/( 2u | 0x80000000u ),
        /*w2=*/0u } );
    REQUIRE( h.chan.process( dg, m ) );
    CHECK_EQ( static_cast<int>( h.chan.incoming_reliable_sequence() ), 0 );
}

// ---------- fragment round-trip ------------------------------------------

namespace {

// Build a sender + receiver pair sharing the same GoldSrc driver so the
// wire bytes a sender's transmit() emits flow back into a receiver's
// process() unchanged.  Both sides use the same chunk size so the sender's
// fragment slicing decisions are independent of the test's choice.
struct FragHarness
{
    GoldSrcHarness sender;
    GoldSrcHarness receiver;

    FragHarness() noexcept
    {
        sender.bs.fragment_size     = 16;
        sender.bs.unreliable_size   = 1024;
        receiver.bs.fragment_size   = 16;
        receiver.bs.unreliable_size = 1024;
    }

    [[nodiscard]] bool setup_pair() noexcept
    {
        return sender.setup_client() && receiver.setup_client();
    }

    // Drain one packet from sender -> receiver.  Returns the packet size.
    [[nodiscard]] std::size_t pump_one( std::array<std::byte, 256> &buf ) noexcept
    {
        auto r = sender.chan.transmit( {}, buf );
        if( !r.has_value() ) return 0u;
        MessageBuf m;
        const bool ok = receiver.chan.process(
            std::span<const std::byte>{ buf.data(), *r }, m );
        return ok ? *r : 0u;
    }
};

} // namespace

static void test_fragment_round_trip_normal_stream()
{
    FragHarness h;
    REQUIRE( h.setup_pair() );

    // Build a 40-byte payload that slices into 16/16/8 fragment pieces.
    std::vector<std::byte> payload( 40u );
    for( std::size_t i = 0; i < payload.size(); ++i )
        payload[ i ] = static_cast<std::byte>( 0xA0u + ( i & 0x1Fu ) );

    auto cf = h.sender.chan.create_fragments( FragStream::Normal, payload );
    REQUIRE( cf.has_value() );
    CHECK_EQ( static_cast<int>( h.sender.chan.pending_fragments( FragStream::Normal ) ), 3 );

    std::array<std::byte, 256> buf{};
    // Pump three packets (one per fragment).
    REQUIRE( h.pump_one( buf ) > 0u );
    CHECK( !h.receiver.chan.incoming_ready() );
    REQUIRE( h.pump_one( buf ) > 0u );
    CHECK( !h.receiver.chan.incoming_ready() );
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.receiver.chan.incoming_ready() );

    // Sender side: all fragments drained.
    CHECK_EQ( static_cast<int>( h.sender.chan.pending_fragments( FragStream::Normal ) ), 0 );

    std::array<std::byte, 128> out{};
    auto cn = h.receiver.chan.copy_normal_fragments( out );
    REQUIRE( cn.has_value() );
    CHECK_EQ( static_cast<int>( *cn ), static_cast<int>( payload.size() ) );
    for( std::size_t i = 0; i < payload.size(); ++i )
        CHECK( out[ i ] == payload[ i ] );

    // copy_* must reset the slot so a second call returns 0 bytes ready.
    CHECK( !h.receiver.chan.incoming_ready() );
    auto cn2 = h.receiver.chan.copy_normal_fragments( out );
    REQUIRE( cn2.has_value() );
    CHECK_EQ( static_cast<int>( *cn2 ), 0 );
}

static void test_fragment_round_trip_file_stream()
{
    FragHarness h;
    REQUIRE( h.setup_pair() );

    // Filename header consumes 9 bytes ("model.mdl" + NUL) of the first
    // fragment's chunk capacity, leaving 7 bytes for data; subsequent
    // fragments carry up to 16 bytes each.
    std::vector<std::byte> payload( 30u );
    for( std::size_t i = 0; i < payload.size(); ++i )
        payload[ i ] = static_cast<std::byte>( 0x10u + i );

    auto cf = h.sender.chan.create_file_fragments_from_buffer( "model.mdl", payload );
    REQUIRE( cf.has_value() );
    // 30 bytes payload, 7 in first fragment, 16+7 in second/third
    // -> 3 fragments total.
    CHECK_EQ( static_cast<int>( h.sender.chan.pending_fragments( FragStream::File ) ), 3 );

    std::array<std::byte, 256> buf{};
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.receiver.chan.incoming_ready() );

    std::array<std::byte, 128> out{};
    std::array<char, 32>       name{};
    auto cf2 = h.receiver.chan.copy_file_fragments( out, name );
    REQUIRE( cf2.has_value() );
    CHECK_EQ( static_cast<int>( *cf2 ), static_cast<int>( payload.size() ) );
    CHECK( std::string_view{ name.data() } == std::string_view{ "model.mdl" } );
    for( std::size_t i = 0; i < payload.size(); ++i )
        CHECK( out[ i ] == payload[ i ] );
}

static void test_fragment_descriptor_sets_w1_bit_30()
{
    FragHarness h;
    REQUIRE( h.setup_pair() );

    const std::array<std::byte, 8> payload{
        std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 }, std::byte{ 4 },
        std::byte{ 5 }, std::byte{ 6 }, std::byte{ 7 }, std::byte{ 8 } };
    auto cf = h.sender.chan.create_fragments( FragStream::Normal, payload );
    REQUIRE( cf.has_value() );

    std::array<std::byte, 256> buf{};
    auto r = h.sender.chan.transmit( {}, buf );
    REQUIRE( r.has_value() );

    const std::uint32_t w1 = le_u32( std::span<const std::byte>{ buf.data(), 4 } );
    CHECK( ( w1 & 0x80000000u ) != 0u ); // reliable
    CHECK( ( w1 & 0x40000000u ) != 0u ); // reliable-fragment
}

static void test_fragment_round_trip_resets_offset_between_batches()
{
    FragHarness h;
    REQUIRE( h.setup_pair() );

    // Queue two back-to-back batches on the Normal stream.  After the
    // first drains, byte_offset must reset to 0 for the second.
    const std::array<std::byte, 8> a{
        std::byte{ 0xA0 }, std::byte{ 0xA1 }, std::byte{ 0xA2 }, std::byte{ 0xA3 },
        std::byte{ 0xA4 }, std::byte{ 0xA5 }, std::byte{ 0xA6 }, std::byte{ 0xA7 } };
    const std::array<std::byte, 8> b{
        std::byte{ 0xB0 }, std::byte{ 0xB1 }, std::byte{ 0xB2 }, std::byte{ 0xB3 },
        std::byte{ 0xB4 }, std::byte{ 0xB5 }, std::byte{ 0xB6 }, std::byte{ 0xB7 } };
    REQUIRE( h.sender.chan.create_fragments( FragStream::Normal, a ).has_value() );
    REQUIRE( h.sender.chan.create_fragments( FragStream::Normal, b ).has_value() );

    std::array<std::byte, 256> buf{};
    // Drain batch a (single fragment, fits in chunk size 16).
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.receiver.chan.incoming_ready() );
    std::array<std::byte, 64> out{};
    auto c1 = h.receiver.chan.copy_normal_fragments( out );
    REQUIRE( c1.has_value() );
    CHECK_EQ( static_cast<int>( *c1 ), 8 );
    for( std::size_t i = 0; i < a.size(); ++i ) CHECK( out[ i ] == a[ i ] );

    // Drain batch b — the second fragment's byte_offset on the wire is 0.
    REQUIRE( h.pump_one( buf ) > 0u );
    REQUIRE( h.receiver.chan.incoming_ready() );
    out.fill( std::byte{} );
    auto c2 = h.receiver.chan.copy_normal_fragments( out );
    REQUIRE( c2.has_value() );
    CHECK_EQ( static_cast<int>( *c2 ), 8 );
    for( std::size_t i = 0; i < b.size(); ++i ) CHECK( out[ i ] == b[ i ] );
}

static void test_copy_file_fragments_rejects_path_traversal()
{
    // Craft a malicious assembled buffer directly through the round-trip
    // path: a filename containing "../" must be rejected before any data
    // is copied to the caller.  We do this by sending a hand-built batch
    // whose filename is "../etc/passwd".
    FragHarness h;
    REQUIRE( h.setup_pair() );

    const std::array<std::byte, 4> payload{
        std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 }, std::byte{ 4 } };
    auto cf = h.sender.chan.create_file_fragments_from_buffer( "../etc/passwd", payload );
    REQUIRE( cf.has_value() );

    std::array<std::byte, 256> buf{};
    while( h.sender.chan.pending_fragments( FragStream::File ) > 0u )
        REQUIRE( h.pump_one( buf ) > 0u );

    REQUIRE( h.receiver.chan.incoming_ready() );
    std::array<std::byte, 64> out{};
    std::array<char, 32>       name{};
    auto cf2 = h.receiver.chan.copy_file_fragments( out, name );
    CHECK( !cf2.has_value() );
    if( !cf2.has_value() ) CHECK( cf2.error() == NetError::InvalidArgument );
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
    test_create_file_fragments_inactive_rejected();
    test_create_file_fragments_empty_payload_is_noop();
    test_create_file_fragments_rejects_empty_filename();
    test_create_file_fragments_rejects_oversize_filename();
    test_create_file_fragments_rejects_filename_filling_chunk();
    test_create_file_fragments_splits_with_filename_header();
    test_transmit_inactive_returns_not_initialised();
    test_transmit_emits_eight_byte_header_when_idle();
    test_transmit_appends_unreliable_payload();
    test_transmit_drops_unreliable_when_exceeds_cap();
    test_transmit_with_reliable_sets_bit_and_clears_buf();
    test_transmit_overflows_on_tiny_buffer();
    test_transmit_bits_rounds_up_to_byte_boundary();
    test_transmit_bits_zero_length_emits_header_only();
    test_transmit_bits_rejects_too_few_bytes();
    test_process_inactive_returns_false();
    test_process_empty_datagram_returns_false();
    test_process_truncated_header_returns_false();
    test_process_accepts_valid_header_and_updates_state();
    test_process_drops_stale_sequence();
    test_process_tracks_reliable_ack_bit();
    test_process_flips_incoming_reliable_sequence_on_reliable_bit();
    test_fragment_round_trip_normal_stream();
    test_fragment_round_trip_file_stream();
    test_fragment_descriptor_sets_w1_bit_30();
    test_fragment_round_trip_resets_offset_between_batches();
    test_copy_file_fragments_rejects_path_traversal();
    test_stats_binding();

    std::printf( "test_netchan: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
