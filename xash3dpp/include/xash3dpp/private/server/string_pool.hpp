#pragma once
// xash3dpp — server string pool (string_t ↔ char* arena)
// Legacy reference: engine/server/sv_game.c — str64_s (:2976),
// SV_AllocStringPool (:3061, the !USE_MMAP heap path — the legacy
// Windows-x64 baseline per server-boundary OQ-5/OQ-6), SV_EmptyStringPool
// (:3000), SV_SetStringArrayMode (:3032), SV_ProcessString (:3173),
// SV_AllocString (:3229), SV_MakeString (:3317), SV_GetString (:3340).
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md.
//
// One block of 2 × arena_size bytes: the first half is the DYNAMIC arena
// (per-level strings, reset on level change), the second half the STATIC
// arena (strings allocated before the server finishes spawning — survive
// level changes).  pStringBase points at the block start; string_t values
// are offsets from it.  Offset 0 is the empty string (the zeroed first
// byte of the block).  On arena exhaustion the write cursor WRAPS to the
// arena start and overwrites old strings (legacy numoverflows quirk —
// stale string_t values then read newer text; preserved).
//
// The mmap near-module probing of the Linux path is deliberately not
// ported (Q-20/OQ-6: the heap arena + SV_MakeString INT-range fallback IS
// the legacy-Windows parity baseline).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>

namespace xash::server {

struct StringPoolStats
{
    std::size_t total_alloc   = 0; // bytes ever written (legacy totalalloc)
    std::size_t max_alloc     = 0; // high-water offset (legacy maxalloc)
    std::size_t num_dups      = 0; // dedup hits
    std::size_t num_overflows = 0; // arena wraps
};

class StringPool
{
public:
    StringPool() = default;

    StringPool( const StringPool & )            = delete;
    StringPool &operator=( const StringPool & ) = delete;

    // arena_size 0 → legacy sizing: 65536 * ceil(max_edicts / 1024).
    // Tests pass a tiny explicit size to exercise the wrap path.
    [[nodiscard]] bool init( ::xash::memory::PoolHandle pool,
                             std::size_t max_edicts,
                             std::size_t arena_size = 0 );
    void shutdown();

    // pStringBase for globalvars_t (the block start).
    [[nodiscard]] const char *base() const noexcept { return block_; }

    // SV_SetStringArrayMode: static phase during spawn, dynamic after —
    // switching resets the cursor into the selected arena (no stats
    // clear, matching the legacy call).
    void set_dynamic( bool dynamic );

    // SV_EmptyStringPool: level-change reset of the active arena.
    void empty_pool( bool clear_stats );

    // SV_AllocString: escape-process, dedup against the active arena
    // (disable_dedup mirrors -str64dup), append with wrap-on-overflow.
    [[nodiscard]] ::xash::abi::string_t alloc_string( const char *value );

    // SV_MakeString: pointer already within INT range of base → direct
    // offset; anything else (e.g. a game-DLL static on x64) falls back to
    // alloc_string.
    [[nodiscard]] ::xash::abi::string_t make_string( const char *value );

    // SV_GetString.
    [[nodiscard]] const char *get_string( ::xash::abi::string_t s ) const noexcept
    {
        return block_ + s;
    }

    void set_allow_dup( bool allow ) noexcept { allow_dup_ = allow; }

    [[nodiscard]] const StringPoolStats &stats() const noexcept { return stats_; }
    [[nodiscard]] std::size_t arena_size() const noexcept { return arena_size_; }

    // SV_ProcessString: '\n' (and the xash extension '\r'/'\t' — GoldSrc
    // leaves those two alone, kept for FWGS parity) escape expansion.
    // dst == nullptr measures; returns length INCLUDING the terminator.
    static std::size_t process_string( char *dst, const char *src ) noexcept;

    // The SV_MakeString range test, exposed for deterministic unit tests
    // (pointer distance is synthesized there, never dereferenced).
    [[nodiscard]] static bool offset_in_int_range( const char *base,
                                                   const char *p ) noexcept;

private:
    ::xash::memory::PoolHandle pool_;
    char       *block_       = nullptr; // 2 × arena_size
    std::size_t arena_size_  = 0;
    bool        dynamic_     = false;
    bool        allow_dup_   = false;
    char       *cursor_base_ = nullptr; // legacy pstringbase (active arena)
    char       *old_base_    = nullptr; // legacy poldstringbase (dedup floor)
    char       *last_        = nullptr; // legacy plast (write cursor)
    StringPoolStats stats_;
};

} // namespace xash::server
