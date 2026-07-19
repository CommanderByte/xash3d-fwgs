#pragma once
// xash3dpp — save/restore token hash table (Chunk 8, slice S8.1).
// Legacy reference: engine/server/sv_save.c :792-814 (StoreHashTable — flatten),
// :823-844 (BuildHashTable — rebuild); the slot-assignment hash is the game-DLL
// CSaveRestoreBuffer::TokenHash / HashString de-facto ABI (HL SDK cbase.cpp —
// NOT present in this repo; see deep-dive "Token hash system").
//
// WHY a standalone class rather than a SaveBuffer member-only helper: the two
// parity-critical operations (StoreHashTable flatten / BuildHashTable rebuild)
// and the slot hash are self-contained state->bytes machinery that must be unit-
// testable WITHOUT a live SaveBuffer, edict arena, or game DLL (save-boundary.md
// "codec testability" verdict).  SaveBuffer owns exactly one TokenTable as a
// member, mirroring how the legacy SAVERESTOREDATA owns pTokens.
//
// Slot model: slot i holds an owned token string; an EMPTY string means the slot
// is NULL/unoccupied — this is the exact legacy on-disk semantics, where "" and
// NULL are indistinguishable (BuildHashTable maps a leading '\0' back to NULL,
// sv_save.c:836) and real tokens (classnames / field names) are never empty.
//
// @thread-safety: T_Main-only, asserted — every mutating entry point asserts
// ThreadRole::Main (save runs entirely on T_Main; save-boundary.md §Threading).
// The const query/serialize helpers are pure and assert-free by design so they
// remain usable in a standalone (no-DLL) parse, SV_GetSaveComment-style.

#include <xash3dpp/save/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xash::save {

// The de-facto ABI token hash (HL SDK CSaveRestoreBuffer::HashString): a 32-bit
// rotate-right-by-4 XOR-accumulate over the token bytes.  `_rotr(hash, 4)` is
// spelled portably as (hash >> 4) | (hash << 28).  `static_cast<int>(ch)`
// reproduces the HL SDK's implicit `char` -> `int` promotion (sign-extends a
// signed char); token strings are ASCII so the high bit never fires in practice.
[[nodiscard]] inline std::uint32_t hash_string( std::string_view token ) noexcept
{
    std::uint32_t hash = 0;
    for ( char ch : token )
        hash = ( ( hash >> 4 ) | ( hash << 28 ) ) ^
               static_cast<std::uint32_t>( static_cast<int>( ch ) );
    return hash;
}

class TokenTable
{
public:
    // Construct a table with `token_count` slots, all NULL.  token_count is the
    // hash modulus (SAVE_HASHSTRINGS == 4095 for a real save; smaller for tests
    // and for a loaded file's own tokenCount).
    explicit TokenTable( std::size_t token_count );

    // Non-copyable; move is implicitly suppressed by the user-declared deleted
    // copy.  A TokenTable is only ever constructed in place (a SaveBuffer member
    // or a test local) and never copied or moved — it is a stateful component,
    // not a value aggregate (QJ), and abi_ptrs_ self-references slot storage.
    // NOT a pimpl type: all members are complete, so the destructor is implicit.
    TokenTable( const TokenTable & )            = delete;
    TokenTable &operator=( const TokenTable & ) = delete;

    // Insert (or find existing) a token, returning its slot index.  Hash + linear
    // probe (HL SDK TokenHash, sv_save.c token system).  TokenOverflow when a
    // full probe finds no free/matching slot.  Mutating -> asserts T_Main.
    [[nodiscard]] Result<std::uint16_t> insert( std::string_view token ) noexcept;

    // Look up a token without inserting.  Probe stops at the first NULL slot
    // (legacy semantics).  Pure read — no thread assert.
    [[nodiscard]] std::optional<std::uint16_t> find( std::string_view token ) const noexcept;

    // Slot contents; empty view for a NULL/out-of-range slot.  Pure read.
    [[nodiscard]] std::string_view token_at( std::size_t index ) const noexcept;

    // Number of slots (the hash modulus / on-disk tokenCount).  Pure read.
    [[nodiscard]] std::size_t token_count() const noexcept { return token_count_; }

    // StoreHashTable (sv_save.c:792-814): flatten every slot in slot order as
    // token-bytes + '\0' terminator (a NULL slot writes just the terminator).
    // Returns the byte count written (== on-disk tokenSize), or BufferExhausted.
    // Pure serialize (no shared-state mutation) — assert-free by design.
    [[nodiscard]] Result<std::size_t> flatten( std::span<std::byte> out ) const noexcept;

    // Byte count flatten() would emit (== on-disk tokenSize).  Pure read.
    [[nodiscard]] std::size_t flattened_size() const noexcept;

    // BuildHashTable (sv_save.c:823-844): rebuild slot strings from a flattened
    // blob by splitting it token_count() times on '\0'.  TruncatedBlock if the
    // blob ends before token_count() terminators are found.  An empty blob
    // leaves every slot NULL (legacy `if (tokenSize > 0)` guard).  Mutating ->
    // asserts T_Main.
    [[nodiscard]] Result<void> rebuild( std::span<const std::byte> blob ) noexcept;

    // Reset every slot to NULL (SaveClear token half, sv_save.c:744).  Mutating.
    void clear() noexcept;

    // The char** array for SAVERESTOREDATA.pTokens (the game-DLL sharing window).
    // Regenerated from the owned slot strings on each call.
    // @lifetime: TokenTable — the returned pointers alias this table's slot
    // storage and are valid until the next mutating call or table destruction.
    [[nodiscard]] char **abi_pointers() noexcept;

private:
    std::size_t              token_count_;
    // @lifetime: TokenTable — owned token strings; slot i empty() == NULL.
    std::vector<std::string> slots_;    // @pre-reserved: token_count (sized once in ctor; cold — per save/load op, not per-frame, Q-13)
    // @lifetime: TokenTable — scratch char** view of slots_ for the ABI window,
    // regenerated by abi_pointers(); not authoritative state.
    std::vector<char *>      abi_ptrs_; // @pre-reserved: token_count (sized once in ctor; cold path)
};

} // namespace xash::save
