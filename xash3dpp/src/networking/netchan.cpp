// xash3dpp — Netchan implementation stub (Layer 3)
// Legacy reference: engine/common/net_chan.c
//
// All method bodies below are TODO stubs.  The Chunk plan in
// docs/architecture/networking/README.md (Layer 3 section) and the output
// of `/plan-implementation networking` will drive the order in which
// these are filled in.

#include <xash3dpp/networking/netchan.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <array>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace xash::networking {

namespace core     = ::xash::core;
namespace limits   = ::xash::limits;

// ---------------------------------------------------------------------------
// Internal fragment-queue types.  Mirrors legacy fragbuf_t / fragbufwaiting_t
// but uses owning std::vector for the payload bytes so we can free per-batch
// without manual pool walks.  TODO(pool-migration): switch the payload
// storage onto the parent NetworkContext's PoolHandle once a pool-backed
// byte-vector adapter exists.
// ---------------------------------------------------------------------------

namespace {

struct Fragbuf
{
    std::uint32_t          bufferid { 0 }; // 1-based within its batch
    std::vector<std::byte> payload;
    bool                   is_file  { false }; // file-stream marker
    std::string            filename;           // set only on the first file fragment
};

// One create_fragments() call produces exactly one FragbufBatch — the legacy
// `fragbufwaiting_t`.  Batches preserve the user's logical message grouping
// so that transmit() can mark a batch "complete" after its final fragment
// has been acknowledged.
struct FragbufBatch
{
    std::vector<Fragbuf> bufs;
};

} // namespace

// ---------------------------------------------------------------------------
// Impl — owns all mutable channel state.  Lives behind a unique_ptr so the
// public header stays free of fragment-queue, flow-stats, and pool types.
// ---------------------------------------------------------------------------

struct Netchan::Impl
{
    // Identity
    SocketKind          sock              { SocketKind::Client };
    NetAddress          remote_address    {};
    std::uint16_t       qport             { 0 };
    NetchanFlags        flags             {};
    IProtocolDriver    *driver            { nullptr };
    IBlockSizeProvider *block_size_provider { nullptr };

    // Backing pool for fragment buffers + reliable_buf.  Owned by the
    // parent NetworkContext (PoolHandle("networking")); Netchan never
    // creates or destroys it.
    xash::memory::PoolHandle pool {};

    // Sequencing
    std::uint32_t       incoming_sequence              { 0 };
    std::uint32_t       incoming_acknowledged          { 0 };
    std::uint32_t       incoming_reliable_acknowledged { 0 };
    std::uint32_t       incoming_reliable_sequence     { 0 };
    std::uint32_t       outgoing_sequence              { 1 };
    std::uint32_t       reliable_sequence              { 0 };
    std::uint32_t       last_reliable_sequence         { 0 };

    // Timing / flow
    double              last_received     { 0.0 };
    double              connect_time      { 0.0 };
    double              cleartime         { 0.0 };
    double              rate              { 0.0 }; // DEFAULT_RATE applied in setup()

    // Reliable buffer.  Sized at limits::net_max_payload; backing storage
    // will move into the parent NetworkContext's pool in Chunk 7.
    // TODO(Chunk 7): pool-allocate; currently a vector to keep the stub
    // header-includes minimal.
    std::vector<std::byte> reliable_buf;
    std::size_t            reliable_length_bits { 0 };

    // Outgoing fragment queues — one per FragStream (Normal, File).  Each
    // queue entry is a FragbufBatch (legacy fragbufwaiting_t): one logical
    // message that was sliced into fragment-sized chunks by
    // create_fragments().  transmit() drains the front batch fragment-by-
    // fragment.
    std::array<std::deque<FragbufBatch>, 2> outgoing_fragments {};

    // TODO(Chunk 8): incoming reassembly slots per stream.

    // Active flag — true between successful setup() and clear()/move-out.
    bool                active { false };

    // Optional Tier-2 atomics owned by parent NetworkContext.
    NetworkingStats    *stats  { nullptr };
};

// ---------------------------------------------------------------------------
// Construction / destruction / move
// ---------------------------------------------------------------------------

Netchan::Netchan() noexcept : impl_( std::make_unique<Impl>() ) {}
Netchan::~Netchan() = default;

Netchan::Netchan( Netchan && ) noexcept            = default;
Netchan &Netchan::operator=( Netchan && ) noexcept = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool Netchan::setup( const NetchanConfig &config ) noexcept
{
    if( !impl_ ) return false;
    if( config.driver == nullptr || config.block_size_provider == nullptr )
    {
        core::log( core::LogLevel::Error, "netchan",
                   "setup: driver and block_size_provider are required" );
        return false;
    }
    if( !config.pool.valid() )
    {
        core::log( core::LogLevel::Error, "netchan",
                   "setup: pool handle is required (must be the parent "
                   "NetworkContext's networking pool)" );
        return false;
    }

    // Wipe any prior state.
    *impl_ = Impl{};

    impl_->sock                = config.sock;
    impl_->remote_address      = config.remote_address;
    impl_->qport               = config.qport;
    impl_->flags               = config.flags;
    impl_->driver              = config.driver;
    impl_->block_size_provider = config.block_size_provider;
    impl_->pool                = config.pool;
    impl_->rate                = config.rate;
    impl_->active              = true;

    // TODO(Chunk 7): allocate reliable_buf and per-stream fragment queues
    // from the parent NetworkContext's pool.

    return true;
}

void Netchan::clear() noexcept
{
    if( !impl_ || !impl_->active ) return;

    impl_->reliable_buf.clear();
    impl_->reliable_length_bits = 0;
    impl_->cleartime            = 0.0;

    for( auto &queue : impl_->outgoing_fragments )
        queue.clear();

    // TODO(Chunk 8): zero the flow_t telemetry array.
}

bool Netchan::is_active() const noexcept
{
    return impl_ && impl_->active;
}

// ---------------------------------------------------------------------------
// Reliable / fragment queue input
// ---------------------------------------------------------------------------

bool Netchan::write_reliable( std::span<const std::byte> bytes ) noexcept
{
    if( !impl_ || !impl_->active ) return false;
    if( bytes.empty() ) return true;

    const std::size_t current = impl_->reliable_buf.size();
    if( bytes.size() > ::xash::limits::net_max_payload - current )
    {
        // Would overflow the reliable queue.  Drop and signal the caller;
        // the legacy engine treats this as a fatal condition for the
        // channel, but at this layer we only refuse the write.
        core::log( core::LogLevel::Warning, "netchan",
                   "write_reliable: reliable buffer overflow, dropping payload" );
        return false;
    }

    impl_->reliable_buf.insert( impl_->reliable_buf.end(), bytes.begin(), bytes.end() );
    impl_->reliable_length_bits = impl_->reliable_buf.size() * 8u;
    return true;
}

Result<void> Netchan::create_fragments( FragStream stream,
                                        std::span<const std::byte> payload ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    if( payload.empty() )
        return {};
    if( payload.size() > ::xash::limits::net_max_payload )
    {
        core::log( core::LogLevel::Warning, "netchan",
                   "create_fragments: payload exceeds net_max_payload" );
        return std::unexpected( NetError::Overflow );
    }

    const int chunksize_raw = impl_->block_size_provider->block_size( FragSize::Fragment );
    if( chunksize_raw <= 0 )
    {
        core::log( core::LogLevel::Error, "netchan",
                   "create_fragments: block_size_provider returned non-positive size" );
        return std::unexpected( NetError::InvalidArgument );
    }
    const std::size_t chunksize = static_cast<std::size_t>( chunksize_raw );

    FragbufBatch batch;
    const std::size_t total = payload.size();
    const std::size_t batch_count = ( total + chunksize - 1u ) / chunksize;
    batch.bufs.reserve( batch_count );

    std::uint32_t bufferid = 1; // legacy bufferid is 1-based within a batch
    for( std::size_t pos = 0; pos < total; pos += chunksize )
    {
        const std::size_t bytes = ( total - pos < chunksize ) ? total - pos : chunksize;
        Fragbuf fb;
        fb.bufferid = bufferid++;
        fb.payload.assign( payload.begin() + static_cast<std::ptrdiff_t>( pos ),
                           payload.begin() + static_cast<std::ptrdiff_t>( pos + bytes ) );
        batch.bufs.emplace_back( std::move( fb ) );
    }

    const std::size_t idx = static_cast<std::size_t>( stream );
    impl_->outgoing_fragments[ idx ].emplace_back( std::move( batch ) );
    return {};
}

Result<void> Netchan::create_file_fragments_from_buffer(
    std::string_view filename,
    std::span<const std::byte> payload ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    if( payload.empty() )
        return {};
    if( filename.empty() )
    {
        core::log( core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: empty filename" );
        return std::unexpected( NetError::InvalidArgument );
    }
    if( filename.size() >= ::xash::limits::net_max_filename )
    {
        core::log( core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: filename exceeds net_max_filename" );
        return std::unexpected( NetError::InvalidArgument );
    }
    if( payload.size() > ::xash::limits::net_max_payload )
    {
        core::log( core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: payload exceeds net_max_payload" );
        return std::unexpected( NetError::Overflow );
    }

    const int chunksize_raw = impl_->block_size_provider->block_size( FragSize::Fragment );
    if( chunksize_raw <= 0 )
    {
        core::log( core::LogLevel::Error, "netchan",
                   "create_file_fragments_from_buffer: block_size_provider returned "
                   "non-positive size" );
        return std::unexpected( NetError::InvalidArgument );
    }
    const std::size_t chunksize       = static_cast<std::size_t>( chunksize_raw );
    const std::size_t filename_header = filename.size() + 1u; // legacy MSG_WriteString writes a NUL
    if( filename_header >= chunksize )
    {
        core::log( core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: filename header consumes the "
                   "entire fragment payload" );
        return std::unexpected( NetError::InvalidArgument );
    }
    const std::size_t first_chunk_max = chunksize - filename_header;

    FragbufBatch batch;
    const std::size_t total       = payload.size();
    const std::size_t after_first = ( total > first_chunk_max ) ? total - first_chunk_max : 0u;
    const std::size_t batch_count = 1u + ( after_first + chunksize - 1u ) / chunksize;
    batch.bufs.reserve( batch_count );

    std::uint32_t bufferid = 1;
    std::size_t   pos      = 0;
    bool          first    = true;
    while( pos < total )
    {
        const std::size_t cap   = first ? first_chunk_max : chunksize;
        const std::size_t bytes = ( total - pos < cap ) ? total - pos : cap;
        Fragbuf fb;
        fb.bufferid = bufferid++;
        fb.is_file  = true;
        if( first )
            fb.filename.assign( filename );
        fb.payload.assign( payload.begin() + static_cast<std::ptrdiff_t>( pos ),
                           payload.begin() + static_cast<std::ptrdiff_t>( pos + bytes ) );
        batch.bufs.emplace_back( std::move( fb ) );
        pos += bytes;
        first = false;
    }

    impl_->outgoing_fragments[ static_cast<std::size_t>( FragStream::File ) ]
        .emplace_back( std::move( batch ) );
    return {};
}

// ---------------------------------------------------------------------------
// Transmit / process
// ---------------------------------------------------------------------------

Result<std::size_t> Netchan::transmit( std::span<const std::byte> unreliable,
                                       std::span<std::byte>       out ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    if( impl_->driver == nullptr )
        return std::unexpected( NetError::NotInitialised );

    // Wrap the caller's output buffer.  All header / reliable / unreliable
    // writes flow through MessageBuf so overflow is detected centrally.
    MessageBuf msg{ out, "netchan-transmit" };

    // Decide whether this packet carries the pending reliable payload.
    // Legacy net_chan.c only re-emits reliable bytes when the previous
    // batch has been acknowledged; until process() lands we conservatively
    // send any pending reliable on every transmit so the channel can drain.
    // TODO(Chunk 8): gate on (incoming_reliable_acknowledged != reliable_sequence).
    const bool send_reliable = impl_->reliable_length_bits > 0u;

    // TODO(Chunk 7-frag): consume the front-of-queue FragbufBatch when a
    // reliable fragment is being shipped.  For now transmit() assumes no
    // pending fragments and asserts via the queue size — callers should
    // not invoke transmit() with outstanding fragments yet.
    const bool send_reliable_fragment = false;

    PacketHeaderInput hdr_in{};
    hdr_in.outgoing_sequence          = impl_->outgoing_sequence;
    hdr_in.incoming_sequence          = impl_->incoming_sequence;
    hdr_in.incoming_reliable_sequence = impl_->incoming_reliable_sequence;
    hdr_in.qport                      = impl_->qport;
    hdr_in.send_reliable              = send_reliable;
    hdr_in.send_reliable_fragment     = send_reliable_fragment;
    hdr_in.is_client                  = impl_->sock == SocketKind::Client;

    if( auto r = impl_->driver->write_packet_header( msg, hdr_in ); !r.has_value() )
        return std::unexpected( r.error() );

    // Reliable bits — emitted at the legacy `MSG_WriteBits(reliable_buf,
    // reliable_length)` slot.  Cleared on success so the next transmit()
    // begins a fresh reliable batch.
    if( send_reliable )
    {
        std::span<const std::byte> reliable_span{ impl_->reliable_buf.data(),
                                                  impl_->reliable_buf.size() };
        if( !msg.write_bits( reliable_span, impl_->reliable_length_bits ) )
            return std::unexpected( NetError::Overflow );
    }

    // Unreliable tail — gated by the per-driver Unreliable block size.
    // Legacy: `pfnBlockSize(client, FRAGSIZE_UNRELIABLE)` returns the max
    // total datagram size in bytes; we drop the tail if appending it would
    // exceed that cap.  A non-positive block size means "no cap" (used by
    // the loopback path).
    if( !unreliable.empty() )
    {
        const int unrel_cap_raw =
            impl_->block_size_provider->block_size( FragSize::Unreliable );
        const std::size_t projected = msg.real_bytes_written() + unreliable.size();
        const bool fits = unrel_cap_raw <= 0
                       || projected <= static_cast<std::size_t>( unrel_cap_raw );
        if( fits )
        {
            if( !msg.write_bytes( unreliable ) )
                return std::unexpected( NetError::Overflow );
        }
        else
        {
            core::log( core::LogLevel::Verbose, "netchan",
                       "transmit: unreliable tail dropped, would exceed "
                       "block_size(Unreliable) cap" );
        }
    }

    if( msg.overflowed() )
        return std::unexpected( NetError::Overflow );

    // TODO(Chunk 7-frag): if !loopback and bytes_written < 16, pad with
    // clc_nop / svc_nop (layer-4 message IDs) so the legacy
    // anti-spoof-tracking heuristics hold.  Padding is deferred until the
    // layer-4 message IDs land in xash3dpp.

    const std::size_t bytes_written = msg.real_bytes_written();

    // Bump sequence + clear reliable batch on success.  last_reliable_sequence
    // remembers the outgoing_sequence in which the reliable was shipped so
    // process() can match the ack on the receive side.
    if( send_reliable )
    {
        impl_->reliable_buf.clear();
        impl_->reliable_length_bits = 0u;
        impl_->last_reliable_sequence = impl_->outgoing_sequence;
    }
    ++impl_->outgoing_sequence;

    return bytes_written;
}

Result<std::size_t> Netchan::transmit_bits( std::span<const std::byte> unreliable,
                                            std::size_t                length_in_bits,
                                            std::span<std::byte>       out ) noexcept
{
    // Byte-aligned shim: bit-granular unreliable payloads are not used by
    // any current caller, and a partial trailing byte cannot be represented
    // through write_bytes().  The legacy engine padded to the next byte
    // boundary via MSG_WriteBits(); we replicate that by rounding up.
    if( length_in_bits == 0u )
        return transmit( {}, out );

    const std::size_t bytes_needed = ( length_in_bits + 7u ) / 8u;
    if( bytes_needed > unreliable.size() )
        return std::unexpected( NetError::InvalidArgument );

    return transmit( unreliable.subspan( 0, bytes_needed ), out );
}

bool Netchan::process( std::span<const std::byte> /*datagram*/,
                       MessageBuf                 & /*msg*/ ) noexcept
{
    // TODO(Chunk 8): demux header, run optional unmunge, validate sequence,
    // handle reliable ack / fragment ingest, set msg to the post-header
    // payload span.  Returns false on stale / duplicate / malformed packets.
    return false;
}

// ---------------------------------------------------------------------------
// Receive-side fragment readiness
// ---------------------------------------------------------------------------

bool Netchan::incoming_ready() const noexcept
{
    // TODO(Chunk 8): scan per-stream incomingready flags.
    return false;
}

Result<std::size_t> Netchan::copy_normal_fragments( std::span<std::byte> /*out*/ ) noexcept
{
    // TODO(Chunk 8): copy assembled normal-stream fragments into out.
    return std::unexpected( NetError::NotInitialised );
}

Result<std::size_t> Netchan::copy_file_fragments( std::span<std::byte> /*out*/,
                                                  std::span<char>      /*filename_out*/ ) noexcept
{
    // TODO(Chunk 8): copy assembled file-stream fragments + filename out.
    return std::unexpected( NetError::NotInitialised );
}

// ---------------------------------------------------------------------------
// Bandwidth / choke
// ---------------------------------------------------------------------------

namespace {

// IP + UDP header overhead per legacy net_chan.c (UDP_HEADER_SIZE = 28).
// Counted against the per-channel rate cap when accumulating cleartime.
constexpr std::size_t k_udp_header_size = 28;

[[nodiscard]] bool is_loopback_address( const NetAddress &addr ) noexcept
{
    return addr.family == IpFamily::V4 && addr.addr.v4[0] == 127;
}

} // namespace

bool Netchan::can_packet( double now_seconds, bool choke ) const noexcept
{
    if( !impl_ || !impl_->active ) return false;

    // Never choke loopback or explicit-bypass packets.  Mutating cleartime
    // here mirrors legacy Netchan_CanPacket so the next throttled send
    // doesn't carry over backlog from a quiet period.
    if( !choke || is_loopback_address( impl_->remote_address ) )
    {
        impl_->cleartime = now_seconds;
        return true;
    }

    return impl_->cleartime < now_seconds;
}

void Netchan::update_choke( double now_seconds, std::size_t bytes_sent ) noexcept
{
    if( !impl_ || !impl_->active ) return;
    if( impl_->rate <= 0.0 ) return; // no rate cap configured

    if( impl_->cleartime < now_seconds )
        impl_->cleartime = now_seconds;

    const double seconds_per_byte = 1.0 / impl_->rate;
    impl_->cleartime += static_cast<double>( bytes_sent + k_udp_header_size )
                        * seconds_per_byte;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const NetAddress &Netchan::remote_address() const noexcept { return impl_->remote_address; }
SocketKind        Netchan::sock()           const noexcept { return impl_->sock; }
std::uint16_t     Netchan::qport()          const noexcept { return impl_->qport; }
std::uint32_t     Netchan::incoming_sequence() const noexcept { return impl_->incoming_sequence; }
std::uint32_t     Netchan::outgoing_sequence() const noexcept { return impl_->outgoing_sequence; }
double            Netchan::last_received() const noexcept { return impl_->last_received; }
double            Netchan::connect_time()  const noexcept { return impl_->connect_time; }
double            Netchan::rate()          const noexcept { return impl_->rate; }
IProtocolDriver  *Netchan::driver()        const noexcept { return impl_->driver; }
std::size_t       Netchan::reliable_length_bits() const noexcept { return impl_->reliable_length_bits; }

std::size_t Netchan::pending_fragments( FragStream stream ) const noexcept
{
    if( !impl_ ) return 0;
    const std::size_t idx = static_cast<std::size_t>( stream );
    if( idx >= impl_->outgoing_fragments.size() ) return 0;
    std::size_t total = 0;
    for( const auto &batch : impl_->outgoing_fragments[ idx ] )
        total += batch.bufs.size();
    return total;
}

void Netchan::bind_stats( NetworkingStats *stats ) noexcept { impl_->stats = stats; }
NetworkingStats *Netchan::stats() const noexcept             { return impl_->stats; }

} // namespace xash::networking
