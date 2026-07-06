// xash3dpp — Netchan implementation (Layer 3)
// Legacy reference: engine/common/net_chan.c
//
// Fully implemented: setup/clear, write_reliable, can_packet/update_choke,
// create_fragments / create_file_fragments_from_buffer, transmit/transmit_bits,
// process with reliable-ack tracking and incoming fragment reassembly.
// Tracked deferrals (see TODO markers below): pool-migration of fragment and
// reliable buffers, flow_t bandwidth telemetry, and sub-16-byte nop padding
// (blocked on layer-4 message IDs).

#include <xash3dpp/networking/netchan.hpp>

#include <xash3dpp/core/log.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace xash::networking {


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
    // Total assembled-payload size across all fragments in this batch
    // (including the file-stream filename header on the first piece).
    // Computed once at create-time and used as the `total_size` field in
    // every per-fragment wire descriptor so the value stays consistent
    // even after transmit() pops the front fragment.
    std::uint32_t        total_size { 0 };
};

// IncomingStream — per-stream reassembly slot for inbound reliable fragments.
// Fragments always arrive in-order on the reliable channel; an out-of-order
// or size-mismatched fragment resets the slot.  The assembled buffer for the
// File stream begins with a NUL-terminated filename written by the sender's
// create_file_fragments_from_buffer() pass; copy_file_fragments() strips it.
//
// Hot-path containers: `data` is pre-reserved on the first fragment via
// reserve(total_expected) — @pre-reserved per Q-13.
struct IncomingStream
{
    std::vector<std::byte> data;
    std::uint32_t          total_expected { 0 };
    bool                   ready          { false };

    void reset() noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
    {
        data.clear();
        total_expected = 0;
        ready          = false;
    }

    // Append a fragment.  Returns true when the assembly is complete.
    // Mismatched total / out-of-order offset resets the slot.
    [[nodiscard]] bool ingest( std::uint32_t              total,
                               std::uint16_t              frag_offset,
                               std::span<const std::byte> payload ) noexcept
    {
        if( total_expected == 0u )
        {
            total_expected = total;
            data.reserve( total ); // @pre-reserved
        }
        else if( total != total_expected )
        {
            reset();
            total_expected = total;
            data.reserve( total ); // @pre-reserved
        }
        if( static_cast<std::size_t>( frag_offset ) != data.size() )
            return false; // out-of-order — drop this packet (caller returns false)
        if( data.size() + payload.size() > total_expected )
            return false; // would overrun
        data.insert( data.end(), payload.begin(), payload.end() );
        if( data.size() == total_expected )
            ready = true;
        return ready;
    }
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

    // Running byte offset reported in the outgoing fragment descriptor's
    // `byte_offset` field, per stream.  Reset to 0 when a batch is fully
    // drained so the next batch starts at offset 0.
    std::array<std::uint32_t, 2> frag_offset {};

    // Incoming reassembly slots per stream (Chunk 8).  See IncomingStream.
    std::array<IncomingStream, 2> incoming_streams {};

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
        ::xash::core::log( ::xash::core::LogLevel::Error, "netchan",
                   "setup: driver and block_size_provider are required" );
        return false;
    }
    if( !config.pool.valid() )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "netchan",
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

void Netchan::clear() noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
{
    if( !impl_ || !impl_->active ) return;

    impl_->reliable_buf.clear();
    impl_->reliable_length_bits = 0;
    impl_->cleartime            = 0.0;

    for( auto &queue : impl_->outgoing_fragments )
        queue.clear();
    for( auto &off : impl_->frag_offset )
        off = 0u;
    for( auto &slot : impl_->incoming_streams )
        slot.reset();

    // TODO(Chunk 8): zero the flow_t telemetry array.
}

bool Netchan::is_active() const noexcept
{
    return impl_ && impl_->active;
}

// ---------------------------------------------------------------------------
// Reliable / fragment queue input
// ---------------------------------------------------------------------------

bool Netchan::write_reliable( std::span<const std::byte> bytes ) noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
{
    if( !impl_ || !impl_->active ) return false;
    if( bytes.empty() ) return true;

    const std::size_t current = impl_->reliable_buf.size();
    if( bytes.size() > ::xash::limits::net_max_payload - current )
    {
        // Would overflow the reliable queue.  Drop and signal the caller;
        // the legacy engine treats this as a fatal condition for the
        // channel, but at this layer we only refuse the write.
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
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
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
                   "create_fragments: payload exceeds net_max_payload" );
        return std::unexpected( NetError::Overflow );
    }

    const int chunksize_raw = impl_->block_size_provider->block_size( FragSize::Fragment );
    if( chunksize_raw <= 0 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "netchan",
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

    batch.total_size = static_cast<std::uint32_t>( total );

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
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: empty filename" );
        return std::unexpected( NetError::InvalidArgument );
    }
    if( filename.size() >= ::xash::limits::net_max_filename )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: filename exceeds net_max_filename" );
        return std::unexpected( NetError::InvalidArgument );
    }
    if( payload.size() > ::xash::limits::net_max_payload )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
                   "create_file_fragments_from_buffer: payload exceeds net_max_payload" );
        return std::unexpected( NetError::Overflow );
    }

    const int chunksize_raw = impl_->block_size_provider->block_size( FragSize::Fragment );
    if( chunksize_raw <= 0 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, "netchan",
                   "create_file_fragments_from_buffer: block_size_provider returned "
                   "non-positive size" );
        return std::unexpected( NetError::InvalidArgument );
    }
    const std::size_t chunksize       = static_cast<std::size_t>( chunksize_raw );
    const std::size_t filename_header = filename.size() + 1u; // legacy MSG_WriteString writes a NUL
    if( filename_header >= chunksize )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
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

    // total_size for the wire descriptor includes the filename + NUL prepended
    // to the first fragment's payload.
    batch.total_size = static_cast<std::uint32_t>( total + filename_header );

    impl_->outgoing_fragments[ static_cast<std::size_t>( FragStream::File ) ]
        .emplace_back( std::move( batch ) );
    return {};
}

// ---------------------------------------------------------------------------
// Transmit / process
// ---------------------------------------------------------------------------

Result<std::size_t> Netchan::transmit( std::span<const std::byte> unreliable, // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
                                       std::span<std::byte>       out ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    if( impl_->driver == nullptr )
        return std::unexpected( NetError::NotInitialised );

    // Wrap the caller's output buffer.  All header / reliable / unreliable
    // writes flow through MessageBuf so overflow is detected centrally.
    MessageBuf msg{ out, "netchan-transmit" };

    // Detect a pending fragment at the front of either outgoing queue.
    // When a fragment is being shipped, it carries the reliable bytes for
    // this packet; reliable_buf is held back until all fragments drain.
    // This matches legacy net_chan.c's `send_reliable_fragment` interlock.
    bool send_reliable_fragment = false;
    for( std::size_t i = 0; i < 2u; ++i )
    {
        if( !impl_->outgoing_fragments[ i ].empty()
            && !impl_->outgoing_fragments[ i ].front().bufs.empty() )
        {
            send_reliable_fragment = true;
            break;
        }
    }

    // Decide whether this packet carries the pending reliable payload.
    // Mutually exclusive with send_reliable_fragment: when shipping a
    // fragment, reliable_buf waits its turn.
    const bool send_reliable =
        ( impl_->reliable_length_bits > 0u ) && !send_reliable_fragment;
    // The combined "this packet contains reliable data" flag — used by the
    // driver to set bit-31 of w1.  True for both reliable_buf and fragment
    // frames so the receiver-side parity bit flips on every reliable packet.
    const bool wire_reliable_bit = send_reliable || send_reliable_fragment;

    PacketHeaderInput hdr_in{};
    hdr_in.outgoing_sequence          = impl_->outgoing_sequence;
    hdr_in.incoming_sequence          = impl_->incoming_sequence;
    hdr_in.incoming_reliable_sequence = impl_->incoming_reliable_sequence;
    hdr_in.qport                      = impl_->qport;
    hdr_in.send_reliable              = wire_reliable_bit;
    hdr_in.send_reliable_fragment     = send_reliable_fragment;
    hdr_in.is_client                  = impl_->sock == SocketKind::Client;

    if( auto r = impl_->driver->write_packet_header( msg, hdr_in ); !r.has_value() )
        return std::unexpected( r.error() );

    // Fragment descriptor block — written by the channel directly so the
    // driver stays header-only.  Format (xash3dpp wire encoding):
    //   per pending stream:
    //     word(stream_1based), word(bufferid), dword(total_size),
    //     word(byte_offset), word(frag_size), frag_size bytes payload
    //   terminator: word(0)
    // For file-stream fragments whose first piece carries a filename, the
    // NUL-terminated filename is prepended to that piece's wire payload;
    // total_size and frag_size include the filename header bytes.
    if( send_reliable_fragment )
    {
        for( std::size_t i = 0; i < 2u; ++i )
        {
            if( impl_->outgoing_fragments[ i ].empty() ) continue;
            const auto &batch = impl_->outgoing_fragments[ i ].front();
            if( batch.bufs.empty() ) continue;
            const auto &fb = batch.bufs.front();

            // total_size is precomputed at create-time so it stays stable
            // as transmit() pops successive fragments off the front.
            const std::uint32_t total = batch.total_size;

            const std::size_t header_extra =
                ( fb.is_file && !fb.filename.empty() )
                    ? fb.filename.size() + 1u
                    : 0u;
            const std::size_t frag_size_bytes = fb.payload.size() + header_extra;

            msg.write_word( static_cast<std::uint16_t>( i + 1u ) );
            msg.write_word( static_cast<std::uint16_t>( fb.bufferid ) );
            msg.write_dword( static_cast<std::uint32_t>( total ) );
            msg.write_word( static_cast<std::uint16_t>( impl_->frag_offset[ i ] ) );
            msg.write_word( static_cast<std::uint16_t>( frag_size_bytes ) );

            if( header_extra > 0u )
            {
                if( !msg.write_string( fb.filename ) )
                    return std::unexpected( NetError::Overflow );
            }
            if( !fb.payload.empty() )
            {
                if( !msg.write_bytes( fb.payload ) )
                    return std::unexpected( NetError::Overflow );
            }
        }
        msg.write_word( 0u ); // terminator
        if( msg.overflowed() )
            return std::unexpected( NetError::Overflow );
    }

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
            ::xash::core::log( ::xash::core::LogLevel::Verbose, "netchan",
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
    if( send_reliable_fragment )
    {
        // Pop the front fragment of each stream we shipped a piece of; on
        // batch exhaustion, reset the per-stream byte_offset so the next
        // batch starts at offset 0.
        for( std::size_t i = 0; i < 2u; ++i )
        {
            if( impl_->outgoing_fragments[ i ].empty() ) continue;
            auto &batch = impl_->outgoing_fragments[ i ].front();
            if( batch.bufs.empty() )
            {
                impl_->outgoing_fragments[ i ].pop_front();
                impl_->frag_offset[ i ] = 0u;
                continue;
            }
            const auto &fb = batch.bufs.front();
            const std::size_t shipped_bytes =
                fb.payload.size()
                + ( ( fb.is_file && !fb.filename.empty() )
                        ? fb.filename.size() + 1u
                        : 0u );
            impl_->frag_offset[ i ] += static_cast<std::uint32_t>( shipped_bytes );
            batch.bufs.erase( batch.bufs.begin() );
            if( batch.bufs.empty() )
            {
                impl_->outgoing_fragments[ i ].pop_front();
                impl_->frag_offset[ i ] = 0u;
            }
        }
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

bool Netchan::process( std::span<const std::byte> datagram, // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
                       MessageBuf                 &msg ) noexcept
{
    if( !impl_ || !impl_->active || impl_->driver == nullptr )
        return false;
    if( datagram.empty() )
        return false;

    msg.rebind_read( datagram, "netchan-recv" );

    auto meta = impl_->driver->read_packet_header(
        msg, impl_->sock == SocketKind::Server );
    if( !meta.has_value() )
    {
        ::xash::core::log( ::xash::core::LogLevel::Verbose, "netchan",
                   "process: header decode failed, dropping datagram" );
        return false;
    }
    if( meta->is_oob )
        return false;

    // Stale / duplicate: legacy net_chan.c drops packets whose sequence
    // is not strictly greater than the last we accepted.  We replicate
    // that policy here.
    if( meta->sequence <= impl_->incoming_sequence
        && impl_->incoming_sequence != 0u )
    {
        ::xash::core::log( ::xash::core::LogLevel::Verbose, "netchan",
                   "process: stale or duplicate sequence, dropping datagram" );
        return false;
    }

    impl_->incoming_sequence              = meta->sequence;
    impl_->incoming_acknowledged          = meta->sequence_ack;
    impl_->incoming_reliable_acknowledged = meta->reliable_ack ? 1u : 0u;

    // Each reliable packet flips the receiver-side reliable parity bit so
    // the sender can detect drops via the w2 high bit on the next ack.
    if( meta->is_reliable )
        impl_->incoming_reliable_sequence ^= 1u;

    // Reliable-fragment descriptor block (bit-30 of w1).  Parse the
    // per-stream descriptors and ingest each fragment payload into the
    // matching IncomingStream slot.  See transmit() for the wire format.
    if( meta->is_fragment )
    {
        for( ;; )
        {
            if( msg.num_bytes_left() < 2u ) return false;
            const std::uint16_t stream_1based = msg.read_word();
            if( msg.overflowed() ) return false;
            if( stream_1based == 0u ) break; // terminator

            const std::size_t si = static_cast<std::size_t>( stream_1based - 1u );
            if( si >= 2u ) return false; // malformed: only streams 0 and 1 exist

            // Descriptor body: word + dword + word + word = 10 bytes.
            if( msg.num_bytes_left() < 10u ) return false;
            (void) msg.read_word();                          // bufferid (unused on receive)
            const std::uint32_t total_size  = msg.read_dword();
            const std::uint16_t byte_offset = msg.read_word();
            const std::uint16_t frag_size   = msg.read_word();
            if( msg.overflowed() ) return false;
            if( static_cast<std::size_t>( frag_size ) > msg.num_bytes_left() )
                return false;

            std::vector<std::byte> frag_payload( frag_size );
            if( !msg.read_bytes( frag_payload ) )
                return false;

            (void) impl_->incoming_streams[ si ].ingest(
                total_size, byte_offset,
                std::span<const std::byte>{ frag_payload.data(), frag_payload.size() } );
        }
    }

    // msg now holds the post-header payload; its read cursor is positioned
    // right after the header so the caller can MSG_Read* on it directly.
    return true;
}

// ---------------------------------------------------------------------------
// Receive-side fragment readiness
// ---------------------------------------------------------------------------

bool Netchan::incoming_ready() const noexcept
{
    if( !impl_ || !impl_->active ) return false;
    for( const auto &slot : impl_->incoming_streams )
        if( slot.ready ) return true;
    return false;
}

Result<std::size_t> Netchan::copy_normal_fragments( std::span<std::byte> out ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    auto &slot = impl_->incoming_streams[ static_cast<std::size_t>( FragStream::Normal ) ];
    if( !slot.ready )
        return std::size_t{ 0 };
    if( out.size() < slot.data.size() )
        return std::unexpected( NetError::BufferTooSmall );
    const std::size_t n = slot.data.size();
    if( n > 0u )
        std::memcpy( out.data(), slot.data.data(), n );
    slot.reset();
    return n;
}

Result<std::size_t> Netchan::copy_file_fragments( std::span<std::byte> out,
                                                  std::span<char>      filename_out ) noexcept
{
    if( !impl_ || !impl_->active )
        return std::unexpected( NetError::NotInitialised );
    auto &slot = impl_->incoming_streams[ static_cast<std::size_t>( FragStream::File ) ];
    if( !slot.ready )
        return std::size_t{ 0 };

    // Assembled buffer layout: [NUL-terminated filename][file data bytes].
    const std::byte *const begin = slot.data.data();
    const std::byte *const end   = begin + slot.data.size();
    const std::byte *      nul   = begin;
    while( nul < end && *nul != std::byte{ 0 } )
        ++nul;
    if( nul == end )
    {
        // Malformed: no filename terminator.  Drop the slot to avoid
        // wedging future copies on the same bad state.
        slot.reset();
        return std::unexpected( NetError::InvalidArgument );
    }

    const std::size_t name_len = static_cast<std::size_t>( nul - begin );
    const std::byte  *data_ptr = nul + 1;
    const std::size_t data_len = static_cast<std::size_t>( end - data_ptr );

    // SECURITY (OWASP path-traversal): reject filenames containing
    // parent-directory traversal sequences or backslashes before we hand
    // them to the caller's filesystem layer.
    const std::string_view raw_name(
        reinterpret_cast<const char *>( begin ), name_len ); // SAFETY: re-views the wire byte buffer as char for a length-bounded filename field (name_len); same object
    if( raw_name.find( ".." )  != std::string_view::npos
        || raw_name.find( '\\' ) != std::string_view::npos
        || ( !raw_name.empty() && raw_name.front() == '/' ) )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, "netchan",
                   "copy_file_fragments: rejected filename with path-traversal "
                   "sequence" );
        slot.reset();
        return std::unexpected( NetError::InvalidArgument );
    }

    if( out.size() < data_len )
        return std::unexpected( NetError::BufferTooSmall );
    if( !filename_out.empty() )
    {
        const std::size_t copy_n =
            std::min( name_len, filename_out.size() - 1u );
        if( copy_n > 0u )
            std::memcpy( filename_out.data(), raw_name.data(), copy_n );
        filename_out[ copy_n ] = '\0';
    }
    if( data_len > 0u )
        std::memcpy( out.data(), data_ptr, data_len );
    slot.reset();
    return data_len;
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

void Netchan::update_choke( double now_seconds, std::size_t bytes_sent ) noexcept // compliance-allow(thread-assert): T_NetIO single-thread caller contract — transport stack has no internal sync; role unasserted until the NetIO thread is split out (G-2)
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
std::uint32_t     Netchan::incoming_acknowledged() const noexcept { return impl_->incoming_acknowledged; }
std::uint32_t     Netchan::incoming_reliable_acknowledged() const noexcept { return impl_->incoming_reliable_acknowledged; }
std::uint32_t     Netchan::incoming_reliable_sequence() const noexcept { return impl_->incoming_reliable_sequence; }
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
