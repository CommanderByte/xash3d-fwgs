# Scripting Runtime — Decision Brief (G-5)

> **Date**: 2026-07-06 (stub — the spike fills §5/§6)\
> **Status**: SHORTLISTED, not decided. The final pick is made HERE, after
> the prototype spike, and promoted to a register Q-entry.\
> **Scope decided (Q-21/G-5)**: tooling-first — debug automation, scripted
> test scenarios, console scripting on the cold path; gameplay/modding is a
> later promotion with its own brief revision.\
> **Related**: `extension-goals.md` §G-5 (goal + door rules),
> `layer-model.md` (tiers; `TODO(net-base)`), Q-11 (satellite), Q-22
> (lifecycle idiom the bindings must follow)

______________________________________________________________________

## 1. Hard constraints (from the north star — the runtime pick cannot relax these)

- Engine targets stay `/EHs-c- /GR-`. The VM lives in an **isolated-island
  satellite** static lib compiled with its own flags; **no exception — and
  no longjmp across engine RAII frames — ever unwinds an engine frame**.
  Binding shims are `noexcept` TUs compiled with engine flags.
- **Complete allocator hook** bridgeable to `xash::memory` pools —
  per-pool accounting at minimum, per-VM attribution preferred. Note the
  pools guarantee only **≥8-byte payload alignment** (no aligned-alloc API,
  Q-22): a runtime demanding higher alignment needs its own aligned path
  inside the satellite.
- Cold-path-only until proven; **deterministic-hardenable** for the warm
  path (fixed hash seeds, explicit RNG seeding, no reliance on unspecified
  iteration order); strict-FP compatible (Q-18 — no fast-math in the VM
  for sim-adjacent use).
- MSVC x64 **and** x86 (32-bit); GCC/Clang portability; permissive license;
  active upstream.

## 2. Reserved decisions (recorded now, built never — until the spike)

- **Satellite target `xash3dpp_script`** — reserved, decided-not-built (the
  `xash3dpp_http` precedent; Q-11 verdict: own state machine + own external
  dependency + useful standalone ⇒ separate target). Slots after
  `cmd_cvar`/`filesystem`/`core` in the root dependency order.
- **EH-island build pattern**: an `xash3dpp_allow_exceptions(target)` helper
  in the `cmake/fp_model.cmake` style — but it must **string-REPLACE
  `/EHs-c-` out of the target's inherited options before appending `/EHsc`**
  (naive appending triggers MSVC D9025; the same D9025 pair already fires
  as pre-existing noise from CMake's default `/EHsc` vs the global
  `/EHs-c-` — fixing THAT with the same REPLACE technique at the root is
  the companion tidy).
- **Optionality**: link-time real-vs-`_null` TU selection (the
  `XASH_NET_COMPRESSION`/bz2 precedent) if the runtime becomes an optional
  build feature.
- **Vendoring**: the chosen runtime establishes `xash3dpp/3rdparty/`
  (named in the instructions, unpopulated to date).

## 3. Script API surface v0 (already exists — the spike binds exactly this)

`CmdCvarContext`: `cmd_add` / `cbuf_add_text` / `cbuf_execute` /
`cmd_execute_string` / `cvar_get_or_create` / `cvar_set` / `cvar_describe`
(+ the reserved `CvarWriteSource::Script`, `CvarType`, `ParamSpec` slots);
`ITrustOracle` (untrusted-source gating) and `ICvarObserver`;
`core::log` + `log_set_callback` (script `print` + REPL capture);
`Filesystem::load_file` / `search`; `platform` dynlib if the VM ships
shared. All P-4-conformant — the spike must not add backdoors.

## 4. Decision axes (score each candidate in the spike)

1. **Error-model safety under `/EHs-c-`** — provably no unwind into engine
   frames; for longjmp VMs, the longjmp-clean callback discipline is
   demonstrated, not asserted.
2. **Allocator fidelity** — per-VM pool accounting accuracy vs effort;
   alignment demands vs the ≥8-byte pool guarantee.
3. **Binding ergonomics / total glue cost** against the C++23 engine
   (no-RTTI, no-exceptions bindings only).
4. **Determinism reproducibility** — hardened build, identical output
   across runs and across x86/x64 with strict FP.
5. **Suspend/resume + coroutine shape** (scripted test scenarios need to
   yield across frames).
6. **Sandboxing** vs `ITrustOracle` (rises to near-hard-gate iff untrusted
   mods become a warm-path requirement — then add **Luau** as a fourth
   candidate; it would top this axis).
7. **Debugger/introspection support** — feeds G-3/G-4 (the debug thread and
   in-game tooling want VM stack/locals visibility).
8. **Footprint + per-VM startup cost** (VM-per-thread is the G-2-era model).

## 5. Verified shortlist (research workflow, 2026-07-06 — 9 research agents → judge → 6 adversarial verifications)

Ranked field (fit 0-100): **Lua 5.4-as-C 90** · AngelScript 83 ·
~~Quirrel 82~~ (eliminated) · QuickJS-ng 81 · Wren 74 · Duktape 70 ·
WAMR 67 · LuaJIT 65 · wasmtime 52 · wasm3 47. Hard gates (EH/RTTI +
complete allocator hook) eliminated the WASM tail and demoted LuaJIT
(JIT mcode bypasses the allocator; JIT is also a determinism liability).

1. **Lua 5.4, built as C — front-runner.** Zero EH/RTTI (compile-time
   selectable longjmp error model); `lua_Alloc` covers every allocation
   incl. the `lua_State` itself, with per-VM userdata and type-coded stats
   (best pool fit of the field); no JIT → no interpreter-vs-JIT divergence;
   fully reentrant VM-per-thread, no global lock; GC steppable per frame.
   **Hardening required**: pin `luai_makeseed` (string-hash seed is
   ASLR/time-randomized), explicit `math.randomseed`, never rely on `pairs`
   order. **Binding discipline (verification finding)**: raw C API — the
   sol2 + `SOL_NO_EXCEPTIONS` recipe was CONTRADICTED by primary sources;
   no non-trivial RAII live across a potential `lua_error`; all VM entry
   via `lua_pcall` + `lua_atpanic`.
2. **QuickJS-ng.** All claims verification-CONFIRMED: `JSMallocFunctions`
   covers all runtime/context allocations with a per-VM opaque;
   `js_malloc_usable_size` implementable over our pools; the MSVC
   compound-literal issue is contained by C-only/`extern "C"` inclusion.
   Return-code error model (nothing can corrupt engine frames);
   spec-mandated deterministic iteration order (unique in the field); most
   active upstream. Costs: hand-rolled C-API binding; weak FPS-modding
   precedent.
3. **AngelScript.** Cleanest error model (AS_NO_EXCEPTIONS — return codes
   and callbacks, no longjmp either); best native C++ binding ergonomics;
   strongest gameplay-scripting precedent. Allocator gate UNCLEAR by
   verification (global, size-only hook; no per-VM handle; std-container
   bypass in the standard config) — **its spike task is exactly the
   TLS-routing/size-tracking shim**, or accepting pool-level (not per-VM)
   accounting.

**Eliminated by adversarial verification**: Quirrel — its mandatory
script-compiler static lib requires C++ exceptions (the repo's own CMake
applies `-fno-exceptions` only to the VM core), and the per-VM
`SQAllocContext` injection path is not public/stable as claimed.

## 6. Spike protocol (fills the decision)

Per candidate, in a throwaway branch: vendor minimally; build the VM as its
own static lib (own flags); bind `cbuf_execute` + `cvar_get/set` + log
capture behind `noexcept` shims; route the allocator hook into a dedicated
pool; run a scripted smoke (set a cvar, echo, read it back) on x64 AND x86;
measure per-axis (§4); go/no-go. The pick is recorded here with the scores,
then promoted to a register Q-entry; only then does `xash3dpp_script` get
built for real.

**Open inputs**: the warm-path determinism bar (x86↔x64 lockstep would
tighten libm/int-width constraints); whether untrusted-mod sandboxing enters
scope (adds Luau); the ABI-v2 binding shape (direct C++ object graphs favor
AngelScript; message-passing neutralizes that edge).
