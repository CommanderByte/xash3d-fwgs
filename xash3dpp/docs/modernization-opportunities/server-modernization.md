# server Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> C++ standard in use: C++**23** (`xash3dpp/src/server/CMakeLists.txt`,
> `target_compile_features(xash3dpp_server PUBLIC cxx_std_23)`; the tree-wide
> `CMAKE_CXX_STANDARD 23`). `std::span`, `std::string_view`,
> `std::optional`/`.has_value()`, `enum class`, and pimpl are used throughout —
> this subsystem was written directly in modern C++, not converted from C.
> Boundary spec: `docs/boundaries/server-boundary.md`
> Threading: `docs/threading-analysis/server-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-server-lifecycle.md`,
> `deep-dive-server-game-dll-bridge.md`, `deep-dive-server-clients.md`,
> `deep-dive-server-physics.md`, `deep-dive-server-world-frame.md`,
> `deep-dive-server-save-boundary.md` (plus `deep-dive-delta-encoder.md`,
> `deep-dive-trace-pvs.md`).
> ABI-/behaviour-frozen surfaces in this subsystem: the `enginefuncs_t`
> (159 slots) / `DLL_FUNCTIONS` (50) / `NEW_DLL_FUNCTIONS` (5) tables, the
> `edict_t` header + array-of-edicts representation, `entvars_t` (123 fields)
> and `globalvars_t` (`engine/progdefs.h`), the single `playermove_t` +
> `physent_t` (`pm_shared/pm_defs.h`), `server_physics_api_t` /
> `physics_interface_t` (`engine/physint.h`), and the Quake-lineage physics
> constants. See the frozen-ABI prohibition below.

## Summary

server is the largest subsystem in the rewrite (**30 TUs**, five slices) and
the dedicated-server milestone (Chunk 6, **Complete**). Despite its size it was
written **directly in modern C++23** — there is no legacy-C residue to convert.
The legacy `sv`/`svs`/`svgame` global triple folded into one heap-owned
`ServerRuntime` reached only through Main-thread entry points; entvars are read
through the **`EntityView`** zero-cost typed facade (Q-20) rather than raw
`->v.` poking; the game-DLL binding is a pimpl `Server` with injected deps
(Q-4); trace/contents/hull results are value types; errors return safe defaults
or route through the `host_error` hook rather than a scattered `Host_Error`
process-kill. `compliance_scan.py server` is **clean** (74 files, 0 findings).

The result is that the usual modernization headline — global mutable state,
raw casts over engine structs, C string handling, sentinel error returns —
**mostly does not apply to engine-internal server code**: those idioms were
either designed out (Q-2, Q-20) or **deliberately preserved** where the frozen
ABI demands them. That last clause is the crux of this report: unlike a
utilities or filesystem sweep, a large fraction of the "C-shaped" code in
`src/server/abi/` (and the pmove bridge) is **load-bearing by contract**, not
technical debt. The two categories must be kept apart, so this doc leads with
the prohibition and an explicit "deliberately C-shaped" inventory before the
short genuine-opportunity tail.

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that
headlines the utilities / filesystem / cmd_cvar reports is **absent** here —
see the cross-cutting note at the end.

______________________________________________________________________

## The frozen game-DLL ABI (do NOT "modernize" the slots)

This is the single most important item and it is a **prohibition**, not an
opportunity. The server carries the highest ABI risk in the whole rewrite: two
frozen surfaces meet here, and one of them — the **game DLL ABI** — is a
byte-frozen contract shared with unmodified Half-Life mod binaries. The
following are pinned and must **not** be "cleaned up" in any way that changes a
slot signature, calling convention, struct layout, field order/width, or the
array-of-edicts representation:

- **The `enginefuncs_t` table (`abi/engine_table.cpp`).** All 159 slots are
  plain C function pointers with fixed GoldSrc signatures and **no userdata
  parameter** — that is *why* they reach engine state through the single
  file-scope `g_bridge` singleton (a deliberate Q-20 carve-out, annotated
  `compliance-allow`). Do not "fix" the global by adding a context parameter to
  a slot; the signature is the ABI. This file is the **one** TU that projects
  engine types onto the frozen ABI (`SvTrace → TraceResult`, `Vec3 →
  float[3]`), and raw `->v.` / `reinterpret_cast` at the projection edge is
  *correct here and only here* (Q-20 confines it).
- **The edict store (`abi/edict_arena.cpp`, `edict.h`).** The `edict_t` header
  layout, the `serialnumber` EHANDLE-invalidation scheme, the free-list reuse
  quarantine, the 16-byte private-data rounding (`(cb + 15) & ~15`, the Poke646
  workaround), and — critically — the **array-of-edicts representation itself**
  are ABI (game code addresses entities by byte offset from the array base via
  `pfnEntOffsetOfPEntity`). A future handle/arena flavor is a **post-parity
  load-time binding behind the `EntityView` seam** (Q-20), never an edit to the
  array.
- **`entvars_t` / `globalvars_t` (`progdefs.h`).** 123 + N byte-exact fields
  handed to the DLL by pointer. `EntityView` is a *view over* this layout — it
  must never reorder or repack it.
- **The pmove bridge (`physics/pmove.cpp`, `init_client_move.cpp`,
  `pm_trace.cpp`).** The single global `playermove_t` and its ~30 callback
  slots, the `physent_t` layout, and the exact entvars↔playermove copy rules
  (MP `onground = -1`, `waterjumptime ↔ teleport_time` aliasing, `pitch =
  -v_angle/3` copy-back, the ±256 gather box, the 600/64 caps) are frozen. This
  is the exact surface G-2 reworks — **alongside**, never in place.
- **The Quake-lineage physics constants (`physics/physics.cpp`).** The
  whole-vector maxvelocity clamp, ClipVelocity ±1.0 snap-to-zero, 4-bump
  FlyMove, `SV_AddGravity` basevelocity fold, pusher ltime clock + ±3600 angle
  wrap, chase-dir `215.0f` typo, drown `dmg<15→10`, friction
  consumed-then-reset-to-1.0, and the `pushed[256]` cap are **behavioural
  contract** — a rewrite may add a bounds-check-and-log above the cap but must
  not change behaviour below it.

Any future refactor touching these must pass the hl.dll smoke (real `hl.dll`
loads, `c0a0` spawns, one frame runs clean) as the acceptance gate, exactly as
`map_loader` uses its golden trace vectors and `networking` its wire-format
tests. The **`EntityView` + `EngineBridge` + Q-20 confinement is the firewall**:
a modernized (v2) game ABI is a new sibling flavor selected at load time behind
that seam, never an edit to the frozen tables.

______________________________________________________________________

## Deliberately C-shaped ABI slot bodies (not opportunities)

These *look* like modernization targets but are correct as written because they
sit at (or just inside) the frozen boundary. Recorded so a future reader does
not "discover" and regress them:

- **`abi/engine_table.cpp` `strcmp` / `strncpy` over `v->name`, message names,
  cvar names.** C-string, NUL-terminated slot bodies — the ABI hands `const
  char*`. These are **not** `string_view` over-reads (see the note at the end).
- **`clients/info_string.cpp` `Info_ValueForKey` static-buffer key parser.**
  A faithful port of the GoldSrc `\key\value` codec (MAX_KV_SIZE 128 field
  truncation, `*`-key protection, the `c > 13` ascii filter, the "team"
  lowercasing quirk). The static return buffers (`s_value[256]` etc.) are the
  frozen slot contract — a `const char*` valid only for the call duration
  (threading Race-static-buf row, safe under OQ-9). Rewriting to
  `std::string_view` would break the ABI return type.
- **`abi/engine_table.cpp` varargs `pfnAlertMessage` / print slots.** The
  `<cstdarg>` `va_list` handoff is mandated by the frozen `...` signature; the
  modern move (format then hand off a bounded buffer) is already the shape —
  the varargs entry cannot be removed.
- **The two RNG statics (`s_rng_state` / `s_pm_rng`).** Marked
  `XASH3DPP-STUB(chunk6)` — the tracked idtech `COM_RandomLong`/`Float`
  parity port. When ported, the single shared stream stays Main-thread-only
  (threading note). Not a modernization item; a parity follow-up.

______________________________________________________________________

## Implementation-status table

Every design element the boundary spec calls for is either implemented or a
tracked stub; there is no "convert from C" backlog. The table records the modern
idioms in place (so a future reader does not regress them) plus the deferred
work.

| Design element | Status | Notes |
|----------------|--------|-------|
| `ServerRuntime` heap aggregate (no `sv`/`svs`/`svgame` globals) | **Implemented** | Q-2 held; reached only through Main-thread entry points |
| `EntityView` zero-cost typed entvars facade (Q-20) | **Implemented** | value-semantic accessors; raw `->v.` confined to `abi/` + pmove + save |
| `EdictArena` single authoritative store (Q-20) | **Implemented** | free-list, serialnumbers, freetime grace, 16-byte rounding |
| `EngineBridge` slot-state struct behind the 159-slot table | **Implemented** | one `g_bridge` file-scope singleton (deliberate ABI carve-out) |
| pimpl `Server` + injected deps (Q-4) | **Implemented** | `ServerInitParams` non-owning `cvars`/`fs`/`maps`/`net` + `host_error` hook |
| `ILevelChangeExecutor` seam (map_loader FSM) | **Implemented** | `Server : ILevelChangeExecutor`; `exec_load_level` full chain |
| PHS consumed read-only from map_loader (Q-19) | **Implemented** | `EngineBridge::phs` = `const PhsTable*`; no build code in server |
| Snapshot pipeline (PVS/PHS mask, delta, baselines) | **Implemented** | `clients/snapshot.cpp` (14 assert sites); wire byte-exact via networking |
| `std::optional` model/brush resolves over the world | **Implemented** | `.has_value()` guards throughout `world/` + `physics/` |
| Tier-1 `ServerStats` atomic (`frames_run`) | **Implemented** | any-thread read surface; Tier-2/3 compile-gated TODO |
| `ITrustOracle` answer wired (cmd_cvar D2) | **Deferred** | `server.hpp` TODO — G-1 precondition, no consumer yet |
| `ICompatPolicy` (Q-12) for peoei/gsmrf/HLMODS | **Deferred** | `peoei_broken` currently a plain init bool; policy seam owed Chunk 7 |
| Studio-hitbox trace loop + LRU (OQ-2) | **Deferred** | geometric core done (content); trace parity gated on hl.dll goldens |
| Chunk 8 save serializer (four primitives) | **Stubbed** | `exec_load_game`/`exec_change_level` are `XASH3DPP-STUB(chunk8)` |
| OQ-8 milestone trims (voice/HLTV/testpacket/NAT/A2S) | **Stubbed** | `// XASH3DPP-STUB(chunk6)` markers (134 tree-wide), finish-subsystem gate |

______________________________________________________________________

## Genuine internal modernization opportunities

These are the real (small) tail — engine-internal code, away from the frozen
boundary, where a modern idiom would improve clarity without touching any ABI.
None are urgent; the code is correct, `noexcept`, and compliance-clean.

### L-1: centralise the byte→char aliasing at the info-string edge

- **File(s)**: `clients/info_string.cpp` — the `read_field` / `const_cast`
  cursor walk over the caller-owned mutable info buffer (Q-16-annotated).
- **Modernization**: the port is faithful and correct, but the `char*` cursor
  plus the `const_cast` splice is the one place a bounded `std::span<char>` view
  could make the in-place edit explicit without changing the ABI (the buffer
  stays a caller-owned `char[]`). Cosmetic; low priority — the SAFETY
  annotations already document the invariant.

### L-2: `physics.cpp` `switch` MOVETYPE dispatch → table

- **File(s)**: `physics/physics.cpp` — the MOVETYPE_* dispatch switch.
- **Modernization**: a `constexpr` dispatch table keyed by MOVETYPE would DRY
  the switch, but the current form mirrors the legacy `SV_Physics_Entity`
  exactly and each arm carries a distinct stub/behaviour note. **Verdict: leave
  as switch** until the S9 physFuncs override stubs land — the switch is where
  those hooks slot in. Recorded only so a reader does not table-ify prematurely.

### L-3: fold the RNG-unification stub into one shared stream

- **File(s)**: `abi/engine_table.cpp:1543` (`s_rng_state`),
  `physics/init_client_move.cpp:280` (`s_pm_rng`).
- **Modernization**: the tracked idtech `COM_RandomLong`/`Float` parity port
  replaces both xorshift placeholders with the single shared legacy stream.
  This is a **parity** task (must match GoldSrc byte-for-byte), not a free
  cleanup — it lands with its golden test, and the shared stream stays
  Main-thread-only. Cross-references the same RNG-unification stub flagged in
  the threading doc.

______________________________________________________________________

## Deliberately NOT opportunities

- **Do not add a context parameter to any `enginefuncs_t` slot to "remove"
  `g_bridge`.** The slot signatures are the frozen ABI; the global is the
  documented Q-20 carve-out for the userdata-less C surface. G-2's v2 flavor
  carries the context — as a *new* interface alongside the frozen one.
- **Do not replace the array-of-edicts with a `std::vector<Entity>` or a
  handle map.** Game DLLs address entities by byte offset from the array base
  (`pfnEntOffsetOfPEntity`); the representation is ABI. Handleization is a
  post-parity load-time flavor behind `EntityView` (Q-20), not a refactor.
- **Do not "modernize" the Quake physics constants.** The ±1.0 ClipVelocity
  snap, the `215.0f` chase-dir typo, the friction reset-to-1.0, and the
  `pushed[256]` cap are behavioural contract (deep-dive-server-physics.md).
  A bounds-check-and-log above the cap is allowed; changing behaviour below it
  is not.
- **Do not touch the rotated-brush / trace math for tidiness (Q-18).**
  `world/clip.cpp:211` already carries a `TODO(Q-18)` noting the rotated-brush
  transform is ULP-inexact vs legacy — the fix there is toward *more* exactness,
  not a `std::ranges`/FMA rewrite. The trace/contents/hull kernels join the
  tree-wide float-exact no-touch set (map_loader trace/PVS/CRC + networking wire
  codec + content studio bone math).
- **Do not unify the pmove `RandomLong` with a `std::mt19937`.** The idtech RNG
  parity port is byte-exact by requirement (L-3); a standard-library generator
  would diverge.

______________________________________________________________________

## `strnicmp` / `strncmp` string_view over-read — ABSENT

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern
(utilities M-4 / filesystem M-7 / cmd_cvar M-5) is **absent** in server. Every
bounded/length compare here is over **NUL-terminated C-strings**, matching the
prior abi-phase note that `src/server/abi/**` `strcmp`/`strncpy` are C-string
slot bodies, not `string_view.data()` over-reads. Verified sites:

- `abi/engine_table.cpp:227` — `strcmp(v->name, name)` over NUL-terminated
  cvar names (ABI slot body).
- `clients/info_string.cpp` — `Info_ValueForKey` compares `strcmp(key, pkey)`
  where `pkey` is a `read_field`-produced NUL-terminated `char[128]`; the key
  parser is fully NUL-terminated C-string work.
- `clients/filter.cpp:61` — `std::strncmp(id, f.id, len)` where
  `len = min(strlen(id), strlen(f.id))`, so the compare reads at most `len`
  bytes of two **NUL-terminated** C-strings — a bounded compare over C-strings,
  **not** a non-terminated `string_view` fed to a length-bounded compare.
- `clients/client_state.cpp` / `messages.cpp` / `query.cpp` — `strcmp` /
  `strncpy` over NUL-terminated client names / message names / userinfo.

The candidate the cross-cutting note flags — **userinfo / `Info_ValueForKey`
key parsing** — *exists here* (`info_string.cpp`), but is implemented as a
NUL-terminated C-string parser (the frozen GoldSrc codec), so it is **not** an
over-read site. This is server's **negative** data point (the 9th across
platform / core / host / abi / launcher / map_loader / networking / content) —
the over-read pattern is confined to the four early text-heavy subsystems
(utilities / filesystem / cmd_cvar), and the shared bounded `ci_compare(sv, sv)`
the sweep proposes has no server caller.
