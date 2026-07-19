#pragma once
// xash3dpp — SAVERESTOREDATA working buffer (Chunk 8, slice S8.1).
// Legacy reference: engine/server/sv_save.c :715-733 (SaveInit — alloc+size),
// :742-754 (SaveClear — reuse reset), :763-783 (SaveFinish — release),
// engine/eiface.h :313-346 / abi/eiface.hpp:158-179 (SAVERESTOREDATA layout).
//
// The legacy engine allocs `Mem_Calloc(host.mempool, sizeof(SAVERESTOREDATA) +
// size)` plus a separate `pTokens` array, hands the struct to the game DLL via
// `svgame.globals->pSaveData`, and frees it in SaveFinish.  The rewrite promotes
// that to a pool-owned RAII class (Q-22 / P-7): create_save_buffer is the
// factory, the destructor is SaveFinish, reset() is SaveClear, and to_abi()
// projects a vendored SAVERESTOREDATA over the owned storage for the DLL window.
//
// @thread-safety: T_Main-only, asserted — every mutating entry point asserts
// ThreadRole::Main (save is entirely T_Main; save-boundary.md §Threading).
// @lifetime: the SAVERESTOREDATA that to_abi() fills borrows raw pointers into
// this buffer's owned storage and is valid only while *this is alive and not
// further mutated.

#include <xash3dpp/save/errors.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace xash::save {

class SaveBuffer
{
public:
    // Constructed only via create_save_buffer (Q-22 factory).  Public because
    // pool_new needs to reach it; do not construct directly.
    SaveBuffer( ::xash::memory::PoolHandle pool, std::size_t buffer_size,
                std::size_t token_count, float time ) noexcept;
    ~SaveBuffer();

    // Pool-owned, self-referenced by SaveBufferSink -> address must be stable.
    SaveBuffer( const SaveBuffer & )            = delete;
    SaveBuffer &operator=( const SaveBuffer & ) = delete;
    SaveBuffer( SaveBuffer && )                 = delete;
    SaveBuffer &operator=( SaveBuffer && )      = delete;

    // Pool-aware deallocation (Q-22): routes back to the source pool so the
    // default std::unique_ptr<SaveBuffer> deleter is correct.
    static void operator delete( void *p ) noexcept;
    static void operator delete( void *p, std::size_t ) noexcept;

    // True when the working buffer was allocated (false == OOM at construction).
    [[nodiscard]] bool valid() const noexcept { return base_ != nullptr; }

    // --- Sequential write primitives (bounds-checked; BufferExhausted on
    //     overrun).  Integers/floats are encoded explicitly little-endian to
    //     match the LE-only on-disk format (save-boundary.md §Compat scope). ---
    [[nodiscard]] Result<void> write_bytes( std::span<const std::byte> data ) noexcept;
    [[nodiscard]] Result<void> write_i16( std::int16_t v ) noexcept;
    [[nodiscard]] Result<void> write_i32( std::int32_t v ) noexcept;
    [[nodiscard]] Result<void> write_f32( float v ) noexcept;

    // --- Sequential read primitives (bounds-checked against the valid data
    //     region; TruncatedBlock past the end). ---
    [[nodiscard]] Result<std::span<const std::byte>> read_bytes( std::size_t n ) noexcept;
    [[nodiscard]] Result<std::int16_t> read_i16() noexcept;
    [[nodiscard]] Result<std::int32_t> read_i32() noexcept;
    [[nodiscard]] Result<float>        read_f32() noexcept;

    // Copy an on-disk image into the working buffer for reading (load path):
    // sets the valid-data size and rewinds the cursor to 0.  BufferExhausted if
    // the image does not fit.
    [[nodiscard]] Result<void> load_from( std::span<const std::byte> image ) noexcept;

    // SaveClear (sv_save.c:742-754): rewind cursor, drop valid-data size, clear
    // the token table for buffer reuse.
    void reset() noexcept;

    // Chunk 8, slice S8.7 (live game-DLL bridge).  A real `pfnSave`/`pfnRestore`
    // writes field-record bytes straight into the ABI window via
    // SAVERESTOREDATA.pCurrentData and advances SAVERESTOREDATA.size itself
    // (HL-SDK CSave::BufferData) — bypassing write_bytes(), so `cursor_`/
    // `data_size_` are unaware of the growth.  After such a call, adopt the DLL's
    // new total (`new_size` == SAVERESTOREDATA.size) as the valid-data extent so
    // the surrounding sink-based writes/reads (header/ETABLE blocks) continue
    // sequentially past the entity payload.  new_size must be <= capacity() and
    // >= the current cursor (a DLL never rewinds); rejected with BufferExhausted
    // otherwise (reject-gracefully).
    [[nodiscard]] Result<void> commit_abi_write( std::size_t new_size ) noexcept;

    // Move the read/write cursor.  pos must be <= capacity.
    [[nodiscard]] Result<void> seek( std::size_t pos ) noexcept;

    // Restore-side bounded read view at an ABSOLUTE position, WITHOUT moving the
    // cursor (Chunk 8, slice S8.4).  This is the read-at-location primitive the
    // per-entity restore loop needs: `pSaveData->pCurrentData = pSaveData->
    // pBaseData + pTable->location` (sv_save.c:1667) projects to a bounded
    // [pos, pos+n) window over the loaded data region.  Unlike read_bytes it does
    // not advance `cursor_`, so each entity is positioned independently of the
    // last.  Pure read (assert-free by design, matching data()).  TruncatedBlock
    // if [pos, pos+n) is not wholly within the valid data region.
    // @lifetime: SaveBuffer — the returned span aliases owned storage.
    [[nodiscard]] Result<std::span<const std::byte>>
    view_at( std::size_t pos, std::size_t n ) const noexcept;

    // Project a vendored SAVERESTOREDATA over this buffer for the game-DLL
    // window (SaveInit shape: cursor at base, tokenSize 0 until StoreHashTable
    // runs, pTokens = the all-NULL token window the DLL fills).  Overwrites
    // every field of |out| (landmark / level-list fields zeroed).
    void to_abi( ::xash::abi::SAVERESTOREDATA &out ) noexcept;

    // --- Accessors ---
    [[nodiscard]] std::size_t capacity() const noexcept { return buffer_size_; }
    [[nodiscard]] std::size_t size()     const noexcept { return data_size_; }   // SAVERESTOREDATA.size
    [[nodiscard]] std::size_t cursor()   const noexcept { return cursor_; }
    [[nodiscard]] float       time()     const noexcept { return time_; }

    // The valid (written / loaded) data as a read-only span, for the pure parse
    // helpers (next_field_record).  @lifetime: SaveBuffer.
    [[nodiscard]] std::span<const std::byte> data() const noexcept;

    [[nodiscard]] TokenTable       &tokens() noexcept       { return tokens_; }
    [[nodiscard]] const TokenTable &tokens() const noexcept { return tokens_; }

private:
    ::xash::memory::PoolHandle pool_;
    // @lifetime: SaveBuffer — pool-owned working buffer, freed in the destructor.
    std::byte  *base_ = nullptr;
    std::size_t buffer_size_ = 0; // total capacity (SAVERESTOREDATA.bufferSize)
    std::size_t data_size_   = 0; // valid bytes written/loaded (SAVERESTOREDATA.size)
    std::size_t cursor_      = 0; // sequential read/write position
    float       time_        = 0.0f;
    TokenTable  tokens_;          // owns pTokens, mirrors SAVERESTOREDATA ownership
};

// Factory (Q-22 / P-7): allocates a `buffer_size`-byte working buffer from
// `pool` and a `token_count`-slot token table.  Defaults are the frozen save
// budgets (SAVE_HEAPSIZE / SAVE_HASHSTRINGS).  `time` seeds SAVERESTOREDATA.time
// (legacy uses svgame.globals->time).  Returns nullptr on OOM (logged, Q-5).
[[nodiscard]] std::unique_ptr<SaveBuffer>
create_save_buffer( ::xash::memory::PoolHandle pool,
                    std::size_t buffer_size = ::xash::limits::save_heap_size,
                    std::size_t token_count = ::xash::limits::save_hash_strings,
                    float       time        = 0.0f ) noexcept;

} // namespace xash::save
