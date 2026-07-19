// xash3dpp — ENTITYTABLE build/read layer implementation (Chunk 8, S8.2).
// See entity_table.hpp for the legacy citations and the FIELD_STRING /
// edict_from() deviation notes.

#include <xash3dpp/private/save/entity_table.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <array>
#include <vector>

namespace xash::save {

namespace
{
// Little-endian int32 encode/decode for ETABLE's raw int fields
// (id/location/size/flags). Local to this TU, matching the existing
// per-file duplication style (save_buffer.cpp / field_sink.cpp each carry
// their own copies).

[[nodiscard]] std::array<std::byte, 4> encode_i32_le( std::int32_t v ) noexcept
{
    const auto u = static_cast<std::uint32_t>( v );
    return {
        static_cast<std::byte>( u & 0xFFu ),
        static_cast<std::byte>( ( u >> 8 ) & 0xFFu ),
        static_cast<std::byte>( ( u >> 16 ) & 0xFFu ),
        static_cast<std::byte>( ( u >> 24 ) & 0xFFu ),
    };
}

[[nodiscard]] std::int32_t decode_i32_le( std::span<const std::byte> b ) noexcept
{
    const auto u =
        static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[2] ) ) << 16 ) |
        ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}

// Write one int32-valued field record ("id"/"location"/"size"/"flags").
// Caller has already applied the DataEmpty (all-zero) skip test — this
// helper always emits a record when called.
[[nodiscard]] Result<void>
write_int_field( IFieldSink &sink, TokenTable &tokens, std::string_view name,
                 std::int32_t value ) noexcept
{
    auto tok = tokens.insert( name );
    if ( !tok )
        return std::unexpected( tok.error() );
    return sink.write_field_record( *tok, encode_i32_le( value ) );
}

// Write the classname FIELD_STRING field: payload is the raw string bytes
// INCLUDING the trailing '\0' (`size = strlen + 1`) — SV_GetSaveComment's
// independent hand-parse (sv_save.c:2390-2436) proves field VALUES live
// inline in the payload; the token table indexes NAMES only (block/field
// names), never FIELD_STRING values. Caller has already applied the
// DataEmpty (empty-text) skip test — this helper always emits a record.
[[nodiscard]] Result<void>
write_classname_field( IFieldSink &sink, TokenTable &tokens, std::string_view name,
                       std::string_view text ) noexcept
{
    auto name_tok = tokens.insert( name );
    if ( !name_tok )
        return std::unexpected( name_tok.error() );

    std::vector<std::byte> payload( text.size() + 1 );
    for ( std::size_t i = 0; i < text.size(); ++i )
        payload[i] = static_cast<std::byte>( static_cast<unsigned char>( text[i] ) );
    payload[text.size()] = std::byte{ 0 }; // trailing NUL terminator

    return sink.write_field_record( *name_tok, payload );
}
} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void EntityTable::init( std::size_t count ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // Value-initialised ENTITYTABLE{} zero-fills every member (matches
    // legacy Mem_Calloc); id is then set per-row, pent stays NULL (see
    // header doc — this component has no edict-arena dependency).
    rows_.assign( count, ::xash::abi::ENTITYTABLE{} );
    classnames_.assign( count, std::string{} );

    for ( std::size_t i = 0; i < count; ++i )
        rows_[i].id = static_cast<int>( i );
}

::xash::abi::ENTITYTABLE &EntityTable::row( std::size_t index ) noexcept
{
    return rows_[index];
}

const ::xash::abi::ENTITYTABLE &EntityTable::row( std::size_t index ) const noexcept
{
    return rows_[index];
}

void EntityTable::set_classname( std::size_t index, std::string_view text ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    classnames_[index] = text;
}

std::string_view EntityTable::classname( std::size_t index ) const noexcept
{
    return classnames_[index];
}

// ---------------------------------------------------------------------------
// edict_from
// ---------------------------------------------------------------------------

::xash::abi::edict_t *EntityTable::edict_from( int index ) const noexcept
{
    // Deviation from legacy UB: tableCount == 0 -> bound(0,i,-1) == -1 ->
    // pTable[-1] (out-of-bounds read). Reject gracefully instead.
    if ( rows_.empty() )
        return nullptr;

    // bound(0, index, tableCount - 1) — public/xash3d_mathlib.h:141: a CLAMP
    // ((num >= min) ? (num < max ? num : max) : min), not a modulo.
    const int max_index = static_cast<int>( rows_.size() ) - 1;
    int       clamped   = index;
    if ( clamped < 0 )
        clamped = 0;
    else if ( clamped > max_index )
        clamped = max_index;

    return rows_[static_cast<std::size_t>( clamped )].pent;
}

// ---------------------------------------------------------------------------
// serialize / deserialize
// ---------------------------------------------------------------------------

Result<void> EntityTable::serialize( IFieldSink &sink, TokenTable &tokens ) const noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    static constexpr std::string_view k_block_name = "ETABLE"; // sv_save.c:977

    for ( std::size_t i = 0; i < rows_.size(); ++i )
    {
        const auto &row = rows_[i];

        // HL-SDK CSave::WriteFields DataEmpty semantics: a field whose
        // in-memory bytes are ALL ZERO is omitted from the stream entirely.
        // int32 fields: zero test over the 4 bytes (== value != 0); classname
        // (FIELD_STRING): omitted when its text is empty/unset. The block
        // header's field count is the number of fields ACTUALLY emitted
        // after this skip (D3), computed here before any record is written.
        const bool has_id        = row.id != 0;
        const bool has_location  = row.location != 0;
        const bool has_size      = row.size != 0;
        const bool has_flags     = row.flags != 0;
        const bool has_classname = !classnames_[i].empty();

        const auto actual_count = static_cast<std::int32_t>(
            ( has_id ? 1 : 0 ) + ( has_location ? 1 : 0 ) + ( has_size ? 1 : 0 ) +
            ( has_flags ? 1 : 0 ) + ( has_classname ? 1 : 0 ) );

        if ( auto r = write_block_header( sink, tokens, k_block_name, actual_count ); !r )
            return r;

        // A field's NAME token is interned only at the moment it is written
        // (legacy: TokenHash is called from inside BufferField) — a skipped
        // field's name is never inserted into `tokens` for this row.
        if ( has_id )
        {
            if ( auto r = write_int_field( sink, tokens, "id", row.id ); !r )
                return r;
        }
        if ( has_location )
        {
            if ( auto r = write_int_field( sink, tokens, "location", row.location ); !r )
                return r;
        }
        if ( has_size )
        {
            if ( auto r = write_int_field( sink, tokens, "size", row.size ); !r )
                return r;
        }
        if ( has_flags )
        {
            if ( auto r = write_int_field( sink, tokens, "flags", row.flags ); !r )
                return r;
        }
        if ( has_classname )
        {
            if ( auto r = write_classname_field( sink, tokens, "classname", classnames_[i] ); !r )
                return r;
        }
    }
    return {};
}

Result<void>
EntityTable::deserialize( std::span<const std::byte> data, std::size_t &offset,
                          const TokenTable &tokens ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    static constexpr std::string_view k_block_name = "ETABLE";

    for ( std::size_t i = 0; i < rows_.size(); ++i )
    {
        auto hdr = read_block_header( data, offset, tokens, k_block_name );
        if ( !hdr )
            return std::unexpected( hdr.error() );
        if ( *hdr < 0 )
            return std::unexpected( SaveError::BadFieldRecord );

        // Start from the row's PRE-EXISTING state (set by init(), or by a
        // previous deserialize()), not a fresh zero-initialized struct: a
        // field omitted from the stream (the DataEmpty skip in serialize())
        // must leave the destination value untouched, not reset to zero.
        // This matters for `id` specifically — init() sets `id = row index`
        // (non-zero for every row but 0), so only row 0's `id` field is ever
        // skipped at write time, and it must round-trip via the pre-existing
        // init() value rather than an incidental zero-init default.
        ::xash::abi::ENTITYTABLE parsed         = rows_[i];
        std::string              classname_text = classnames_[i];

        for ( std::int32_t f = 0; f < *hdr; ++f )
        {
            auto rec = next_field_record( data, offset );
            if ( !rec )
                return std::unexpected( rec.error() );

            const std::string_view name = tokens.token_at( rec->token_idx );

            if ( ::xash::utilities::ci_equal( name, "id" ) )
            {
                if ( rec->payload.size() != sizeof( std::int32_t ) )
                    return std::unexpected( SaveError::BadFieldRecord );
                parsed.id = decode_i32_le( rec->payload );
            }
            else if ( ::xash::utilities::ci_equal( name, "location" ) )
            {
                if ( rec->payload.size() != sizeof( std::int32_t ) )
                    return std::unexpected( SaveError::BadFieldRecord );
                parsed.location = decode_i32_le( rec->payload );
            }
            else if ( ::xash::utilities::ci_equal( name, "size" ) )
            {
                if ( rec->payload.size() != sizeof( std::int32_t ) )
                    return std::unexpected( SaveError::BadFieldRecord );
                parsed.size = decode_i32_le( rec->payload );
            }
            else if ( ::xash::utilities::ci_equal( name, "flags" ) )
            {
                if ( rec->payload.size() != sizeof( std::int32_t ) )
                    return std::unexpected( SaveError::BadFieldRecord );
                parsed.flags = decode_i32_le( rec->payload );
            }
            else if ( ::xash::utilities::ci_equal( name, "classname" ) )
            {
                // D1: the payload is the raw TEXT bytes INCLUDING the
                // trailing '\0' — require it (a payload without a trailing
                // NUL is malformed, not a valid empty/short string).
                if ( rec->payload.empty() || rec->payload.back() != std::byte{ 0 } )
                    return std::unexpected( SaveError::BadFieldRecord );

                // SAFETY: std::byte and char are both byte types; the range
                // excludes the trailing NUL (bounds-checked above).
                classname_text.assign(
                    reinterpret_cast<const char *>( rec->payload.data() ),
                    rec->payload.size() - 1 );
            }
            // else: unrecognized field name -> skip (forward-compat; mirrors
            // the legacy codec's per-name field lookup, which ignores fields
            // it does not know about rather than failing the whole block).
        }

        parsed.pent = nullptr; // never read from disk (deep-dive: ETABLE serialization)
        rows_[i]        = parsed;
        classnames_[i]  = classname_text;
    }
    return {};
}

} // namespace xash::save
