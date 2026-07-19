// xash3dpp — save/restore token hash table implementation (Chunk 8, S8.1).
// Byte-exact ports of StoreHashTable (sv_save.c:792-814) and BuildHashTable
// (sv_save.c:823-844); the slot hash is the HL SDK CSaveRestoreBuffer de-facto
// ABI (token_table.hpp).

#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/core/thread_role.hpp>

#include <cstring>

namespace xash::save {

TokenTable::TokenTable( std::size_t token_count )
    : token_count_( token_count )
{
    slots_.resize( token_count_ );          // token_count_ empty (NULL) slots
    abi_ptrs_.assign( token_count_, nullptr );
}

Result<std::uint16_t> TokenTable::insert( std::string_view token ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( token_count_ == 0 )
        return std::unexpected( SaveError::TokenOverflow );

    const std::size_t start = hash_string( token ) % token_count_;
    for ( std::size_t i = 0; i < token_count_; ++i )
    {
        std::size_t index = start + i;
        if ( index >= token_count_ )
            index -= token_count_;

        // Free slot, or an exact match already stored here (idempotent insert).
        if ( slots_[index].empty() )
        {
            slots_[index].assign( token ); // occupy (empty token stays NULL)
            return static_cast<std::uint16_t>( index );
        }
        if ( slots_[index] == token )
            return static_cast<std::uint16_t>( index );
    }

    return std::unexpected( SaveError::TokenOverflow );
}

std::optional<std::uint16_t> TokenTable::find( std::string_view token ) const noexcept
{
    if ( token_count_ == 0 )
        return std::nullopt;

    const std::size_t start = hash_string( token ) % token_count_;
    for ( std::size_t i = 0; i < token_count_; ++i )
    {
        std::size_t index = start + i;
        if ( index >= token_count_ )
            index -= token_count_;

        if ( slots_[index].empty() )     // probe stops at first NULL (legacy)
            return std::nullopt;
        if ( slots_[index] == token )
            return static_cast<std::uint16_t>( index );
    }
    return std::nullopt;
}

std::string_view TokenTable::token_at( std::size_t index ) const noexcept
{
    if ( index >= slots_.size() )
        return {};
    return slots_[index]; // empty view for a NULL slot
}

Result<std::size_t> TokenTable::flatten( std::span<std::byte> out ) const noexcept
{
    std::size_t pos = 0;
    for ( std::size_t i = 0; i < token_count_; ++i )
    {
        const std::string_view tok = slots_[i]; // "" for a NULL slot
        if ( pos + tok.size() + 1 > out.size() )
            return std::unexpected( SaveError::BufferExhausted );

        for ( char c : tok )
            out[pos++] = static_cast<std::byte>( static_cast<unsigned char>( c ) );
        out[pos++] = std::byte{ 0 }; // terminator (sv_save.c:807)
    }
    return pos; // == on-disk tokenSize
}

std::size_t TokenTable::flattened_size() const noexcept
{
    std::size_t n = 0;
    for ( std::size_t i = 0; i < token_count_; ++i )
        n += slots_[i].size() + 1; // +1 terminator per slot
    return n;
}

Result<void> TokenTable::rebuild( std::span<const std::byte> blob ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( auto &s : slots_ )
        s.clear(); // reset to NULL before parse

    if ( blob.empty() )
        return {}; // legacy `if (tokenSize > 0)` guard: empty blob => all NULL

    std::size_t pos = 0;
    for ( std::size_t i = 0; i < token_count_; ++i )
    {
        const std::size_t begin = pos;
        while ( pos < blob.size() && blob[pos] != std::byte{ 0 } )
            ++pos;

        if ( pos >= blob.size() ) // ran off the end before a terminator
            return std::unexpected( SaveError::TruncatedBlock );

        if ( pos > begin )
        {
            // SAFETY: std::byte and char are both byte types; a byte-span may be
            // read as chars.  The range [begin,pos) is bounds-checked above.
            slots_[i].assign( reinterpret_cast<const char *>( &blob[begin] ),
                              pos - begin );
        }
        // else: leading '\0' => NULL slot (already cleared above)

        ++pos; // consume the terminator (sv_save.c:837)
    }
    return {};
}

void TokenTable::clear() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    for ( auto &s : slots_ )
        s.clear();
}

char **TokenTable::abi_pointers() noexcept
{
    for ( std::size_t i = 0; i < token_count_; ++i )
        abi_ptrs_[i] = slots_[i].empty() ? nullptr : slots_[i].data();
    return abi_ptrs_.data();
}

void TokenTable::sync_from_abi() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( std::size_t i = 0; i < token_count_; ++i )
    {
        const char *p = abi_ptrs_[i];
        if ( p == nullptr || p[0] == '\0' )
            continue; // NULL / empty slot — unoccupied
        // An occupied slot the DLL left pointing at our own storage (idempotent
        // re-find) already matches; a genuinely DLL-interned pointer is copied
        // in.  assign() is a no-op-cost self-assign in the former case.
        if ( slots_[i].empty() || slots_[i] != p )
            slots_[i].assign( p );
    }
}

} // namespace xash::save
