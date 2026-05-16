# MessageBuf

> **Defined in**: `networking/message_buf.hpp`  
> **Source**: `src/networking/message_buf.cpp`  
> **Namespace**: `xash::networking`

## Overview

`MessageBuf` is a **bit-level read/write buffer** over an externally-owned
byte span. It replaces the legacy `sizebuf_t` struct plus its `MSG_*` free
function family. The class owns no storage and is safe to instantiate per
call site or on the stack — there are no globals.

The same instance supports both reading and writing (matching legacy
`MSG_StartReading` / `MSG_StartWriting`). Mixing reads and writes within one
message is the caller's responsibility; the class does not enforce direction.

### Overflow handling

Writes past the buffer end set a **sticky overflow flag** and silently discard
subsequent bytes. Reads past the end set the same flag and return zero/empty.
Callers must check `overflowed()` before trusting the result of any I/O
sequence. No exceptions are thrown, no logging occurs — the flag is the signal.

---

## MessageBuf

### Fields / members

| Name | Type | Role |
|------|------|------|
| `data_` | `std::byte *` | Pointer to caller-owned storage |
| `num_bits_` | `std::size_t` | Total capacity in bits (`capacity_bytes * 8`) |
| `cur_bit_` | `std::size_t` | Current read/write cursor in bits |
| `overflow_` | `bool` | Sticky flag set on any out-of-range access |
| `name_` | `const char *` | Debug label (not copied; must be a string literal or outlive `MessageBuf`) |

### Key operations

**Lifecycle**:
- Default constructor — produces an empty/unusable buffer (null `data_`, zero capacity)
- `MessageBuf(span, name)` — binds to caller-owned storage
- `rebind(span, name)` — re-targets to a different buffer; resets cursor and overflow
- `reset()` — rewinds cursor to bit 0 and clears overflow; does not change the bound buffer

**Accessors**:
- `num_bits_written()` → current cursor in bits
- `num_bytes_written()` → cursor rounded up to the next byte
- `real_bytes_written()` → cursor divided by 8 (no rounding)
- `num_bits_left()` / `num_bytes_left()` → remaining capacity
- `max_bits()` / `max_bytes()` → total capacity
- `data()` → `std::span<std::byte>` over the full bound buffer (mutable or const overload)
- `overflowed()` → sticky flag
- `tell_bit()` → current cursor position

**Seek**:
- `seek_to_bit(bit, origin)` → `bool` — repositions the cursor; returns `false` (and sets no overflow) for out-of-range requests. `SeekOrigin::Begin`, `Current`, `End`.

**Bit-level writes**:
- `write_one_bit(int)` — writes the LSB of the argument
- `write_bits(std::uint32_t, count)` — writes `count` bits (1–32)

**Byte-aligned writes** (all with implicit alignment to byte boundary first):
- `write_byte`, `write_char`, `write_short`, `write_word`, `write_long`, `write_float`
- `write_string(string_view)` — null-terminated string
- `write_bytes(span)` — raw block copy

**Bit-level reads**:
- `read_one_bit()` → `int`
- `read_bits(count)` → `std::int32_t`

**Byte-aligned reads**:
- `read_byte`, `read_char`, `read_short`, `read_word`, `read_long`, `read_float`
- `read_string()` → `std::string_view` aliasing the buffer (valid until buffer is reused)
- `read_bytes(span)` — read into caller-provided span

**GoldSrc coordinate extensions** (matching legacy `MSG_ReadCoord` / `MSG_WriteAngle`):
- `write_coord`, `read_coord` — encode a float as a fixed-point integer
- `write_angle`, `read_angle` — encode a float as an 8-bit byte angle
- `write_angle_hires`, `read_angle_hires` — 16-bit high-resolution angle
- `write_vec3_coord`, `read_vec3_coord` — three consecutive coord fields

### Lifecycle / ownership

`MessageBuf` is a value type (moveable, copyable). The underlying storage is
**not owned** — callers are responsible for ensuring the bound `std::span`
remains valid for the lifetime of the `MessageBuf`.

Typical usage pattern:

```cpp
std::byte buf[MAX_MSGLEN];
xash::networking::MessageBuf msg{ buf };
msg.write_byte( SVC_PRINT );
msg.write_string( "hello" );
send_packet( sock, msg.data().subspan( 0, msg.num_bytes_written() ), to );
```

---

## Threading model

`MessageBuf` has no internal locks and is not thread-safe. A single instance
must be used from one thread at a time. Multiple independent instances may
exist on different threads without interference (they share no state).

## Error handling

Overflow sets the sticky `overflow_` flag. Calling code must check this flag
after any sequence of reads/writes before acting on the result. There is no
way to resume a corrupted `MessageBuf` short of calling `rebind` or `reset`.

## Endianness

All multi-byte primitives are little-endian on the wire. No big-endian
adaptation exists; GoldSrc/Xash targets are universally little-endian. If a
big-endian port is ever needed, swaps belong inside `message_buf.cpp`, not at
call sites.

## Edge cases and invariants

- Writing zero bits is a no-op; reading zero bits returns 0 without advancing
  the cursor.
- `num_bytes_written()` rounds up: writing 1 bit reports 1 byte written.
  Use `real_bytes_written()` when you need the actual filled byte count.
- `read_string()` returns a `string_view` into the buffer — the view is only
  valid while the buffer is unchanged.
- The cursor tracks bit position even when all writes are byte-aligned; the
  legacy `MSG_StartBitWriting` / `MSG_EndBitWriting` dance is handled
  transparently.

## See also

- [wire-encoding.md](./wire-encoding.md) — wire header constants sit above MessageBuf
- [context-lifecycle.md](./context-lifecycle.md) — `NetworkContext` will use `MessageBuf` for netchan frames (Layer 3)
- Legacy: `engine/common/net_buffer.{c,h}` — `sizebuf_t`, `MSG_*` functions
