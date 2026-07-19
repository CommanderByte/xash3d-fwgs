# networking Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> C++ standard in use: C++**23** (`xash3dpp/src/networking/CMakeLists.txt`,
> `target_compile_features(xash3dpp_networking PUBLIC cxx_std_23)`; the
> tree-wide `CMAKE_CXX_STANDARD 23`). `std::expected`, `std::span`,
> `std::string_view`, `std::bit_cast`, and `enum class` are used throughout —
> this subsystem was written directly in modern C++, not converted from C.
> Boundary spec: `docs/boundaries/networking-boundary.md`
> Threading: `docs/threading-analysis/networking-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-networking.md` (transport/netchan/
> buffer core), `docs/legacy-survey/deep-dive-delta-encoder.md` (the codec).
> ABI-/wire-frozen surfaces in this subsystem: the packet-header magic numbers
> (`-1`/`-2`/`-3`), the SPLITPACKET / SPLITPACKETGS framing, the netchan
> reliability scheme, the LZSS header + algorithm parameters
> (`window_size = 4096`, `lookahead = 16`, command-byte bit order), the delta
> field tables + both wire dialects, and the packed 20-byte `netadr_t` at every
> frozen-ABI edge. As a **behavioural** ABI, the produced wire bytes for
> protocol 48/49 traffic are frozen — see the wire-exactness prohibition below.

## Summary

networking is a large subsystem (24 TUs, 5 layers) but — like `map_loader` — it
was written directly in modern C++23 and is already **highly modern**. There is
no legacy-C residue to convert: the codec is a value-semantic `MessageBuf` over
a caller-owned buffer (no `sizebuf_t` global, no `MSG_*` free-function family),
errors propagate through `std::expected<T, NetError>` (no `Host_Error`
process-kill on the I/O path), addresses are a `NetAddress` value type that
enforces the `ip6_0[0..1] == 0` invariant and writes to a **caller-owned**
`std::span<char>` (the legacy `NET_AdrToString` static return buffer is gone),
the packet-source `net_from` global was eliminated (OQ-8), and both wire
dialects sit behind `IProtocolDriver` / `IDeltaWireFormat` seams selected
per-channel. `compliance_scan.py networking` is **clean**.

The result is that the usual modernization headline — raw casts over wire data,
C string handling, sentinel error returns, static return buffers —
**does not apply**: those idioms were never introduced, or were designed out at
scaffold time. What remains is a short tail of **cosmetic** items plus one
**load-bearing non-opportunity**: the byte-exact wire-format constraint that
forbids "modernizing" any bit codec or delta width in a way that changes the
bytes on the wire (the exact analog of the `map_loader` Q-18 determinism
caution and the `utilities` studio-math caution).

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that
headlines the utilities / filesystem / cmd_cvar reports is **absent** here — see
the cross-cutting note at the end.

______________________________________________________________________

## The wire-exactness constraint (do NOT "modernize" the bytes)

This is the single most important item and it is a **prohibition**, not an
opportunity. For traffic that identifies as vanilla GoldSrc or Xash3D protocol
48/49, the produced bytes are a **behavioural ABI**: game DLLs, third-party
tools, and real GoldSrc peers parse them. The following files are pinned
byte-/bit-exact to the legacy engine and must **not** be "cleaned up" in any way
that reorders bits, changes a field width, contracts arithmetic, or alters a
sign encoding:

- **The bit codec (`message_buf.cpp`).** The little-endian byte order, the
  bit-packing order in `write_ubit_long` / `read_ubit_long`, the `write_coord`
  1/8-unit fixed-point quantisation, `write_bit_angle`'s `angle * (1<<bits) /
  360` rounding, and the `MSG_ReadStringExt` `'%' → '.'` format-specifier
  defense are all wire-load-bearing. The header comment already pins the
  endianness decision ("swap inline in the implementation, not at call sites"
  if a BE port ever happens). Do not swap the bit order for a "tidier"
  `std::bitset` or reorder the coord scale.
- **The delta field codec (`field_codec.cpp`, `delta_codec.cpp`).** The DT_\*
  field widths, the `SignEncoding` split (GoldSrc sign-magnitude vs Xash two's
  complement — the "GoldSrc broken signed integers" quirk), the ×4000 angle
  premultiply, and the per-field-mark (Xash) vs count+mask (GoldSrc) framing
  are frozen. See `deep-dive-delta-encoder.md` §1–3. A `std::ranges` rewrite of
  the field loop is allowed only if it is proven to emit identical bytes.
- **The split-packet framing (`compat_goldsrc.cpp`, `compat_xash.cpp`).** The
  GoldSrc 4-bit-nibble `packet_id` (max 5 fragments) vs the Xash `short`
  high/low-byte split (up to ~506) are wire discriminators. Do not unify them.
- **The LZSS codec (`compress_lzss.cpp`).** The `LZSS` magic, the 8-byte
  header, `window_size = 4096`, `lookahead = 16`, and the command-byte bit
  ordering are part of the protocol contract (`compress.hpp` says so). The
  `reinterpret_cast<std::uintptr_t>` in the hash bucketing is a masked index
  computation (never dereferenced) and is already SAFETY-annotated — leave it.
- **The OOB / packet-header magic (`oob_packet.cpp`).** The `FF FF FF FF`
  connectionless prefix and the `-2`/`-3` split/compressed discriminators are
  read before netchan is consulted; third-party tools depend on them.

Any future refactor touching these files must re-run the networking wire-format
tests as the acceptance gate, exactly as `map_loader` uses its golden vectors.
The `IProtocolDriver` / `IDeltaWireFormat` seams are the **firewall**: a new or
experimental protocol is a new sibling TU behind the seam, never an edit to the
frozen codecs.

______________________________________________________________________

## Implementation-status table

Every design element the boundary spec calls for is already implemented; there
is no "convert from C" backlog. The table records the modern idioms that are in
place (so a future reader does not regress them) and the short cosmetic tail.

| Design element | Status | Notes |
|----------------|--------|-------|
| Value-semantic `MessageBuf` over caller-owned buffer (no `sizebuf_t` global) | **Implemented** | `std::span<std::byte>`; sticky overflow flag; no exceptions |
| `std::expected<T, NetError>` error propagation | **Implemented** | `Result<T>`; no `Host_Error` kill on the I/O path |
| `NetAddress` value type + invariant, caller-owned `to_string` span | **Implemented** | legacy `NET_AdrToString` static return buffer eliminated |
| `net_from` packet-source global eliminated (OQ-8) | **Implemented** | `from` is a `NetAddress&` out-param — the G-2 NetIO precondition |
| `IProtocolDriver` per-channel wire selection (Q-14) | **Implemented** | GoldSrc (48) + Xash (49) drivers; registry Meyers singleton |
| `IDeltaWireFormat` sibling dialects behind one seam | **Implemented** | Xash mark-bit vs GoldSrc count+mask; `SignEncoding` explicit |
| Link-time compression selection (OQ-7) | **Implemented (LZSS/null)** | `XASH_NET_COMPRESSION`; real **bzip2** deferred (see below) |
| Three-tier `NetworkingStats` (Tier-1 relaxed atomics) | **Implemented** | any-thread read surface; Tier-2/3 compile-gated |
| SAFETY-annotated byte aliasing at wire seams | **Implemented** | every `reinterpret_cast`/`memcpy` carries a SAFETY comment |
| Real bzip2 backend | **Deferred** | `compress_bz2.cpp` TODO until `3rdparty/bzip2` is in CMake |
| Async DNS (`string_to_adr_nb`) | **Deferred** | OQ-4 one-thread model not yet ported (no `dns.cpp`) |
| Netchan pool-allocated `reliable_buf` / fragment queues | **Deferred** | Chunk 7 — currently `vector` stubs (`stub_scan` markers) |

______________________________________________________________________

## Low-priority opportunities

All of these are cosmetic — the current code is correct, `noexcept`, and
wire-exact. They are recorded for completeness, not because anything is wrong.

### L-1: `as_chars(std::span<const std::byte>)` view helper

- **File(s)**: `src/networking/delta/delta_tables.cpp:233` (delta.lst byte
  buffer re-viewed as `char` for text parsing) and `src/networking/netchan.cpp:752`
  (wire byte buffer re-viewed as `char` for a length-bounded filename field);
  both `reinterpret_cast<const char*>` with SAFETY comments.
- **Modernization**: a one-line `[[nodiscard]] inline std::string_view
  as_chars(std::span<const std::byte>) noexcept` would centralise the well-formed
  byte→char aliasing behind one audited seam. Pure readability; identical to the
  `map_loader` L-2 suggestion — a candidate for a **shared** utilities helper if
  Phase 14 consolidates the pattern tree-wide.

### L-2: `field_codec.cpp` typed `read_field<T>` / `write_field<T>`

- **File(s)**: `src/networking/delta/field_codec.cpp:30,37` — `memcpy` in/out of
  a `void* base + offset` at the game-struct ABI offset.
- **Modernization**: the `memcpy` form is the correct, alignment-safe idiom for
  reading a `T` at a byte offset within a foreign struct layout (same reasoning
  as `map_loader`'s `read_record<T>`). `std::bit_cast` does **not** help — the
  source is a slice of a larger object, not a same-sized value. **Verdict: leave
  as memcpy.** Recorded only so a future reader does not "discover" `bit_cast`
  and regress it.

### L-3: `message_buf.cpp` bit-op aliasing helper

- **File(s)**: `src/networking/message_buf.cpp:114,147` — two
  `reinterpret_cast<std::uint8_t&>(data_[byte_idx])` for bit read/write.
- **Modernization**: `std::byte`↔`std::uint8_t` aliasing is well-defined; a tiny
  `byte_ref(idx)` inline accessor would DRY the two sites. Cosmetic; the casts
  are already SAFETY-annotated and correct. Must not change the bit order (see
  the wire-exactness prohibition).

______________________________________________________________________

## Deliberately NOT opportunities

- **Do not replace the `IProtocolDriver` / `IDeltaWireFormat` virtual seams
  with templates.** They are runtime-selected per `netchan_t` (a server speaks
  GoldSrc to one client and Xash to another concurrently), so the polymorphism
  is load-bearing, not incidental. `/GR-` (no RTTI) is respected — these are
  plain virtual dispatch, no `dynamic_cast`.
- **Do not fold the `compress_null.cpp` / `compress_bz2.cpp` link-time pair into
  a runtime `if`.** The OQ-7 link-time selection keeps zero `#ifdef` in the core
  and lets dedicated builds drop the bzip2 dependency entirely.
- **Do not add a networking RNG.** `qport` is caller-supplied
  (`NetchanConfig::qport`) and `LagQueue` takes the caller's drop decision by
  design — keeping randomness out of the subsystem is what makes it
  deterministic and testable. The legacy `Netchan_Init` random-qport global was
  removed on purpose.
- **Do not "modernize" the endianness.** All primitives are little-endian on the
  wire; the header comment pins the decision. A BE port swaps inline in the
  codec, never at call sites.

______________________________________________________________________

## `strnicmp` / `strncmp` string_view over-read — ABSENT

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern
(utilities M-4 / filesystem M-7 / cmd_cvar M-5) is **absent** in networking.
The only string comparisons in the subsystem are `::xash::utilities::strcmp`
(full C-string, NUL-terminated) over **delta field names** — `field.name` /
`func_name` are fixed `char[]` arrays or NUL-terminated literals from
`delta.lst`, guarded with `!name || !name[0]` before use (see
`delta_tables.cpp`, `lst_parser.cpp`, `table_wire.cpp`, `field_codec.cpp`).
There is no bounded-length compare fed a non-NUL-terminated `string_view`.

The candidate the cross-cutting note flags — **info-string / userinfo key
parsing** (`Info_ValueForKey` / `net_api_t::ValueForKey`) — does **not exist in
the shipped code**: the `net_api_t` info-string helper family is deferred (it is
client-DLL-facing and arrives with the client subsystem), so no over-read site
exists here today. When those helpers are ported, they should be written against
the bounded `ci_compare(sv, sv)` the sweep proposes, not the legacy
`Info_ValueForKey` static-buffer form. This is networking's **7th consecutive
negative** data point after platform / core / host / abi / launcher / map_loader
— the pattern is confined to the four early text-heavy subsystems.
