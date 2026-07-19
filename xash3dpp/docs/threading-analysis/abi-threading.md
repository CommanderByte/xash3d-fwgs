# abi — Threading Analysis

> Boundary spec: `docs/boundaries/abi-boundary.md`

*Authored 2026-07-06 (as-built pass). Decision refs: `abi-boundary.md` +
`host-boundary.md` Resolved-decision OQ-10 (single sanctioned global accessor,
state relocated to `xash3dpp_host` under D-1), Q-2 (`EngineContext`), Q-4
(recursion guard), Q-14 (`Host_Error` frozen `GAME_EXPORT`), Q-20 (EDICT_STORE
seam), Q-21 (extension posture). `abi` is a vendoring + bridge layer with no
runtime state of its own — this file is deliberately short but complete.
Companion docs: `host-threading.md` (owns the accessor state + frame pump),
`server-threading.md` (owns the `enginefuncs_t` slot bodies + their scratch
buffers), `memory-threading.md` (below-platform posture precedent).*

Sources scanned: `src/abi/engine_funcs.cpp`, `src/abi/CMakeLists.txt`,
`include/xash3dpp/abi/*.hpp` (vendored declarations),
`src/host/engine_context_accessor.cpp` (the relocated accessor state it reads).

______________________________________________________________________

## Ownership model

**No owned threading state.** `abi` runs entirely on the **Main** thread —
*transitively*, because its one shipped symbol (`Host_Error`) is reached only
from engine/game-DLL synchronous calls that already execute on
`ThreadRole::Main`. The subsystem holds:

- **No file-scope mutable state.** The one sanctioned global,
  `g_engine_ctx` (`std::atomic<EngineContext*>`), is **not** owned by `abi` —
  its state lives in `xash3dpp_host` (`src/host/engine_context_accessor.cpp`)
  after the D-1 relocation (OQ-10). `abi` only *reads* it via
  `current_engine_context()`.
- **No mutex, no atomic of its own, no `setjmp`/`longjmp`.** The shim formats
  varargs into `core::log_va` and forwards to `Host::signal_frame_abort`.
- **No hot path, no counters** (stats-exempt, `engine_funcs.cpp`).

There is no init-then-read-only transition because there is nothing to
initialise: the vendored headers are pure declarations, and the shim is a
stateless function.

______________________________________________________________________

## Thread-role posture

`abi` sits **above** platform (where `ThreadRole` lives) and *could* call
`assert_thread_role`, but deliberately does **not** — the opposite choice from
host, and for a good reason: the shim's contract is to be **unconditionally
callable across the `extern "C"` boundary** from any game-DLL thread. A game DLL
may (wrongly) call `Host_Error` off-Main; the shim must still format and forward
rather than trap in release. The main-thread + recursion policy is enforced
**downstream** in `Host::signal_frame_abort` (host Q-4 / OQ-1), which is the
correct single choke point.

There are therefore **zero** `assert_thread_role` sites in `abi`. This is a
recorded design decision, not a gap — the enforcement belongs to the forward
target, and the accessor's write side (host-owned) already asserts Main.

______________________________________________________________________

## Classification table (analyse-threading)

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `Host_Error(const char*, ...)` | `src/abi/engine_funcs.cpp` | **Main-only by contract (unenforced by design)** | Frozen `GAME_EXPORT` C shim. Formats varargs → `core::log_va` (Fatal) → `Host::signal_frame_abort` via the accessor, or `log_fatal`+`std::abort` if no live context. No `assert_thread_role` **on purpose** (must be callable across the C ABI); recursion/Main policy lives in `signal_frame_abort` |
| `current_engine_context()` read | `engine_context_accessor.hpp` (state in `host`) | **Atomic (acquire) — role-agnostic read** | Lock-free `acquire`-load of the ONE sanctioned global. Callable from any C-ABI caller thread by design; returns a pointer into live Main-owned state. State + write side (release-store, asserted Main) are **host-owned** (OQ-10) — see `host-threading.md` |
| frozen static-return-buffer slots (`pfnGetCvarString`, `pfnGetPlayerAuthId`, `pfnVecToYaw` scratch, …) | vendored decls in `eiface.hpp` (bodies in `server`) | **Race-static-buf — safe only under the single-thread contract** | The frozen signatures return `const char*`/`float*` into per-call engine scratch, valid **only until the next engine call on the calling (Main) thread**. `abi` *declares* them; the buffers are owned by `server`. Not a defect — a frozen ABI constraint |
| vendored PODs / fn-ptr tables (`edict_t`, `enginefuncs_t`, `playermove_t`, …) | `include/xash3dpp/abi/*.hpp` | **Layout-frozen declarations — no runtime behaviour** | Pure `struct`/typedef mirrors; thread-safety of any *instance* is the engine/game contract, not the header's (`@annotation-exempt: abi-pod`, `@thread-safety` = caller contract) |

______________________________________________________________________

## Safe items

- **`current_engine_context()` read** — `std::atomic` acquire-load; the host's
  set-last-in-init / clear-first-in-shutdown bookend means a C-ABI caller never
  observes a partially-initialised or half-torn-down context. Correct as
  written; the machinery lives in `host`.
- **The `Host_Error` no-context fall-through** — when `current_engine_context()`
  returns `nullptr` (before init / after shutdown), the shim routes through
  `core::log_fatal` (which works without a live `EngineContext`) then
  `std::abort`. No dangling deref, no partial state.
- **The vendored declarations** — declarations have no runtime state; including
  them from multiple TUs / threads is trivially safe.

______________________________________________________________________

## Hazards

None **within `abi`** under the single-thread contract. The table records the
two items `abi` *relies on* but does not itself synchronise (both discharged by
other subsystems):

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `Host_Error` → `signal_frame_abort` | `engine_funcs.cpp` | Race-shared (contained; enforced downstream) | Reachable from a game-DLL C call. If a game DLL ever called it off-Main, the frame-abort trio it writes (in `host`) would race the `RunFrame` consumer. The missing enforcement is host's `assert_thread_role` gap on `signal_frame_abort` (see `host-threading.md` Rec 1 / P-8), **not** an abi defect |
| frozen static-return-buffer slots | vendored decls / `server` bodies | Race-static-buf (contract) | An off-main reader (a future G-3 debug thread) can **never** hold a `const char*`/`float*` returned by these slots — the pointer is per-call, Main-thread scratch. This is the frozen ABI's hard limit against P-2 off-main reads; a v2 slot (G-2) would return an owned/handle value instead |

**Step 5 — signal-handler reachability:** `abi` registers **no** signal handler
and runs no code in a signal context. `Host_Error` is reached from game-DLL
**synchronous** calls on Main, never from an OS signal. The only terminal path
is `std::abort()` in the no-context branch (process termination — not
signal-reachable). Step-5 verdict: **N/A**.

______________________________________________________________________

## Required caller contracts

The subsystem relies on these being upheld; none are (or should be) asserted
inside `abi` — enforcement belongs to the forward target and the accessor's
host-owned write side:

1. **`Host_Error` is invoked on Main.** In practice every legacy caller does
   (game thinks, engine funcs, map-load). The shim does not assert this so that
   a misbehaving game DLL still gets a formatted diagnostic; the recursion +
   Main policy is enforced in `Host::signal_frame_abort`. A game DLL must not
   marshal `Host_Error` off-Main — there is no synchronisation on the
   frame-abort trio it writes.
2. **One live `EngineContext` per process.** `current_engine_context()` reads a
   single global; a second context would clobber the accessor. Matches the
   legacy single-`host` assumption (host-owned; listed here for completeness).
3. **Frozen static-return-buffer return values are consumed immediately, on the
   calling (Main) thread, before the next engine call.** This is the vendored
   ABI's contract, not something `abi` can enforce — the buffers live in
   `server`. Off-main code must never retain such a pointer (it marshals through
   host's frame drain / a P-2 snapshot instead).

______________________________________________________________________

## Recommendations

Ordered; none change behaviour today, all are door-keepers:

1. **Keep `abi` assertion-free on the shim, enforce downstream.** Do **not** add
   `assert_thread_role` to `Host_Error` — the unconditional-callability contract
   is deliberate. The real enforcement fix is host's `assert_thread_role` gap on
   `signal_frame_abort` (`host-threading.md` Rec 1 / P-8); land it there.
2. **Keep the abi-owned global count at zero.** `abi` reads the one sanctioned
   `g_engine_ctx` (host-owned after OQ-10) and owns none of its own. Any future
   direct-export shim added here must reach the context through the accessor —
   never a new file-scope global (P-3 door).
3. **Preserve the static-return-buffer contract in documentation, not in code.**
   When P-2 snapshots / a G-3 debug thread land, route them *around* the frozen
   return-pointer slots — an off-main reader cannot hold those values. A v2 ABI
   (G-2) is where those slots gain owned/handle return types; until then, record
   the constraint (done — boundary Extension axes + this file).
4. **When `ThreadRole::Service`/`Debug` are added** (additive enum, with their
   first consumer), the accessor's role-agnostic read path is already the
   sanctioned cross-thread reach-in — no abi change is owed; the marshalling
   happens at host's frame drain.
