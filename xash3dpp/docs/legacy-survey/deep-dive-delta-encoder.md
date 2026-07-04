# Deep Dive: Legacy Delta Encoder (`net_encode.c`)

*RETROACTIVE brief, reconstructed 2026-07-04 after the fact.* The xash3dpp
delta encoder (Chunk 2-4 networking) was implemented **before** this repo
adopted the convention that recon agents commit a `deep-dive-*.md` ahead of
implementation. This document reconstructs what that recon would have said,
derived from the legacy sources below and cross-checked against the shipped
`xash3dpp/src/networking/delta/` implementation and
`xash3dpp/docs/boundaries/networking-boundary.md`. Line numbers are against
the working tree on 2026-07-04; treat as behaviour reference, not a design
constraint. Scope: **Chunk 6 (server)**, which consumes the delta encoder
heavily via `Delta_AddEncoder`, entity deltas, and usercmd deltas — read this
instead of re-deriving from `net_encode.c`.

Legacy sources surveyed: `engine/common/net_encode.c` (2374 lines),
`engine/common/net_encode.h`, `engine/common/net_buffer.c` / `.h`,
`public/xash3d_mathlib.h`, `engine/common/protocol.h`,
`common/entity_state.h`.

---

## 0. Headline findings (read first)

1. **There is exactly ONE delta engine.** All eight built-in struct
   descriptions (`event_t`, `movevars_t`, `usercmd_t`, `clientdata_t`,
   `weapon_data_t`, `entity_state_t`, `entity_state_player_t`,
   `custom_entity_state_t`) share one generic field-descriptor interpreter
   (`Delta_WriteField_`/`Delta_ReadField_`, net_encode.c:1228-1472). There is
   no per-struct hand-written codec in legacy C — Chunk 6 should not expect
   one either; the xash3dpp port already generalised this into
   `field_codec.hpp` + `IDeltaWireFormat`.
2. **Two wire framings coexist, selected per-connection, not per-struct.**
   The Xash native framing marks every field with a 1-bit "changed" flag
   inline (`Delta_WriteField`/`Delta_ReadField`, :1311-1324, :1474-1484). The
   GoldSrc framing instead sends a 3-bit count of changed *byte-groups*
   (8 fields each) followed by that many mask bytes, then only the flagged
   fields' payloads (`Delta_WriteGSFields`/`Delta_ParseGSFields`,
   :1486-1549). **The changed-group count is `last_changed_group + 1`, NOT a
   popcount of set groups** — see §1.
3. **Signed-field wire layout differs by framing, not by field.** Legacy
   `MSG_WriteSBitLong`/`MSG_ReadSBitLong` branch on `sizebuf_t::iAlternateSign`
   (net_buffer.c:354-377, :678-698): GoldSrc's `Delta_ParseTableField_GS`
   brackets its meta-table reads in `MSG_StartBitWriting`/`MSG_EndBitWriting`
   (net_encode.c:2129-2146), which increments `iAlternateSign` and flips
   every signed field in that bracket to **sign-bit-first + magnitude**
   ("GoldSrc broken signed integers"). Xash's own path never sets
   `iAlternateSign` and stays plain two's complement. xash3dpp's
   `SignEncoding` enum (`field_codec.hpp`) makes this an explicit parameter
   instead of a global-flag side effect — the GS wire format sibling always
   passes `SignMagnitude`.
4. **`Delta_AddEncoder`/custom encode is a conditional per-write mask, not a
   filter on which fields exist.** Every write call re-activates all fields
   then invokes the registered callback (`Delta_CustomEncode`,
   net_encode.c:497-509), which may call `Delta_UnsetField*` to suppress
   specific fields for *this* write only. Static entities (`DELTA_STATIC`)
   explicitly bypass the callback and force all fields active
   (:1977-1987). Chunk 6's ABI shim only needs to forward the function
   pointer — see §5.
5. **Baseline resolution is entirely client-side state that the encoder just
   indexes by a signed 7-bit wire offset.** The write side emits an
   already-computed offset (:1944-1949); the read side resolves it against
   one of three client-global arrays depending on sign and `delta_type`
   (:2052-2069). xash3dpp cuts this at `IBaselineResolver` (delta.hpp:111-118)
   — the codec never touches client globals. Chunk 6 (server) is the writer
   side only; it computes `baseline` via `Delta_TestBaseline`-style bit
   counting and passes the result in, it does not need the resolver.
6. **`entityType` is 2 bits, not part of the generic field loop.** It is
   written by `MSG_WriteDeltaEntity` directly (before the field loop) only
   when changed or `force` (:1951-1957), and gates which of the three entity
   delta tables is used (`ENTITY_BEAM` → custom table, else player/normal by
   `delta_type`). This lookup happens **twice per side** (encode: :1959-1970;
   decode: :2079-2090) — table selection depends on the *decoded* value on
   read, so a corrupted/attacker-controlled `entityType` bit can select a
   different table than the sender intended. Legacy does not guard against
   this; xash3dpp's `entity_table_for` (delta_codec.cpp:313-321) reproduces
   the same trust assumption.
7. **Parity-audited 2026-07**: a legacy-parity pass on the shipped
   implementation found and fixed a `MSG_ReadString`-family '%'→'.'
   sanitization gap (commit `c7216327`) and discovered the GoldSrc
   sign-magnitude divergence documented in point 3 above (commit
   `0fe1a6d2`) before it would have silently produced wire-incompatible
   GS delta streams. See §9 and `networking-boundary.md`'s "Known
   Deviations" for the full list — not duplicated here.

---

## 1. Where delta encoding sits in the protocol

Delta encoding is invoked in two contexts, both wire-frozen:

- **Full struct diffs sent inline** in a game-protocol message the *caller*
  already framed with its own `svc_*`/`clc_*` command byte — the delta
  functions themselves never write a command byte for these:
  `MSG_WriteDeltaUsercmd` (client→server, no command byte at all — the
  usercmd stream is its own message type), `MSG_WriteDeltaEvent`,
  `MSG_WriteClientData`, `MSG_WriteWeaponData`, `MSG_WriteDeltaEntity` (used
  inside `svc_packetentities`/`svc_deltapacketentities` bodies, protocol.h:62-63).
  **Exception:** `MSG_WriteDeltaMovevars` writes its own `svc_deltamovevars`
  (protocol.h:66, value 44) command byte internally via
  `MSG_BeginServerCmd` (net_encode.c:1698) — this is the one struct codec
  that owns its message framing.
- **Table *descriptions* sent once at connect**, via
  `Delta_WriteDescriptionToClient` → `Delta_WriteTableField`
  (net_encode.c:593-629), which writes its own `svc_deltatable` command byte
  (protocol.h:36, value 14) per field, and is parsed field-by-field on the
  client by `Delta_ParseTableField` (:631-675, Xash framing) or as one whole
  GoldSrc table via `Delta_ParseTableField_GS` (:2111-2147, string name +
  short field count + GS-framed goldsrc_delta_t records).

**Who owns the svc command byte**: for every struct codec except
`MSG_WriteDeltaMovevars` and `Delta_WriteTableField`, the *caller* (the
higher-level protocol writer, e.g. `SV_EmitPacketEntities`) writes the
enclosing command byte; the delta function only serialises the payload
after it. xash3dpp's seam shift documents this exactly
(networking-boundary.md, "Command bytes are caller-supplied") — 
`DeltaTables::write_description`/`write_delta_movevars` take the raw command
value as a parameter because xash3dpp has no `svc_*` enum yet (it arrives
with the game-protocol layer, i.e. Chunk 6+).

**Per-field-mark (Xash) vs count+mask (GoldSrc) wire, exact shapes:**

Xash (`Delta_WriteField`/`Delta_ReadField`, net_encode.c:1311-1324,
1474-1484), used by every struct codec above via the field loop:
```
for each field in table:
    write_one_bit( changed ? 1 : 0 )
    if changed: write payload (Delta_WriteField_)
```

GoldSrc (`Delta_WriteGSFields`/`Delta_ParseGSFields`, :1486-1549), used only
via `Delta_Write/ReadGSFields` (the GS batch codec — table description
records AND, per xash3dpp's `IDeltaWireFormat` selection axis, an alternate
struct-codec framing selected by `IProtocolDriver::delta_tables()`):
```
bits[8] = {0}                              # one bit per field, 8 fields/byte
for each field i in table:
    if changed(field[i]):
        b = i >> 3;  n = 1 << (i & 7)
        set bit n in bits[b]
        c = b + 1                          # NOTE: last touched group index + 1
write_ubit_long( c, 3 )                     # 3-bit group COUNT, not popcount
for i in 0..c-1: write_byte( bits[i] )       # c whole mask bytes, always written
for each field i in table:
    if bit n of bits[b] set: write payload (Delta_WriteField_)
```
**Critical quirk**: `c` is `last_group_touched + 1`, so groups *before* the
last touched one are always sent (even if all-zero) but groups *after* are
omitted entirely. A single changed field in group 7 forces all 8 mask bytes
onto the wire. `Delta_ParseTableField_GS`'s `numFields > maxFields` bounds
check (:2126-2127) is a `Host_Error` in legacy; xash3dpp's `parse_table_gs`
turns this into a logged failure (Q-5).

---

## 2. `DT_*` flag values and wire-frozen bit widths

`DT_*` type flags (net_encode.c:29-38 — bit flags, combinable with `DT_SIGNED`):

| Flag | Value | Meaning |
|---|---|---|
| `DT_BYTE` | `BIT(0)` = 1 | 1-byte field |
| `DT_SHORT` | `BIT(1)` = 2 | 2-byte field |
| `DT_FLOAT` | `BIT(2)` = 4 | float, scaled to integer by `multiplier` |
| `DT_INTEGER` | `BIT(3)` = 8 | 4-byte integer |
| `DT_ANGLE` | `BIT(4)` = 16 | float angle, bit-angle codec (never multiplier-scaled) |
| `DT_TIMEWINDOW_8` | `BIT(5)` = 32 | float timestamp relative to `timebase`, fixed ×100 scale |
| `DT_TIMEWINDOW_BIG` | `BIT(6)` = 64 | float timestamp relative to `timebase`, `multiplier` scale |
| `DT_STRING` | `BIT(7)` = 128 | NUL-terminated string, sent as raw bytes via `MSG_WriteString` |
| `DT_SIGNED` | `BIT(8)` = 256 | sign modifier (applies to BYTE/SHORT/INTEGER/FLOAT) |
| `DT_SIGNED_GS` | `BIT(31)` | GoldSrc-wire-only sign modifier; remapped to `DT_SIGNED` at parse time, never stored |

`DT_SIGNED_GS` → `DT_SIGNED` remap (net_encode.c:2137-2142): after reading a
GS table-description field through the immutable meta-table, if
`fieldType & DT_SIGNED_GS` is set, clear bit 31 and set bit 8 before calling
`Delta_AddField`. xash3dpp keeps `k_dt_signed_gs = 1u<<31` only as a
documentation/parse-time constant (`delta_types.hpp:36`); it never appears
in a stored `DeltaField::flags`.

Wire-frozen bit widths (all counted in the `svc_deltatable` descriptor
message and/or struct-codec framing):

| Field | Bits | Legacy site |
|---|---|---|
| `tableIndex` | 4 | net_encode.c:610, :640 (assumes ≤16 tables) |
| `nameIndex` | 8 | :611, :645 (assumes ≤255 fields/struct) |
| field `flags` | 10 | :612, :656 |
| `bits - 1` | 5 | :613, :657 (max receivable value 32) |
| entity number | 13 (`MAX_ENTITY_BITS`) | protocol.h:119; net_encode.c:1923, :1941, :2025 wait no — read at :2025 is removeType; number is read by the *caller* before `MSG_ReadDeltaEntity` per its doc comment (:2004) |
| `removeType` | 2 | :1932, :1942, :2025 |
| baseline-present flag + offset | 1 + `SBitLong(7)` | :1946-1949 (write), :2049-2050 (read) |
| entityType-present flag + value | 1 + `UBitLong(2)` | :1953-1957 (write), :2075-2076 (read) |
| `MAX_WEAPON_BITS` (weapon index) | 6 | protocol.h:106; net_encode.c:1852 |
| GS changed-group count | 3 | :1493, :1537 |

Note on `entity number`: `MSG_WriteDeltaEntity` writes it itself
(`MSG_WriteUBitLong( msg, to->number, MAX_ENTITY_BITS )`, :1941, and the
remove-path write at :1923). `MSG_ReadDeltaEntity`'s doc comment states "The
entity number has already been read from the message" (:2003) — the
*caller* reads the 13-bit number before invoking the read function; only
`removeType` (2 bits) is read first inside `MSG_ReadDeltaEntity` itself
(:2025). xash3dpp's `ReadDeltaEntityParams::number` (delta.hpp:135)
mirrors this: the dispatcher reads the 13 bits and passes the decoded value
in.

`MAX_ENTITY_BITS = 13` (protocol.h:119, 8192 edicts); Xash also defines
`MAX_GOLDSRC_EDICTS = BIT(13) + MAX_CLIENTS*15` (protocol.h:315) for the
GoldSrc edict-count ceiling — the wire field width is unchanged, only the
legal value range differs by protocol.

---

## 3. `delta.lst` script grammar, trailing-comma quirk, movevars fallback, field tables

### 3.1 Grammar

Per-section grammar (`Delta_InitFields`/`Delta_ParseTable`/`Delta_ParseField`,
net_encode.c:869-908, :817-867, :677-815):

```
<struct_name> <encodeDll: none|gamedll|clientdll> [<encodeFunc>]
{
    DEFINE_DELTA( <field>, <FLAG[|FLAG...]>, <bits>, <multiplier> )
    DEFINE_DELTA_POST( <field>, <FLAG[|FLAG...]>, <bits>, <multiplier>, <post_multiplier> )
    ...
}
```

- `<struct_name>` resolved via `Delta_FindStruct` (case-insensitive
  `Q_stricmp`, :437-454) against the eight built-in table names.
- `encodeDll` literal `"none"` skips the third token entirely
  (`Delta_InitFields`, :892-894: `encodeFunc` is hard-set to `"null"` and the
  parser does NOT consume a token for it) — **grammar is context-sensitive**:
  `none` sections are `<struct> none { ... }` (two tokens before `{`),
  everything else is `<struct> gamedll|clientdll <funcname> { ... }` (three
  tokens).
- Field flag tokens accept `|` as a no-op separator (net_encode.c:723-724:
  `if( !Q_strcmp( token, "|" )) continue;`) — `DT_BYTE|DT_SIGNED` and
  `DT_BYTE | DT_SIGNED` parse identically; unrecognised flag tokens are
  silently ignored (no `else` branch after the `DT_SIGNED` check, :742-744).
- `DEFINE_DELTA_POST` differs from `DEFINE_DELTA` only in reading one more
  comma + token (`post_multiplier`) before the closing `)`
  (`Delta_ParseField`'s `bPost` argument, :677, :777-799). Plain
  `DEFINE_DELTA` hard-codes `post_multiplier = 1.0f` (:797-799) — this is why
  `Q_equal(post_multiplier, 1.0f)` is checked before applying it on read
  (net_encode.c:1397, :1411, :1425, :1444).

### 3.2 Trailing-comma quirk

After the closing `)` of a field definition, the parser optimistically
tries to consume one more `,` token and, if it isn't a comma, **rewinds the
cursor** (`Delta_ParseField`, :809-813):

```c
oldpos = *delta_script;
*delta_script = COM_ParseFile( *delta_script, token, sizeof( token ));
if( token[0] != ',' ) *delta_script = oldpos; // not a ','
```

This means a `delta.lst` file may separate consecutive `DEFINE_DELTA(...)`
lines with **or without** a trailing comma — both parse identically, and a
trailing comma is silently discarded rather than treated as a syntax error.
This is a real authoring convenience some hand-written `delta.lst` files
rely on; a strict re-implementation that treats a stray `,` as an error
would reject valid legacy scripts.

### 3.3 Movevars fallback

If the `delta.lst` script does not contain a `movevars_t` section (i.e. the
table is still uninitialized after `Delta_InitFields`), `Delta_Init`
(net_encode.c:910-969) hard-codes 27 fields via direct `Delta_AddField`
calls (gravity, stopspeed, maxspeed, spectatormaxspeed, accelerate,
airaccelerate, wateraccelerate, friction, edgefriction, waterfriction,
bounce, stepsize, maxvelocity, zmax, waveHeight, skyName, footsteps,
rollangle, rollspeed, skycolor_r/g/b, skyvec_x/y/z, wateralpha,
fog_settings — in that exact order), then reasserts
`dt->numFields = ARRAYSIZE( pm_fields ) - 4` (:965) — i.e. `31 - 4 = 27`,
excluding `skydir_x/y/z` and `skyangle` from the networked set (they exist
in `pm_fields` as script-overridable slots but are never auto-added). Note
`zmax` is deliberately **unsigned** 24-bit (a1ba comment, :942-949): mods
that push `sv_zmax` high for 3D skyboxes would otherwise clamp at the
16-bit signed max. xash3dpp's `apply_movevars_fallback` (delta_tables.cpp:
160-208) reproduces the field list, order, and the 27-count assertion
verbatim (as an `XASH_ASSERT` consistency check rather than a truncating
assignment — see Known Deviations).

### 3.4 The seven verbatim field-def tables

All defined in net_encode.c via helper macros (`UCMD_DEF`, `PHYS_DEF`,
`EVNT_DEF`, `CLDT_DEF`, `WPDT_DEF`, `ENTS_DEF`, `DESC_DEF` — each expands to
`{ "name", offsetof(struct,field), sizeof(((struct*)0)->field) }`):

| Table | Legacy array | Legacy struct | Field count | xash3dpp mirror |
|---|---|---|---|---|
| `cmd_fields` | net_encode.c:55-73 | `usercmd_t` | 16 | `k_cmd_fields` (field_defs.hpp:41-59) |
| `pm_fields` | :75-108 | `movevars_t` | 31 | `k_pm_fields` (:66-99) |
| `ev_fields` | :110-130 | `event_args_t` | 18 | `k_ev_fields` (:106-126) |
| `wd_fields` | :132-156 | `weapon_data_t` | 22 | `k_wd_fields` (:133-157) |
| `cd_fields` | :158-216 | `clientdata_t` | 56 | `k_cd_fields` (:164-222) |
| `ent_fields` | :218-311 | `entity_state_t` | 91 | `k_ent_fields` (:230-323, shared by all three entity tables) |
| `meta_fields` | :313-322 | `goldsrc_delta_t` | 7 | `k_meta_fields` (:332-342) |

All seven counts are pinned by `static_assert` in
`xash3dpp/include/xash3dpp/private/networking/delta/field_defs.hpp`. Legacy
quirks preserved verbatim in the xash3dpp field-identity tables: `usercmd_t`
`impact_index`/`impact_position[0..2]` alias `reserved[0..3]`
(net_encode.c:69-72; `UCMD_DEF_` macro renames the wire field name while
pointing at the `reserved[]` storage); `movevars_t` omits `entgravity` and
`features` from the networked set entirely (never appear in `pm_fields`);
all vector fields are per-component entries (`"origin[0]"`, not a 3-float
blob).

`dt_info[]` table-index assignment (net_encode.c:360-373) is an enum-indexed
designated-initializer array — **the enum order in `net_encode.h:26-47`
(`DT_EVENT_T=0 .. DT_CUSTOM_ENTITY_STATE_T=7`) IS the wire `tableIndex`**.
xash3dpp's `DeltaStructId` (delta.hpp:42-53) preserves the same order with
an explicit "never reorder" comment.

---

## 4. Field codec math

All arithmetic lives in `Delta_CompareField` (net_encode.c:1050-1162),
`Delta_WriteField_` (:1228-1309), `Delta_ReadField_` (:1381-1472), and
`Delta_CopyField` (:1332-1371). xash3dpp's equivalent is
`field_codec.cpp`/`.hpp` (`compare_field`, `write_field_payload`,
`read_field_payload`, `copy_field`).

- **Integer types (`DT_BYTE`/`DT_SHORT`/`DT_INTEGER`)**: read raw as
  `int8_t`/`uint8_t` etc. per `DT_SIGNED`, multiply by `multiplier` **only
  if** `!Q_equal(multiplier, 1.0f)` (the null-compression check —
  avoids float rounding noise on the common case), then
  `Delta_ClampIntegerField` (:1022-1040) clamps into `[-(2^(bits-signbit-1)),
  2^(bits-signbit-1)-1]` for signed or `[0, 2^bits-1]` for unsigned, **before**
  writing via `MSG_WriteBitLong( msg, iValue, bits, signbit )`. On read the
  same clamp-derived range is implicit in the bit width; the value is
  divided by `multiplier` then multiplied by `post_multiplier` (both
  null-compressed the same way) before store.
- **`DT_FLOAT`**: write converts `(int)((double)flValue * multiplier)` — note
  the **double** intermediate (:1278) even though the field is a float — then
  clamps and writes as a plain integer (no explicit sign check beyond the
  clamp call; `DT_SIGNED` still selects the clamp's signed range). Read:
  reverse (`iValue / multiplier`, then `* post_multiplier`, both
  null-compressed). Comparison (`Delta_CompareField`, :1131-1136) is a raw
  **bit-pattern** compare of the two floats reinterpreted as `int` — NOT a
  value compare, so `-0.0f` and `0.0f` compare unequal (a change is
  detected) even though they'd encode identically. This is a real quirk:
  the "changed" decision uses bit identity, but the actual encoded payload
  after `multiplier`/rounding could still coincide for numerically-close-but
  bit-different floats (no false negative there, only the reverse: a
  bit-identical value never triggers a wasted write).
- **`DT_ANGLE`**: **never applies `multiplier`** ("NOTE: never applies
  multipliers to angle because result may be wrong on client-side",
  net_encode.c:1286-1287). Uses `MSG_WriteBitAngle`/`MSG_ReadBitAngle`
  (net_buffer.c:428-441, :664-675): `d = (int)((fmod(angle,360)+360*(<0)) *
  (1<<bits) / 360.0f) & mask`; the read side reconstructs
  `i * (360.0f/(1<<bits))` and re-normalizes into `[-180, 180]`. Compare path
  (`Delta_CompareField`, :1131) treats `DT_ANGLE` identically to `DT_FLOAT`
  (raw bit-pattern compare) even though the wire encoding is completely
  different math — so two angles that quantize to the same wire value but
  have different raw float bit patterns are still (correctly, conservatively)
  treated as "changed".
- **`DT_TIMEWINDOW_8`**: fixed `×100.0` scale baked into the code (not
  `multiplier`). Write: `dt = Q_rint((timebase - flValue) * 100.0)`, clamped
  signed, `MSG_WriteSBitLong`. Read: `flTime = (timebase*100.0 -
  (int)iValue) / 100.0`. Compare: `Q_rint(val*100.0)` both sides, integer
  compare.
- **`DT_TIMEWINDOW_BIG`**: same shape but scale is the field's `multiplier`
  (test harness uses 1000.0f, net_encode.c:2293). Both timewindow types
  always write **signed** (`MSG_WriteSBitLong`/`Delta_ClampIntegerField(...,
  1, ...)`, :1294, :1301) regardless of the field's `DT_SIGNED` flag — the
  delta from `timebase` can be negative even for a conceptually "unsigned"
  timestamp field.
- **`DT_STRING`**: write is `MSG_WriteString` (raw bytes, NUL-terminated, no
  format-spec sanitization on the writer side — see §9). Read is
  `MSG_ReadString` then `Q_strncpy` truncated to `pField->size` (the
  compile-time `sizeof` of the struct member). Compare is `Q_strcmp == 0`.
  `Delta_TestBaseline`'s bit-cost estimate for a changed string field is
  `strlen(to) * 8` (:1211) — no `+1` for the NUL, an estimate only (actual
  wire cost is `strlen+1` bytes via `MSG_WriteBytes`, net_buffer.c:535-544).
- **`Q_rint`** (`xash3d_mathlib.h:92`): `#define Q_rint(x) ((x)<0.0f ?
  ((int)((x)-0.5f)) : ((int)((x)+0.5f)))` — half-away-from-zero via ±0.5
  truncation, **not** round-to-even. This is a macro, so at
  `DT_TIMEWINDOW_BIG` write (`Q_rint((timebase - flValue) * multiplier)`)
  the subtraction happens in `double` (both operands are `double`/`float`
  promoted) but the macro's `(x)-0.5f`/`+0.5f` literals are `float` — if the
  compiler doesn't promote consistently this can differ from an all-double
  `Q_rint`. xash3dpp's `delta_types.hpp:71-82` ships **both** a `double` and
  a `float` overload of `q_rint` specifically to preserve this
  precision-width-dependent behaviour rather than picking one width.
- **`Q_equal`** (`xash3d_mathlib.h:70,87-88`): `EQUAL_EPSILON = 0.001f`;
  `Q_equal(a,b)` is `a >= b-e && a <= b+e`. Used exclusively as the
  null-compression gate for `multiplier`/`post_multiplier` (never for the
  actual field-value comparison, which is always bit-pattern or quantized
  integer as described above).

---

## 5. `Delta_AddEncoder` / custom encoder hook

**Registration flow (legacy)**: the game DLL (or client DLL) calls
`Delta_AddEncoder( name, encodeFunc )` (net_encode.c:2177-2197) — typically
at `ServerActivate`/`ClientActivate` time. It resolves the table by
`funcName` (set from the `delta.lst` third token,
`Delta_ParseTable`:850-851), refuses if that table wasn't declared
`gamedll`/`clientdll` in the script (`customEncode == CUSTOM_NONE` check,
:2189-2193 — this is the `dt->customEncode` value set by
`Delta_ParseTable`:853-858 from the second script token), then stores the
raw function pointer as `dt->userCallback`. No unregister path exists;
`Delta_Shutdown` (:990-1013) clears `userCallback` for every table on
reset.

**Invocation**: every struct-write entry point calls `Delta_CustomEncode`
(:497-509) immediately before the field loop: it force-activates every
field (`bInactive = false`), then — if a callback is registered — calls
`userCallback( pFields, from, to )`, handing the game DLL the **entire
mutable field array** of that table. The callback is expected to call
`Delta_UnsetField`/`Delta_UnsetFieldByIndex` on the fields it wants
suppressed for this particular write. This means: (a) the callback mutates
shared per-table state (`bInactive`) that is reset before every call, so
there's no cross-call leakage; (b) the callback receives `delta_s*` (an
opaque-to-mods pointer in practice, since HLSDK never dereferences it
directly — it only calls `Delta_Find/Set/UnsetField*`).

**Condition zeroing semantics**: "condition zeroing" here means
`Delta_CustomEncode` unconditionally resets **all** `bInactive` flags to
`false` at the top of every call (:504-505) before invoking the callback —
so a field suppressed on a previous write is active again by default on the
next, and the callback must re-suppress it every time if it wants
persistent suppression. `DELTA_STATIC` entity writes bypass the callback
path entirely and set all fields active without ever consulting
`userCallback` (net_encode.c:1977-1987) — static entities are never
custom-encoded even if the table has a registered callback.

**xash3dpp's function-pointer token-forwarding plan**: `DeltaEncodeFn` is
declared as a **raw C function pointer** (`delta.hpp:98-100`,
`void(*)(DeltaField*, const uint8_t*, const uint8_t*)`), deliberately not
`std::function` or any wrapped type, specifically so the eventual
`enginefuncs_t`/`pfnDeltaAddEncoder` extern-C shim (Chunk 6) can forward it
without an adaptation thunk. `DeltaField` (delta.hpp:78-88) is the
xash3dpp-side struct laid out to serve as that opaque token — its layout is
**not** guaranteed to match legacy `delta_s` byte-for-byte (documented in
both `delta.hpp:69-76` and `networking-boundary.md`'s "Known Deviations"),
so a real HLSDK-compiled game DLL that assumes GoldSrc's `delta_s` layout
and pokes at it directly (rather than going through
`Delta_Find/Set/UnsetField*`) would misread it. **This is the ABI-watchdog
flag to re-check before `pfnDeltaAddEncoder` is exported in Chunk 6**: any
mod DLL that bypasses the accessor functions is a genuine compatibility
risk, not just a theoretical one, since `delta_s` is a public SDK struct
(`common/net_encode.h:58-68`) mods could plausibly touch directly.

`register_encoder`'s return-value seam shift (bool vs legacy void) is
already recorded in `networking-boundary.md`'s Known Deviations — the
future `enginefuncs_t` shim must discard the bool to keep the legacy void
signature; not re-derived here.

---

## 6. Baselines

**Write side** (`MSG_WriteDeltaEntity`, net_encode.c:1909-1998): takes a
pre-computed `baseline` int parameter (the caller — server code, outside
`net_encode.c` — decides which baseline index to reference, typically via
bit-cost comparison against `Delta_TestBaseline`, §7). If non-zero: write a
1-bit "has baseline" flag = 1, then the signed 7-bit offset itself
(`MSG_WriteSBitLong( msg, baseline, 7 )`, :1947). If zero: write the flag as
0 and delta strictly against `from` (the previous packet-entity state).
**The write side never touches client baseline arrays at all** — it is
purely a wire-encode of whatever offset the caller computed.

**Read side** (`MSG_ReadDeltaEntity`, :2011-2109) resolves the offset
against **client-only global state** (this function is compiled out under
`XASH_DEDICATED`, :2013/:2106 — a dedicated server never runs this path):

```
if baseline_offset != 0:
    if delta_type == DELTA_STATIC:
        backup = max(0, clgame.numStatics - abs(baseline_offset))
        from = &clgame.static_entities[backup].baseline
    elif baseline_offset > 0:
        backup = cls.next_client_entities - baseline_offset
        from = &cls.packet_entities[backup % cls.num_client_entities]
    else:  # baseline_offset < 0
        baseline_offset = abs(baseline_offset + 1)
        if baseline_offset < cl.instanced_baseline_count:
            from = &cl.instanced_baseline[baseline_offset]
        # else: `from` is left as whatever the caller passed in — legacy
        # silently keeps the caller-supplied `from` on out-of-range index
```

Three distinct resolution paths keyed by (a) `delta_type == DELTA_STATIC`,
(b) `baseline_offset > 0` (recent packet-entities ring buffer, treated as a
"backward" reference into `cls.packet_entities`), (c) `baseline_offset < 0`
(instanced baseline table, offset encoded as `-(index+1)` so index 0 can be
distinguished from "no baseline" / offset 0). Note the out-of-range case
for path (c) is a **silent no-op** (falls through to `*to = *from` with the
original `from`), not an error.

**xash3dpp seam**: `IBaselineResolver::resolve(baseline_offset, kind)`
(delta.hpp:111-118) replaces all three branches with one virtual call — the
client subsystem (Chunk 9+, not Chunk 6) implements the lookup against its
own state and returns `nullptr` to signal "keep the caller-supplied `from`"
(mirroring the legacy silent-fallthrough on out-of-range). The delta codec
(`read_delta_entity`, delta_codec.cpp:450-460) only calls `resolve()` when
`baseline_offset != 0 && params.baselines != nullptr` — **on the server
side (Chunk 6), `params.baselines` is expected to stay `nullptr`** since the
server is always the write side, matching legacy's `#if !XASH_DEDICATED`
compile-out. Chunk 6 should never need to implement `IBaselineResolver`;
it only computes and passes the `baseline` int into
`write_delta_entity`/`WriteDeltaEntityParams::baseline`.

---

## 7. Entity header walk (number, removeType) and `count_fields` helpers

**Write** (`MSG_WriteDeltaEntity`, :1909-1998):
1. `to == nullptr` → remove message: write `from->number` (13 bits) +
   `removeType` (2 bits: `force ? 2 : 1`) and return immediately — **no**
   entity-state field loop runs for a remove.
2. Otherwise: bounds-check `to->number` against `[0, GI->max_edicts)`
   (`Host_Error` on violation, :1938-1939 — xash3dpp logs and returns
   `false` instead, per Q-5); write `number` (13 bits) then `removeType = 0`
   ("alive", 2 bits) unconditionally.
3. Baseline flag+offset (§6).
4. `entityType` flag+value (§0 point 6) — written **before** table
   selection, and table selection reads `to->entityType` (the value just
   about to be written), not `from->entityType`.
5. Table selection: `ENTITY_BEAM` bit (`common/entity_state.h:20`, value
   `1<<1`) → `DT_CUSTOM_ENTITY_STATE_T`; else `delta_type == DELTA_PLAYER`
   → `DT_ENTITY_STATE_PLAYER_T`; else `DT_ENTITY_STATE_T`.
6. Field loop via the generic per-field-mark codec (§1); `numChanges` also
   incremented for the `entityType` write in step 4 (:1955-1956) — so
   `numChanges == 0` check at the end (:1997) accounts for entityType
   changes too, not just struct-field changes.
7. **No-change rollback**: if `numChanges == 0 && !force`,
   `MSG_SeekToBit( msg, startBit, SEEK_SET )` rewinds the whole entity
   record (including the number/removeType/baseline bits already written)
   — the caller's outer loop is expected to detect the unchanged
   bit-position and skip emitting this entity at all. This is the same
   "roll back the whole record on zero changes" pattern used by
   `MSG_WriteDeltaMovevars`, `MSG_WriteClientData`, `MSG_WriteWeaponData`.

**Read** (`MSG_ReadDeltaEntity`, :2011-2109): bounds-check `number` against
`clgame.maxEntities` first (return `false` + log, no `Host_Error` even in
legacy here — this one path was already non-fatal); read `removeType` (2
bits); `removeType & 1` → return `false` ("removed from delta-message",
i.e. drop this entity from the current packet but don't destroy client
state); `removeType & 2` → `zero to`, set `to->number = -1`, return `false`
("entity was removed from server" — full destroy signal to the caller);
`removeType` with neither bit (any other nonzero value) → log "unknown
update type" and return `false`. Only `removeType == 0` proceeds to
baseline resolution + field decode, returning `true`.

**"`count_fields` helpers"**: the relevant bit-cost estimator is
`Delta_TestBaseline` (net_encode.c:1164-1218), used by server code (outside
this file) to decide whether re-basing an entity against a fresh baseline
is cheaper than a delta against the previous packet-entity. It does **not**
write anything — it re-runs `Delta_CompareField` per field and sums
`pField->bits` for changed non-string fields, or `strlen*8` for changed
strings (§4), plus a fixed header cost of `MAX_ENTITY_BITS + 2` (number +
removeType) + 1 (entityType flag) + `numFields` (one "changed" bit per
field, always counted regardless of whether it changed). Special cases:
`to == nullptr, from == nullptr` → 0 bits (nothing to send);
`to == nullptr, from != nullptr` → exactly `countBits` (the fixed header,
i.e. a remove message) with no field loop. It **does** call
`Delta_CustomEncode` (:1199) before counting, so a registered encoder's
field suppression affects the bit estimate identically to how it would
affect an actual write. xash3dpp's `test_baseline` (delta_codec.cpp:
489-526) mirrors this exactly, including calling `impl_->custom_encode`
before the loop.

---

## 8. Constants inventory

| Constant | Value | File:line | Used by |
|---|---|---|---|
| `DT_BYTE` | `BIT(0)` = 1 | net_encode.c:29 | field type dispatch |
| `DT_SHORT` | `BIT(1)` = 2 | :30 | ″ |
| `DT_FLOAT` | `BIT(2)` = 4 | :31 | ″ |
| `DT_INTEGER` | `BIT(3)` = 8 | :32 | ″ |
| `DT_ANGLE` | `BIT(4)` = 16 | :33 | ″ |
| `DT_TIMEWINDOW_8` | `BIT(5)` = 32 | :34 | ″ |
| `DT_TIMEWINDOW_BIG` | `BIT(6)` = 64 | :35 | ″ |
| `DT_STRING` | `BIT(7)` = 128 | :36 | ″ |
| `DT_SIGNED` | `BIT(8)` = 256 | :37 | sign modifier |
| `DT_SIGNED_GS` | `BIT(31)` | :38 | GS-wire-only, remapped to `DT_SIGNED` at :2137-2142 |
| `tableIndex` width | 4 bits | :610, :640 | `svc_deltatable` |
| `nameIndex` width | 8 bits | :611, :645 | ″ |
| field `flags` width | 10 bits | :612, :656 | ″ |
| `bits - 1` width | 5 bits | :613, :657 | ″ |
| `MAX_ENTITY_BITS` | 13 | protocol.h:119 | entity number field |
| `MAX_EDICTS` | `1<<13` = 8192 | protocol.h:120 | entity number bound |
| `MAX_GOLDSRC_EDICTS` | `8192 + MAX_CLIENTS*15` | protocol.h:315 | GS edict ceiling |
| removeType width | 2 bits | :1932, :2025 | entity remove/alive flag |
| baseline offset width | `SBitLong(7)` | :1947, :2050 | instanced baseline index |
| entityType width | `UBitLong(2)` | :1954, :2076 | entity table selection |
| `MAX_WEAPON_BITS` | 6 | protocol.h:106 | weapon index in `MSG_WriteWeaponData` |
| `MAX_WEAPONS` | `1<<6` = 64 | protocol.h:107 | weapon index bound |
| GS group-count width | 3 bits | :1493, :1537 | `Delta_Write/ParseGSFields` |
| GS mask bytes | up to 8 (`bits[8]`) | :1488, :1519 | ″ |
| GS meta premultiply/postmultiply scale | `4000.0f` | :422, :429 | `dt_goldsrc_meta` |
| `EQUAL_EPSILON` | `0.001f` | xash3d_mathlib.h:70 | `Q_equal`, multiplier null-compression |
| `Q_rint` | half-away-from-zero, `±0.5f`/`±0.5` truncation | xash3d_mathlib.h:92 | all integer-rounding conversions |
| `DELTA_ENTITY`/`DELTA_PLAYER`/`DELTA_STATIC` | 0/1/2 | net_encode.h:27-32 | entity-table selector, "don't change order!" |
| `DT_EVENT_T .. DT_CUSTOM_ENTITY_STATE_T` | 0..7 | net_encode.h:34-47 | `dt_info[]` index == wire `tableIndex` |
| `CUSTOM_NONE`/`CUSTOM_SERVER_ENCODE`/`CUSTOM_CLIENT_ENCODE` | 0/1/2 | net_encode.h:19-24 | custom-encode ownership |
| `svc_deltatable` | 14 | protocol.h:36 | table-description command byte |
| `svc_deltamovevars` | 44 | protocol.h:66 | movevars command byte (self-written) |
| `svc_packetentities` / `svc_deltapacketentities` | 40 / 41 | protocol.h:62-63 | entity-delta message framing (caller-owned) |
| `ENTITY_NORMAL` / `ENTITY_BEAM` | `1<<0` / `1<<1` | common/entity_state.h:19-20 | `entityType` table-select bit |
| field-def table field counts | 16/31/18/22/56/91/7 | net_encode.c (see §3.4) | `cmd/pm/ev/wd/cd/ent/meta_fields` |
| movevars fallback field count | 27 (`31 - 4`) | :965 | `Delta_Init` reassert |
| `iAlternateSign` bracket | `MSG_Start/EndBitWriting`, net_buffer.h:176-194 | net_buffer.h | GoldSrc sign-magnitude scope |

---

## 9. Verification status

Parity-audited 2026-07 against this reconstruction. Two findings from that
pass are load-bearing for Chunk 6 and are called out here (full deviation
list lives in `networking-boundary.md`'s "Known Deviations" — not
duplicated):

1. **`MSG_ReadString`-family `'%'`→`'.'` sanitization** (legacy
   `MSG_ReadStringExt`, net_buffer.c:807-831, specifically line 823) was
   initially missing from the xash3dpp `MessageBuf` read path and has been
   fixed (commit `c7216327`). This matters for delta `DT_STRING` fields
   (`physinfo`, `skyName`, etc.) — without it, decoded strings containing
   `'%'` would differ from legacy byte-for-byte, and any re-encode of that
   decoded value would then diverge on the wire. Write side stays
   unsanitized, matching legacy.
2. **GoldSrc sign-magnitude discovery**: the `iAlternateSign` bracket
   behaviour (§0 point 3, §4) was not obvious from a first read of
   `Delta_ParseTableField_GS` alone — it only manifests through
   `MSG_StartBitWriting`/`EndBitWriting`'s effect on `MSG_WriteSBitLong` in
   `net_buffer.c`. Confirmed and fixed via commit `0fe1a6d2`, with a golden
   test pinning both layouts for the same `-5` value (19-bit GS stream
   `09 58 00` vs 9-bit Xash stream `F7 01`).
3. An exhaustive scan of every `DT_ANGLE` write/read call site
   (`MSG_WriteBitAngle`/`MSG_ReadBitAngle`, all seven struct tables) found
   no additional divergence beyond the documented "never multiplier-scaled"
   and "bit-pattern compare regardless of quantization" behaviours already
   captured in §4.

Chunk 6 should treat `xash3dpp/src/networking/delta/` as parity-verified
for the wire shapes documented above; if new divergences are found while
wiring the server's entity-delta emission path, add them to
`networking-boundary.md`'s Known Deviations rather than this file (this
file documents legacy behaviour; that file documents where xash3dpp departs
from it).

---

## 10. Uncertainties / flags

1. **`delta.lst` itself is not in this repository.** It is game/mod data
   (shipped by Half-Life/mods, loaded via `FS_LoadFile`, net_encode.c:876),
   not engine source. The grammar in §3 is derived from the parser code,
   not from an example file — UNCERTAIN whether every real-world
   `delta.lst` in the wild stays within the grammar as parsed (e.g. exotic
   whitespace/comment handling is `COM_ParseFile`'s general tokenizer
   behaviour, not re-verified here token-by-token).
2. **GS `svc_gs_deltadescription`-equivalent command byte**: legacy
   `Delta_ParseTableField_GS` (net_encode.c:2111) takes only a `sizebuf_t*`
   and assumes the command byte was already consumed by the caller (same
   pattern as the Xash struct codecs) — UNCERTAIN which numeric `svc_*`
   value GoldSrc uses for this on the real wire, since GoldSrc's own
   `svc_*` enum is out of scope for this repo's `protocol.h`. Not
   load-bearing for Chunk 6 unless GoldSrc-client compatibility work is in
   scope.
3. **Entity-number 13-bit field is read by the caller, not
   `MSG_ReadDeltaEntity`.** Confirmed from the doc comment (:2003-2004) and
   the parameter list (`number` passed in, not read), but the exact caller
   site (`CL_ParseDeltaEntity`/equivalent) was not surveyed here — out of
   scope for the encoder itself, but Chunk 6's server-side emitter must
   independently confirm the write-side symmetry (`MSG_WriteDeltaEntity`
   writes it internally at :1941, so writer and reader are NOT symmetric
   in whether the encoder owns that bit — writer owns it, reader's caller
   owns it).
4. **`Delta_ClampIntegerField`'s debug-only overflow warning**
   (`#ifdef _DEBUG`, net_encode.c:1024-1027) means overflow clamping is
   *silent* in release builds — a field value that overflows its declared
   bit width is clamped without any diagnostic outside debug builds. Not
   reproduced as a hard requirement in xash3dpp (Q-5 favours always-logged
   diagnostics), flagged here so Chunk 6 doesn't assume clamping is loud.
5. **`Delta_WriteTableField`'s `Assert` calls** (`dt && dt->bInitialized`,
   `nameIndex >= 0 && nameIndex < dt->maxFields`, net_encode.c:604, :607)
   are debug-only assertions with no release-build guard — a malformed
   internal table state would UB in release. xash3dpp's `parse_table_field`
   explicitly bounds-checks these (documented in
   `networking-boundary.md`'s "Wire-reachable hardenings"); this file only
   notes the legacy gap existed.
6. **Test struct (`delta_test_struct_t`, `XASH_ENGINE_TESTS`-gated,
   net_encode.c:324-358, :2279-2374)** is a legacy self-test harness
   exercising all field-type/sign/multiplier combinations against known
   values — useful as a cross-check oracle for xash3dpp golden vectors, but
   it is compiled out of normal engine builds and was not itself ported
   (xash3dpp has its own golden-vector tests under
   `xash3dpp/tests/networking/delta/`). Worth diffing against if a future
   golden vector disagrees with hand-derivation.
