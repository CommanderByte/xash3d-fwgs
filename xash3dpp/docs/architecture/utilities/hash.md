# Hash utilities

> **Header**: `xash3dpp/include/xash3dpp/utilities/hash.hpp`  
> **Source**: `xash3dpp/src/utilities/hash.cpp`  
> **Namespace**: `xash::utilities`  
> **Legacy reference**: `public/crclib.h`, `public/crclib.c`

## Purpose

Provides CRC32 (IEEE 802.3 polynomial) and MD5 digest functions used for:
- Resource checksum verification (map files, model files, WAD archives)
- Demo consistency checks (`crc32_block_sequence`)
- File integrity validation in the filesystem module

All functions are `noexcept`. The C-level step functions allow incremental
hashing without a heap allocation. RAII wrapper classes (`Crc32Hasher`,
`Md5Hasher`) add a chained builder interface.

## CRC32

The CRC32 implementation uses the IEEE 802.3 polynomial (0xEDB88320,
little-endian / reflected) with a standard 256-entry lookup table. This matches
the legacy `CRC32_*` functions in `crclib.c` and the CRC variant baked into the
GoldSrc network protocol.

```cpp
using Crc32 = std::uint32_t;

// Step-by-step API (no allocation)
Crc32 crc32_init  () noexcept;
void  crc32_update( Crc32 &state, const void *data, std::size_t len ) noexcept;
Crc32 crc32_final ( Crc32 state ) noexcept;

// One-shot convenience
Crc32 crc32( const void *data, std::size_t len ) noexcept;
```

The state is a plain `uint32_t`. `crc32_init` sets it to the correct IEEE 802.3
initial value (0xFFFFFFFF before first update; `crc32_final` completes the
`^ 0xFFFFFFFF` finalisation step).

### `crc32_block_sequence`

```cpp
Crc32 crc32_block_sequence(
    std::span<const std::pair<const void *, std::size_t>> blocks ) noexcept;
```

Computes a single CRC32 over a sequence of non-contiguous memory blocks,
accumulated in order. Used by the demo and resource-check subsystems that hash
multiple structs or file regions in one pass without copying them to a
contiguous buffer.

## MD5

```cpp
struct Md5State { /* opaque, 88 bytes */ };

void                         md5_init  ( Md5State &state ) noexcept;
void                         md5_update( Md5State &state,
                                         const void *data, std::size_t len ) noexcept;
std::array<std::uint8_t, 16> md5_final ( Md5State &state ) noexcept;
```

The MD5 implementation follows RFC 1321. `Md5State` is an opaque value type;
it should be default-initialised and then passed to `md5_init` before first use.
`md5_final` consumes the state (applying message padding and length encoding)
and returns the 16-byte digest.

MD5 is used only for legacy protocol compatibility (e.g., half-life network
protocol consistency tokens). It is not used for security-sensitive purposes.

## RAII wrappers

The RAII wrappers accumulate data through chained `.update()` calls and produce
the final digest on `.finalize()`. A static `.hash()` shorthand covers the
common one-shot case.

### `Crc32Hasher`

```cpp
class Crc32Hasher {
public:
    Crc32Hasher() noexcept;

    Crc32Hasher &update( const void *data, std::size_t len ) noexcept;
    Crc32Hasher &update( std::span<const std::byte> data ) noexcept;

    Crc32 finalize() noexcept;

    static Crc32 hash( const void *data, std::size_t len ) noexcept;
};
```

`update` returns `*this` for method chaining:

```cpp
Crc32 checksum = Crc32Hasher{}
    .update( header_data, header_size )
    .update( body_data, body_size )
    .finalize();
```

### `Md5Hasher`

```cpp
class Md5Hasher {
public:
    Md5Hasher() noexcept;

    Md5Hasher &update( const void *data, std::size_t len ) noexcept;
    Md5Hasher &update( std::span<const std::byte> data ) noexcept;

    std::array<std::uint8_t, 16> finalize() noexcept;

    static std::array<std::uint8_t, 16> hash( const void *data, std::size_t len ) noexcept;
};
```

Same chaining pattern as `Crc32Hasher`. Neither wrapper allocates on the heap;
`Md5State` is stored by value as a member.

## ABI note

The engine struct `enginefuncs_t` exposes CRC functions through function
pointers (see `engine/eiface.h`). The shim layer that fills `enginefuncs_t`
will delegate those pointers to `crc32` / `crc32_update` from this module.
The polynomial and output values are stable.

## Thread safety

All functions operate on caller-provided state. There is no shared mutable
state. Separate `Crc32Hasher` or `Md5Hasher` instances are fully independent.
Sharing a single instance across threads without external synchronisation is
undefined behaviour.
