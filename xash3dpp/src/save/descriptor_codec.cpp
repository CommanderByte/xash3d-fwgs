// xash3dpp — TYPEDESCRIPTION-driven field-block codec (Chunk 8, slice S8.3).
// See descriptor_codec.hpp for the legacy citations and codec-fact notes.

#include <xash3dpp/private/save/descriptor_codec.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <bit>
#include <cstring>
#include <vector>

namespace xash::save {

namespace
{
// Max transformed-payload scratch: FIELD_TIME/FIELD_EDICT rebuild the payload
// element-by-element.  Engine-owned header blocks only ever carry fieldSize==1
// for those types; this bound is generous headroom, not a format constant.
constexpr std::size_t k_transform_scratch = 64;

// HL-SDK DataEmpty: a field whose in-memory bytes are ALL ZERO is omitted.
[[nodiscard]] bool data_empty( const std::byte *p, std::size_t n ) noexcept
{
    for ( std::size_t i = 0; i < n; ++i )
        if ( p[i] != std::byte{ 0 } )
            return false;
    return true;
}

// The engine STRING family — companion-text fields (descriptor_codec.hpp
// FieldTextBinding doc; mirrors ETABLE classname, entity_table.cpp).
[[nodiscard]] bool is_text_field( ::xash::abi::FIELDTYPE t ) noexcept
{
    switch ( t )
    {
    case ::xash::abi::FIELD_STRING:
    case ::xash::abi::FIELD_MODELNAME:
    case ::xash::abi::FIELD_SOUNDNAME:
        return true;
    default:
        return false;
    }
}

// Resolves `field_name`'s companion `std::string*` from the caller's binding
// table (case-insensitive, matching the rest of this codec's name lookups).
// nullptr if no binding matches — a caller error for any text-family field
// actually present in the descriptor table (BadFieldRecord at the call site).
[[nodiscard]] std::string *find_text_binding( std::span<const FieldTextBinding> bindings,
                                              std::string_view field_name ) noexcept
{
    for ( const auto &binding : bindings )
        if ( ::xash::utilities::ci_equal( binding.field_name, field_name ) )
            return binding.text;
    return nullptr;
}

[[nodiscard]] float read_f32_le( const std::byte *p ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
    return std::bit_cast<float>( u );
}

void write_f32_le( std::byte *p, float v ) noexcept
{
    const auto u = std::bit_cast<std::uint32_t>( v );
    p[0] = static_cast<std::byte>( u & 0xFFu );
    p[1] = static_cast<std::byte>( ( u >> 8 ) & 0xFFu );
    p[2] = static_cast<std::byte>( ( u >> 16 ) & 0xFFu );
    p[3] = static_cast<std::byte>( ( u >> 24 ) & 0xFFu );
}

void write_i32_le( std::byte *p, std::int32_t v ) noexcept
{
    const auto u = static_cast<std::uint32_t>( v );
    p[0] = static_cast<std::byte>( u & 0xFFu );
    p[1] = static_cast<std::byte>( ( u >> 8 ) & 0xFFu );
    p[2] = static_cast<std::byte>( ( u >> 16 ) & 0xFFu );
    p[3] = static_cast<std::byte>( ( u >> 24 ) & 0xFFu );
}

[[nodiscard]] std::int32_t read_i32_le( const std::byte *p ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( p[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}
} // namespace

// ---------------------------------------------------------------------------
// write_descriptor_block
// ---------------------------------------------------------------------------

Result<void>
write_descriptor_block( IFieldSink &sink, TokenTable &tokens, std::string_view block_name,
                        const void *base, std::span<const ::xash::abi::TYPEDESCRIPTION> fields,
                        float time_basis, EdictIndexFn edict_index, void *edict_ctx,
                        std::span<const FieldTextBinding> text_bindings ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const auto *base_bytes = static_cast<const std::byte *>( base );

    // Pass 1 — DataEmpty skip count (HL-SDK CSave::WriteFields precalculates the
    // empties, then writes actualCount = fieldCount - emptyCount).  Text-family
    // fields test companion-text emptiness instead of the raw in-struct bytes
    // (FieldTextBinding doc — the in-struct handle is not the wire truth).
    std::int32_t actual_count = 0;
    for ( const auto &f : fields )
    {
        if ( is_text_field( f.fieldType ) )
        {
            const std::string *text = find_text_binding( text_bindings, f.fieldName );
            if ( !text )
                return std::unexpected( SaveError::BadFieldRecord );
            if ( !text->empty() )
                ++actual_count;
            continue;
        }

        const std::size_t span = static_cast<std::size_t>( f.fieldSize ) *
                                 static_cast<std::size_t>( field_element_size( f.fieldType ) );
        if ( !data_empty( base_bytes + f.fieldOffset, span ) )
            ++actual_count;
    }

    if ( auto r = write_block_header( sink, tokens, block_name, actual_count ); !r )
        return r;

    // Pass 2 — one record per non-empty field, in descriptor order.  A field's
    // NAME token is interned only when it is actually written (a skipped field's
    // name is never interned), matching the legacy BufferField/TokenHash flow.
    for ( const auto &f : fields )
    {
        // Text-family fields: the payload is the companion TEXT + trailing
        // NUL (size = strlen + 1) — mirrors write_classname_field
        // (entity_table.cpp).  DataEmpty = empty text -> skip, name not
        // interned.  Handled entirely outside the raw-bytes switch below.
        if ( is_text_field( f.fieldType ) )
        {
            const std::string *text = find_text_binding( text_bindings, f.fieldName );
            if ( !text )
                return std::unexpected( SaveError::BadFieldRecord );
            if ( text->empty() )
                continue;

            auto tok = tokens.insert( f.fieldName );
            if ( !tok )
                return std::unexpected( tok.error() );

            std::vector<std::byte> payload( text->size() + 1 );
            for ( std::size_t i = 0; i < text->size(); ++i )
                payload[i] = static_cast<std::byte>( static_cast<unsigned char>( ( *text )[i] ) );
            payload[text->size()] = std::byte{ 0 }; // trailing NUL terminator

            if ( auto r = sink.write_field_record( *tok, payload ); !r )
                return r;
            continue;
        }

        const int         elem = field_element_size( f.fieldType );
        const std::size_t span = static_cast<std::size_t>( f.fieldSize ) *
                                 static_cast<std::size_t>( elem );
        const std::byte *field_ptr = base_bytes + f.fieldOffset;

        if ( data_empty( field_ptr, span ) )
            continue;

        auto tok = tokens.insert( f.fieldName );
        if ( !tok )
            return std::unexpected( tok.error() );

        switch ( f.fieldType )
        {
        // Raw-copy types: the payload IS the in-memory little-endian bytes
        // (WriteInt/WriteFloat/WriteData/WriteVector all BufferData the field
        // verbatim).  FIELD_CHARACTER writes the FULL fixed array width.
        case ::xash::abi::FIELD_INTEGER:
        case ::xash::abi::FIELD_BOOLEAN:
        case ::xash::abi::FIELD_FLOAT:
        case ::xash::abi::FIELD_VECTOR:
        case ::xash::abi::FIELD_POSITION_VECTOR:
        case ::xash::abi::FIELD_CHARACTER:
        case ::xash::abi::FIELD_SHORT:
        {
            if ( auto r = sink.write_field_record(
                     *tok, std::span<const std::byte>( field_ptr, span ) );
                 !r )
                return r;
            break;
        }

        // FIELD_TIME: WriteTime rebases each element by pSaveData->time before
        // encoding (sv_save.c FIELD_TIME; time_basis == 0 for the Save Header).
        case ::xash::abi::FIELD_TIME:
        {
            if ( span > k_transform_scratch )
                return std::unexpected( SaveError::BadFieldRecord );
            std::byte scratch[k_transform_scratch];
            for ( int j = 0; j < f.fieldSize; ++j )
            {
                const float raw = read_f32_le( field_ptr + static_cast<std::size_t>( j ) * 4 );
                write_f32_le( scratch + static_cast<std::size_t>( j ) * 4, raw - time_basis );
            }
            if ( auto r = sink.write_field_record(
                     *tok, std::span<const std::byte>( scratch, span ) );
                 !r )
                return r;
            break;
        }

        // FIELD_EDICT: DataEmpty already screened NULL pointers; a live edict is
        // encoded as its entity index (HL-SDK EntityIndex()) via the injected
        // resolver.  span is gSizes-derived (4 bytes/elem), the on-wire int width.
        case ::xash::abi::FIELD_EDICT:
        case ::xash::abi::FIELD_ENTITY:
        case ::xash::abi::FIELD_EHANDLE:
        case ::xash::abi::FIELD_EVARS:
        case ::xash::abi::FIELD_CLASSPTR:
        {
            if ( !edict_index || span > k_transform_scratch )
                return std::unexpected( SaveError::BadFieldRecord );
            std::byte scratch[k_transform_scratch];
            for ( int j = 0; j < f.fieldSize; ++j )
            {
                ::xash::abi::edict_t *ent = nullptr;
                std::memcpy( &ent,
                             base_bytes + f.fieldOffset +
                                 static_cast<std::size_t>( j ) * sizeof( void * ),
                             sizeof( void * ) );
                write_i32_le( scratch + static_cast<std::size_t>( j ) * 4,
                              edict_index( ent, edict_ctx ) );
            }
            if ( auto r = sink.write_field_record(
                     *tok, std::span<const std::byte>( scratch, span ) );
                 !r )
                return r;
            break;
        }

        default:
            // Unsupported field type in an engine-owned block — the engine
            // structs never use FIELD_POINTER/FIELD_FUNCTION here, and the
            // STRING family (FIELD_STRING/FIELD_MODELNAME/FIELD_SOUNDNAME) is
            // handled above via the text-field branch, never reaching this
            // switch.
            return std::unexpected( SaveError::BadFieldRecord );
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// read_descriptor_block
// ---------------------------------------------------------------------------

Result<void>
read_descriptor_block( std::span<const std::byte> data, std::size_t &offset,
                       const TokenTable &tokens, std::string_view block_name, void *base,
                       std::span<const ::xash::abi::TYPEDESCRIPTION> fields,
                       float time_basis, std::span<const FieldTextBinding> text_bindings ) noexcept
{
    auto hdr = read_block_header( data, offset, tokens, block_name );
    if ( !hdr )
        return std::unexpected( hdr.error() );
    if ( *hdr < 0 )
        return std::unexpected( SaveError::BadFieldRecord );

    auto *base_bytes = static_cast<std::byte *>( base );

    for ( std::int32_t i = 0; i < *hdr; ++i )
    {
        auto rec = next_field_record( data, offset );
        if ( !rec )
            return std::unexpected( rec.error() );

        const std::string_view name = tokens.token_at( rec->token_idx );

        // Name-matched overlay: find the descriptor this record belongs to.
        const ::xash::abi::TYPEDESCRIPTION *field = nullptr;
        for ( const auto &f : fields )
        {
            if ( ::xash::utilities::ci_equal( name, f.fieldName ) )
            {
                field = &f;
                break;
            }
        }
        if ( !field )
            continue; // unrecognized name -> skip (forward-compat)

        // Text-family fields: the payload is TEXT + trailing NUL (mirrors
        // ETABLE classname's D1 read, entity_table.cpp) — require the NUL,
        // capture into the bound companion string, and leave the in-struct
        // handle untouched (FieldTextBinding doc: the handle is process-
        // local, the TEXT is the wire truth).
        if ( is_text_field( field->fieldType ) )
        {
            std::string *text = find_text_binding( text_bindings, field->fieldName );
            if ( !text )
                return std::unexpected( SaveError::BadFieldRecord );
            if ( rec->payload.empty() || rec->payload.back() != std::byte{ 0 } )
                return std::unexpected( SaveError::BadFieldRecord );

            // SAFETY: std::byte and char are both byte types; the range
            // excludes the trailing NUL (bounds-checked above).
            text->assign( reinterpret_cast<const char *>( rec->payload.data() ),
                         rec->payload.size() - 1 );
            continue;
        }

        const int         elem = field_element_size( field->fieldType );
        const std::size_t span = static_cast<std::size_t>( field->fieldSize ) *
                                 static_cast<std::size_t>( elem );
        std::byte *field_ptr = base_bytes + field->fieldOffset;

        switch ( field->fieldType )
        {
        case ::xash::abi::FIELD_INTEGER:
        case ::xash::abi::FIELD_BOOLEAN:
        case ::xash::abi::FIELD_FLOAT:
        case ::xash::abi::FIELD_VECTOR:
        case ::xash::abi::FIELD_POSITION_VECTOR:
        case ::xash::abi::FIELD_CHARACTER:
        case ::xash::abi::FIELD_SHORT:
        {
            if ( rec->payload.size() != span )
                return std::unexpected( SaveError::BadFieldRecord );
            std::memcpy( field_ptr, rec->payload.data(), span );
            break;
        }

        case ::xash::abi::FIELD_TIME:
        {
            if ( rec->payload.size() != span )
                return std::unexpected( SaveError::BadFieldRecord );
            for ( int j = 0; j < field->fieldSize; ++j )
            {
                const float stored =
                    read_f32_le( rec->payload.data() + static_cast<std::size_t>( j ) * 4 );
                write_f32_le( field_ptr + static_cast<std::size_t>( j ) * 4,
                              stored + time_basis );
            }
            break;
        }

        case ::xash::abi::FIELD_EDICT:
        case ::xash::abi::FIELD_ENTITY:
        case ::xash::abi::FIELD_EHANDLE:
        case ::xash::abi::FIELD_EVARS:
        case ::xash::abi::FIELD_CLASSPTR:
        {
            // Decode the int index for validity, but leave the destination
            // pointer untouched — arena reconstruction is server-core (deferred),
            // matching the ETABLE reader's `pent` handling.  Never exercised in
            // S8.3 (landmark edicts are NULL -> DataEmpty-skipped at write time).
            if ( rec->payload.size() != span )
                return std::unexpected( SaveError::BadFieldRecord );
            (void)read_i32_le( rec->payload.data() );
            break;
        }

        default:
            return std::unexpected( SaveError::BadFieldRecord );
        }
    }

    return {};
}

} // namespace xash::save
