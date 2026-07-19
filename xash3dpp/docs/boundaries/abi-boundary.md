# ABI Boundary Spec

> Refreshed 2026-07-06 (as-built pass). The scope note + design body below are
> preserved verbatim. Reconciliation with the shipped code (`src/abi/`, one TU +
> the vendored headers) is additive: see **As-built reconciliation
> (2026-07-06)**, **Extension axes (Q-21)**, and the **Threading** pointer near
> the end. Key drift recorded this pass: the single global accessor's *state*
> was RELOCATED from `xash3dpp_abi` to `xash3dpp_host`
> (`src/host/engine_context_accessor.cpp`) under the D-1 dependency split — the
> abi shim now only *consumes* it (OQ-10). Companion docs created this pass:
> `threading-analysis/abi-threading.md`,
> `modernization-opportunities/abi-modernization.md`,
> `legacy-survey/deep-dive-abi.md`.

> **Scope note.** `abi` is not a stateful subsystem in the Q-1 sense — it is a
> thin **vendoring + bridge layer**. It has no `EngineContext` member, no init
> params, no runtime state, and no hot path. This spec documents that fact so
> the finish-subsystem checklist reads as recorded judgment rather than
> unfinished work.

## Responsibility

The `abi` module does exactly two things:

1. **Vendors the frozen GoldSrc/Xash game-DLL ABI** — byte-exact C++ mirrors of
   the SDK structs, enums, function-pointer tables, and constants that
   unmodified Half-Life game/client DLLs read and write. Every declaration is a
   verbatim layout mirror of the legacy header it cites; field/slot order,
   types, and implicit padding are frozen.
2. **Hosts the `extern "C"` engine→game direct-export bridge**
   (`src/abi/engine_funcs.cpp`) — the `GAME_EXPORT` symbols game DLLs link
   against by name rather than through the `enginefuncs_t` vtable (today:
   `Host_Error`; the server/client chunks add the rest here). Each shim formats
   its varargs, routes diagnostics through `core::log`, and forwards to the live
   `EngineContext` via `xash::abi::current_engine_context()`.

It does **not** implement engine logic, own entities, allocate, or maintain any
mutable state. The `enginefuncs_t` / `DLL_FUNCTIONS` slot *implementations* live
in `server` (`src/server/abi/engine_table.cpp`), not here.

## ABI (what is frozen and why)

| Surface | Header | Frozen contract |
|---|---|---|
| `entvars_t` / `edict_t` / `globalvars_t` / `link_t` | `edict.hpp` | progdefs.h / edict.h layout; cross-link raw pointers |
| `enginefuncs_t` (159), `DLL_FUNCTIONS` (50), `NEW_DLL_FUNCTIONS` (5) | `eiface.hpp` | eiface.h slot order + verbatim signatures (incl. `unsigned long` / `unsigned int` LLP64-era choices) |
| `cvar_t`, `TraceResult`, `KeyValueData`, `SAVERESTOREDATA`, `TYPEDESCRIPTION`, ... | `eiface.hpp` | eiface.h / cvardef.h support PODs |
| `pmplane_t`, `pmtrace_t`, `physent_t`, `playermove_t` (+ ~30-slot callback table) | `pm_defs.hpp` | pm_defs.h pmove working set |
| `entity_state_t`, `clientdata_t`, `usercmd_t`, `weapon_data_t`, `event_args_t`, `event_info_t`/`event_state_t`, `movevars_t` | respective headers | HLSDK wire/exchange PODs |
| Primitive typedefs + `color24` | `abi_types.hpp` | width-frozen SDK primitives |
| `Host_Error(const char *fmt, ...)` | `engine_funcs.cpp` | `GAME_EXPORT` direct symbol (Q-14, host OQ-10) |

`static_assert`s in the headers plus `tests/server/abi/test_*_layout.cpp` pin
every field offset / slot count against the real legacy headers on both 32- and
64-bit targets. **This layer must never change struct layout, member types,
vtable order, or add `operator delete` to a vendored type.**

## Interface

- **Vendored declarations** — included by `server` (and later `client`) to
  interpret the pointers the game DLL hands across, and by the layout tests.
- **`xash::abi::current_engine_context()` / `set_current_engine_context()`**
  (`engine_context_accessor.hpp`) — the single documented exception to the
  "no global accessor" rule (Q-2, host OQ-10). The *state* behind it is owned
  by `host` (`src/host/engine_context_accessor.cpp`, D-1 hardening); this header
  only declares the accessor. Set is main-thread-only; read is a lock-free
  atomic acquire callable from any C-ABI caller thread.
- **`Host_Error`** — the `extern "C"` fatal shim; formats + `Fatal`-logs, then
  either forwards to `Host::signal_frame_abort` or (no live context)
  `log_fatal` + `std::abort`.

## Dependencies

| Dependency | Why |
|---|---|
| `core` | `core::log_va` / `core::log_fatal` for the `Host_Error` shim |
| `host` | `EngineContext` / `Host::signal_frame_abort` (the shim's forward target); host also owns the accessor's state |

The vendored headers themselves have **no** subsystem dependencies (only
`<cstdint>` / `<cstddef>` and each other) — that self-containment is deliberate,
so `server`/`client`/tests can include the ABI without pulling engine internals.

## Owned state

None. `abi` owns no runtime state. The `EngineContext*` singleton exposed via
the accessor is **owned by `host`**, not by `abi`. Consequently:

- **Stats (reviewer §5):** `stats exempt` — no mutable state, no hot path,
  nothing to instrument (recorded in `engine_funcs.cpp`).
- **Q-11 satellite (reviewer §10 / finish §9):** **not a satellite.** `abi` is
  a vendoring + bridge layer, not a driver/satellite of another subsystem; the
  compat/socket scanners report zero findings. No satellite scoring applies.
- **Limits / constants (QO):** every numeric constant in `abi` is **ABI/wire-
  frozen** (`k_interface_version`, `k_fenttable_*`, `k_max_*`, movetype/solid
  values, …). Per QO these live as vendored `k_*` beside the ABI and are never
  tunable — they belong here, not in `limits.hpp`.

## Annotation discipline (Q-22 / QN)

Every vendored header carries a **file-scope `// @annotation-exempt: abi-pod`**
declaring the category (QN): these are layout-frozen PODs / function-pointer
tables (`fnptr-table`) whose design we do not own, so the per-member `@lifetime`
matrix does not apply and `@thread-safety` is a caller/engine contract, not the
header's. The `annotation-coverage` scan reports **100 % on every axis** for
`abi` (satisfied-by-exemption).

> **Tooling note (recorded judgment).** The `compliance_scan --checks detail`
> *candidate* rule for `lifetime-annotation` only honours a **same-line**
> suppression marker (its `exclude_line_re` runs on the raw member line),
> whereas QN sanctions a type/namespace-scope `abi-pod` marker for a whole
> vendored POD. To satisfy **both** the QN scope decision (file-scope block,
> which drives `annotation-coverage` + `finish_check` item 10) **and** the
> literal `--checks detail = 0` gate, each raw-pointer member additionally
> carries a terse trailing `// @annotation-exempt: abi-pod`. These are
> comments only — **zero** layout/type/vtable impact.

Bridge-code diagnostics are covered too: the `engine_funcs.cpp` `Host_Error`
shim routes through `core::log*` (QI), never raw stdio.

### ABI-signature `compliance-allow`s (echoed, not violations)

The following are frozen-ABI declarations we may not retype; each is
`compliance-allow`-marked with its rationale so the scan echoes rather than
flags them:

- `int-width`: `CRC32_t = unsigned int` (verbatim eiface.h:98 typedef); the
  `pfnGetPlayerWONId` / `pfnGetApproxWavePlayLen` / `pfnCmdStart` fn-ptr
  signatures; the `k_fenttable_*` flags (QO ABI-frozen; `0x80000000u` requires
  unsigned width).
- `nodiscard-missing`: `pfnFunctionFromName` / `pfnGetPlayerWONId` /
  `pfnPrecacheEvent` / `pfnGetApproxWavePlayLen` — function-pointer *members* of
  a frozen table (`[[nodiscard]]` is not applicable to a table slot).
- `abi-file-io`: `pfnEngineFprintf`'s `std::FILE*` export (pre-existing).

## Quirks and invariants

- **Frozen slot counts** — `enginefuncs_t` = 159, `DLL_FUNCTIONS` = 50,
  `NEW_DLL_FUNCTIONS` = 5; `static_assert`ed. "ONLY ADD NEW FUNCTIONS TO THE END
  OF THIS STRUCT. INTERFACE VERSION IS FROZEN AT 138" (eiface.h:286).
- **LLP64-era signatures preserved** — `long cb`, `unsigned long`, etc. are kept
  verbatim even where a width-qualified type would be "cleaner"; the signature
  *is* the ABI.
- **FWGS deviations preserved verbatim** — e.g. `edict_t::leafnums` is a 96-byte
  union either way (GoldSrc has `short leafnums[48]`); `cvar_t::flags` is
  `uint32_t` (same width as GoldSrc's `int`).
- **`Host_Error` recursion / thread policy** — the shim is unconditionally
  callable from any game-DLL thread and deliberately does **not**
  `assert_thread_role`; the recursion/main-thread policy is enforced inside
  `Host::signal_frame_abort` (host Q-4 / OQ-1).

## As-built reconciliation (2026-07-06)

Re-scanned `src/abi/engine_funcs.cpp`, `src/abi/CMakeLists.txt`, the vendored
headers under `include/xash3dpp/abi/`, and the relocated accessor state in
`src/host/engine_context_accessor.cpp`. The spec above is **still accurate**;
this pass records three as-built facts and one drift.

- **OQ-10 accessor relocation (drift, recorded).** The design body still frames
  `abi` as *hosting* the `current_engine_context()` singleton. As shipped, the
  accessor's mutable **state** (`g_engine_ctx`, the one sanctioned
  `std::atomic<EngineContext*>` global) lives in **`xash3dpp_host`**
  (`src/host/engine_context_accessor.cpp`), not in `abi`. The header keeps its
  `include/xash3dpp/abi/engine_context_accessor.hpp` path and the `xash::abi`
  namespace; `abi` only *reads* the accessor. This is the D-1 dependency
  hardening (2026-07-06) that broke the historical `host ⇄ abi` link cycle — the
  link is now one-way `abi → host` (see `CMakeLists.txt`
  `PRIVATE xash3dpp_host`). The Interface/Owned-state/Dependencies sections above
  already reflect the *ownership* correctly; this note pins the *file move* so
  the relocation is not mistaken for missing code. Cross-ref:
  `deep-dive-host.md` §7 and `host-boundary.md` As-built reconciliation.
- **Single shipped shim.** `engine_funcs.cpp` currently exports exactly one
  `GAME_EXPORT` symbol — `Host_Error`. The server/client chunks add the rest of
  the direct-export family here later (the file is the designated home). No
  `enginefuncs_t`/`DLL_FUNCTIONS` *slot bodies* live in `abi`; those are in
  `server` (`src/server/abi/engine_table.cpp`), which is where the
  `strnicmp`-style comparisons actually run (see cross-subsystem note below).
- **Static-return-buffer thread contract (frozen-ABI).** Many frozen
  `enginefuncs_t` slots hand the game DLL back a `const char*` / `float*` that
  points into an engine-owned **static/scratch buffer valid only until the next
  engine call, on the calling (Main) thread** (`pfnGetCvarString`,
  `pfnGetPlayerAuthId`, `pfnPEntityOfEntIndex` string helpers, the `pfnVecToYaw`
  scratch, …). `abi` *declares* these signatures verbatim; it does not own the
  buffers (the slot bodies in `server` do). This contract is frozen and is the
  headline ABI/threading intersection — see the Threading pointer and P-2 door
  below. Documented here so the reconciliation, threading, and G-2 notes agree.

### Cross-subsystem note — `strnicmp`/`strncmp` over-read: **ABSENT** in `abi`

The `string_view → C-string` over-read pattern tracked in utilities (M-4),
filesystem (M-7), and cmd_cvar (M-5) is **absent** in `abi` proper: the one shim
(`engine_funcs.cpp`) does only `va_list` formatting through `core::log_va` and
performs **no** string comparison at all (0 sites). The `strcmp`/`strncpy` calls
that *do* appear under `src/server/abi/**` belong to the **server** subsystem's
slot implementations and use `utilities::strcmp` on NUL-terminated C-strings
(not a bounded `string_view` over-read) — they are out of scope for this spec.
Net: `abi` is a **negative data point** for that sweep.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. **`abi` is the frozen seam
G-2 (Game ABI v2) is conceptually anchored to** — it is the one subsystem whose
entire job is to preserve the immutable GoldSrc/Xash game-DLL contract. The
extension posture here is therefore almost entirely *door-keeping by
preservation*: the frozen flavor must stay a clean, isolated, load-time flavor
so a v2 interface can sit **alongside** it (Q-20 framing), never replacing or
mutating the frozen declarations.

| Goal / primitive | Applies? | Required seam or door |
|------------------|----------|-----------------------|
| **G-2** game ABI v2 | **Yes — headline door** | The frozen GoldSrc ABI ships as **one load-time flavor** (one game DLL per process, Q-20). A v2 interface would carry a **context handle in every slot**, declare/transact entity access, use per-player movement contexts, and split the tick into compute/commit phases (extension-goals §G-2). Door: keep the frozen flavor a *self-contained* vendored layer (no engine internals leak into the vendored headers; no engine-internal C++ reaches around it) so v2 can be a sibling load-time binding **behind the Q-20 `EntityView` seam**, not a rewrite of these headers. `abi` owes **no v2 design** now — its job is to keep the frozen flavor cleanly isolable |
| **Static-return-buffer contract** (P-2 intersection) | **Yes — frozen constraint** | The frozen slots that return `const char*`/`float*` into per-call engine scratch are only safe under the single-thread (Main) contract — the pointer is valid until the next engine call on the same thread. This is *the* place the frozen ABI resists G-3/P-2 off-main reads: an off-main reader can never hold such a return value. A v2 slot would return an owned/handle value instead. Door: do not "modernise" these signatures; record the constraint so P-2 snapshot work routes *around* them |
| **P-3** context-first, no new file-scope state | **Yes — door-keep (consumer)** | `abi` adds **no** global of its own; it *reads* the one sanctioned `g_engine_ctx` singleton, whose state now lives in `host` (OQ-10, D-1). Door: keep it at zero abi-owned globals — the accessor is the single documented C-ABI reach-in, and its state stays host-owned |
| **P-4** typed introspection | Consumer only | The shim routes diagnostics through `core::log*` (P-4 substrate), never raw stdio. No introspection surface owed |
| **P-6** services are satellites | Indirect | A v2 ABI, MCP, or scripting bridge that wants context per slot links *toward* the accessor + the frozen declarations; `abi` never links toward them. The role-agnostic `current_engine_context()` read is the sanctioned reach-in |
| **G-1 / G-3 / G-4 / G-5** | No new seam | `abi` provides no service surface; it is a passive declaration + bridge layer. Off-main goals marshal through host's frame drain (P-1), not through the shim |
| **P-1** main-thread inbox | N/A (consumer path) | `abi` owns no inbox and never will — the frozen slots execute on the game-DLL call stack (Main); anything off-main reaches the engine via host's P-1 drain, never via a slot. Stated here explicitly (was previously only implied by the bundled G row). |
| **P-5** narrowest-state signatures | N/A — signatures frozen | Every slot signature is ABI-frozen verbatim; P-5 cannot apply to them by definition. The one non-frozen function (`Host_Error` shim internals) takes exactly what it forwards. |
| **P-7** pool-owned RAII lifecycle | N/A — no ownership | `abi` allocates nothing and owns no objects; the vendored structs stay PODs per the Q-22 retrofit guard (no members, vtables, or `operator delete` may ever be added to them). |
| **P-8** annotation discipline | **Yes — by-role posture recorded (HB-9)** | Zero `assert_thread_role` sites is the DESIGN for this subsystem (C-ABI shim executing on the game-DLL stack — enforcement lives in the host/server callers); the doc's Annotation-discipline section carries the QN exemptions. This row records the HB-9 "enforcement-free-by-role" flag so the absence is never misread as an HB-3-style gap. |

**Net verdict:** `abi` owes **no new seam**. The single binding door is **G-2**:
keep the frozen GoldSrc flavor a clean, self-contained, load-time-isolable layer
(Q-20) so a context-carrying v2 ABI can be added as a sibling flavor behind the
`EntityView` seam — never by editing, renaming, or changing the calling
convention of any frozen symbol. The static-return-buffer contract is the
frozen ABI's hard limit against off-main P-2 reads and must be documented, not
refactored.

## Threading

Full analysis: **`docs/threading-analysis/abi-threading.md`**. Summary:

- **Runs on Main, transitively.** The `Host_Error` shim has no thread machinery
  of its own; it is reached from engine/game-DLL synchronous calls that run on
  `ThreadRole::Main`. It deliberately does **not** `assert_thread_role` (it must
  be unconditionally callable across the C ABI); the main-thread + recursion
  policy is enforced downstream in `Host::signal_frame_abort`.
- **The one cross-thread surface is the accessor read.**
  `current_engine_context()` is a lock-free `acquire`-load callable from any
  C-ABI caller thread; its *state* and write side are host-owned (Main-only,
  release-store, asserted). See `host-threading.md`.
- **Frozen static-return-buffer slots are `Race-static-buf`, made safe only by
  the single-thread contract** — the vendored declarations promise a `const
  char*`/`float*` valid only for the call on the calling (Main) thread. `abi`
  declares them; `server` owns the buffers. Documented as a required caller
  contract, not a defect.

## Tests (recorded judgment)

`abi`'s tests live under **`tests/server/abi/`** (layout pins, engine-table,
string pool, edict arena, fake game DLL), co-located with the server slot
implementations that exercise the vendored structs. There is deliberately no
`tests/abi/` directory; `finish_check` item 6 counts those server-side TUs and
reports "8 test files" for the `abi` scope (an earlier revision predicted "0" —
the tool attributes the layout-pin tests correctly). The tests are
not moved (they belong beside the server-side consumers). Those test mains that
drive main-thread paths already call `register_thread_role(ThreadRole::Main)`.
