# Delta Encoder

> **Defined in**: `networking/delta.hpp`, `private/networking/delta/*.hpp`,
> `src/networking/delta/*.cpp`\
> **Namespace**: `xash::networking` (satellite folder per Q-11 — no separate
> CMake target)\
> **Legacy reference**: `engine/common/net_encode.c`/`net_encode.h`
> ([deep dive](../../legacy-survey/deep-dive-delta-encoder.md));
> deviations: [networking-boundary.md](../../boundaries/networking-boundary.md)
> "Delta encoder — implementation notes" + "Known Deviations"

## Overview

The delta encoder is the wire codec for entity/usercmd/weapon/event/movevars
state: it serialises only the fields that changed relative to a baseline,
using per-field descriptor tables loaded from `delta.lst` (or the built-in
verbatim fallbacks). It is byte-exact with legacy for both wire dialects and
is the primary dependency the Chunk 6 server consumes (entity updates,
`Delta_AddEncoder` game-DLL hook, baselines).

## Structure

| Piece | Role |
|-------|------|
| `DeltaTables` (public, pimpl) | Table registry + lifecycle (`init(Filesystem&)` loads `delta.lst`, `init_from_script`, `init_client`, `clear`), field set/unset/find, encoder registration, all 12 struct codecs, `test_baseline`, `stats()` |
| `delta_types.hpp` | Wire-frozen DT_* flag values, descriptor bit widths, `q_rint` (float AND double overloads — parity-critical) and `q_equal` (±0.001f) |
| `field_defs.hpp` | The 7 legacy field-definition tables, vendored verbatim via `XASH_DELTA_DEF` macros |
| `field_codec.{hpp,cpp}` | Per-field bit codec with the exact legacy conversion chains; `SignEncoding` selects two's-complement (normal) vs sign-magnitude (GoldSrc `iAlternateSign` brackets) |
| `wire_format.hpp` + `wire_format_{xash,goldsrc}.cpp` | **`IDeltaWireFormat` seam**: Xash = per-field mark bit; GoldSrc = 3-bit changed-group count (last-touched-group+1, NOT popcount) + mask bytes + payloads. `delta_wire_format_for(DeltaTableSet)` picks per protocol driver; a future wire dialect implements the interface as a sibling (Q-14) |
| `lst_parser.{hpp,cpp}` | `delta.lst` tokenizer incl. the trailing-comma backtrack quirk |
| `table_wire.cpp` | `svc_deltatable` descriptor wire (tableIndex 4 / nameIndex 8 / flags 10 / bits−1 5, null-compressed multipliers) + the GoldSrc meta-table parse (×4000 premultiply, DT_SIGNED_GS bit31 → DT_SIGNED bit8 remap, byte-align after parse) |
| `delta_codec.cpp` | Entity header walk (number 13 bits, removeType 2, baseline 1+SBit7, entityType 1+UBit2), count_fields helpers, MAX_WEAPON_BITS 6 |

## Seams (deliberate flexibility points)

- **`IDeltaWireFormat`** — the field-mask dialect; new formats slot in
  without touching codecs (the user-mandated "future wire format" option).
- **`IBaselineResolver`** — replaces the legacy client globals for baseline
  lookup; the server's write path only passes a caller-computed offset.
- **svc command bytes are caller-supplied** for most codecs (the server owns
  message framing) — see the boundary doc for the two self-owned exceptions.
- **`register_encoder` / `Delta_AddEncoder`** — the game-DLL custom-encoder
  hook; the Chunk 6 extern-C shim forwards `DeltaField*` tokens (layout is
  NOT legacy `delta_s` — flagged by the ABI watchdog, recorded in the
  boundary doc).

## Threading model

`DeltaTables` mutation (init/register/set/unset) is main-thread; encode/
decode over initialised tables is read-only per the hazard rows in
[networking-threading.md](../../threading-analysis/networking-threading.md).
`stats()` is the thread-safe observation point (Tier-1 relaxed atomics).

## Verification posture

Adversarially parity-audited 2026-07: byte-exact on both dialects, incl. an
exhaustive 3.4M-value angle-codec scan; two real divergences were found and
fixed during the audit (`MSG_ReadString` '%'→'.' sanitisation; the GoldSrc
sign-magnitude discovery). Golden-vector suites live under
`tests/networking/delta/`.

## See also

- [protocol-driver.md](./protocol-driver.md) — `DeltaTableSet` selection
- [wire-encoding.md](./wire-encoding.md) — the bit-level substrate
- [Deep dive](../../legacy-survey/deep-dive-delta-encoder.md) — legacy
  algorithms, constants, quirks
