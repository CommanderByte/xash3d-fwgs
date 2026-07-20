# cmd_cvar Modernization Opportunities

> Refreshed 2026-07-06 (as-built pass). Re-scanned the shipped code: **every**
> remaining item from the prior table (M-2, L-1..L-6) has since been
> implemented, so the subsystem's mechanical modernization is now essentially
> complete. One **new** item (M-5) is added: completing M-2's `string_view`
> migration introduced a latent NUL-termination over-read shared with
> utilities M-4 / filesystem M-7. Prior dated analysis is retained; the status
> table and per-item notes are updated in place with "**Superseded
> 2026-07-06:**" markers where a status changed.

> **Refreshed 2026-07-20 (tree-wide modernization audit, 35-pack campaign).**
> The mechanical (C-idiom) modernization tracked above stays closed — this
> pass re-verified H-1/H-2/M-1..M-5/L-1..L-6 independently and found no
> regressions, but downgrades H-1/H-2 to informational/verification-only
> (zero remaining action, tier was not defensible for a re-verified done
> item). The pass's real yield is at the **architecture** level, one tier the
> prior mechanical sweeps did not examine: an unreachable GoldSrc compat
> factory that silently disables every legacy-parity cvar/cmd quirk (H-3); a
> genuinely dead interface recommended for **deletion** by the tree-wide
> subtraction lens (H-4, `ICvarObserver`); a boundary-doc claim about
> `ITrustOracle` that is false in production (H-5); a cvar-registry split
> that Chunk 12 turns into a three-way split unless resolved first (H-6);
> and a pure-deletion batch of 105 comment-only lines plus two permanently-
> dead stats fields (H-7). Eleven new Medium items and one new Low item come
> from duplication, doc-drift, and thread-assert-coverage findings. New IDs
> continue from the prior max (H-2, M-5, L-6). See `CORRECTIONS.md` and
> `corpus-digest.md` (§cmd_cvar) for verdict provenance, and `lenses.md`
> (L1/L3/L4/L8/L11) for the cross-cutting analysis cited throughout.

> C++ standard in use: **C++20** (from `xash3dpp/CMakeLists.txt`)\
> Boundary spec: [`docs/boundaries/cmd_cvar-boundary.md`](../boundaries/cmd_cvar-boundary.md)\
> ABI-frozen symbols in this subsystem: `CvarAbi` layout, `CvarFlags` values,
> `CommandFn = void (*)()`, all `pfn*` signatures in `engine/eiface.h` /
> `engine/cdll_int.h` / `engine/menu_int.h`

## Summary

The subsystem is written in clean C++20 — no `NULL`, no `malloc`/`free`, no
raw `new`/`delete` (the two `facts.json` "naked_new" hits are the English
word "new" inside comments, not allocations — corrected tree-wide by
`CORRECTIONS.md`), and all string inputs on the command-buffer side already
use `std::string_view`. The remaining mechanical C-isms (`reinterpret_cast`
scatter, `const_cast` at free sites) are already consolidated (H-1, H-2,
below) or implemented (M-1..M-5, L-1..L-6). **Nothing in that inventory is
open work any more.**

What the 2026-07-20 pass adds is architecture-level, not idiom-level:
`get_compat_policy()` (the GoldSrc link-time compat selector) has no header
declaration and no caller anywhere in the tree, so every GoldSrc quirk it
carries is inert in shipped builds (H-3); `ICvarObserver` is dead in every
direction at once — zero implementations, zero registrations, and unlike
every other deferred interface in the tree it carries no chunk marker — and
is recommended for outright deletion (H-4); the boundary doc's framing of
`ITrustOracle` as an "already exists" G-1 provider is false in production
(H-5); `cvar_register_dll`/`cvar_unlink` are a complete, unwired door onto a
second cvar registry that Chunk 12's client-DLL cvars will turn into a
three-way split if the storage-ownership question is not decided first
(H-6); and two comment-only translation units plus two permanently-zero
stats fields are pure deletions nobody has landed yet (H-7). A dozen smaller
duplication/doc-drift items round out Medium and Low.

## Implementation status

> **Superseded 2026-07-06:** all items previously marked "Remaining" (M-2,
> L-1..L-6) are now **Implemented** — verified against the current tree. The
> only open work is the **new** M-5 (string_view over-read), a by-product of
> M-2's migration.

| Item | Status |
|------|--------|
| H-1 reinterpret_cast helpers | **Implemented** — `cvar_list_next` / `cvar_list_set_next` in `context_impl.hpp`; all sites use them |
| H-2 pool-owned fields → `char *` | **Implemented** (Command/AliasDef only; `Cvar::def_string`/`desc` kept `const char *` — set from string literals at init) |
| M-1 cmd_execute_string stack buf | **Implemented** — `char buf[cmd_line_max]` + `memcpy` in `cmd_dispatch.cpp` |
| M-2 Public API `string_view` | **Implemented** (was Remaining) — `context.hpp` name/value params now `std::string_view`. **Introduced M-5.** |
| M-3 bucket_histogram `std::span` | **Implemented** |
| M-4 `utilities::snprintf` | **Implemented** |
| **M-5 `string_view`→C-string over-read** | **NEW / Remaining** — `.data()` fed to `CmdHashMap::hash`/`find` + `stricmp` (NUL-required). Latent; see below |
| L-1 `std::array` for observers | **Implemented** (was Remaining) — `std::array<ObserverEntry, cmd_observer_max>` |
| L-2 `std::array` for tok_argv/buf | **Implemented** (was Remaining) — `std::array<const char*, k_max_argc>`, `std::array<char, cmd_line_max>` |
| L-3 `std::array` for local hist | **Implemented** (was Remaining) — `std::array<std::size_t, cvar_hash_buckets>` in `dump_hash_stats` |
| L-4 `std::array` for MapStats | **Implemented** (was Remaining) — `std::array<MapStats, 3>` |
| L-5 Compat tables `string_view` | **Implemented** (was Remaining) — `constexpr std::array<std::string_view, N>` in `compat_goldsrc.cpp` |
| L-6 `std::strlen` in pool_dup | **Implemented** (was Remaining) — `pool_dup` uses `std::strlen` |

> **Note (not a modernization item):** several *features* the boundary spec
> describes are still stubs/TODOs (`cmd_scripting` `$`-substitution + `if`/`else`,
> `cl_filterstuffcmd` prefix filter, `exec` / `stuffcmds`, `Cvar_WriteVariables`,
> the `base_cmd` autocomplete tree, the `XASH_DEBUG_CVARS` change-log append).
> These are un-built behaviour, tracked in the boundary spec's *As-built
> reconciliation* section and the implementation plan — not C-ism cleanups, so
> they are out of scope for this document.

> **2026-07-20 independent re-verification:** every row above was re-checked
> against the current tree rather than taken on trust. M-4 in particular was
> re-derived from a fresh grep (`cstdio`/`snprintf`/`sprintf`/`vsnprintf`
> across `src/cmd_cvar`, `include/xash3dpp/cmd_cvar`,
> `include/xash3dpp/private/cmd_cvar`) and returned zero raw hits — all three
> formatting sites route through `utilities::snprintf`. No status in this
> table changed. H-1 and H-2 are additionally **retiered to informational
> only**: both are zero-action re-verifications of already-shipped work, and
> a High slot is not defensible for a finding with no remaining work and
> zero blast radius. They are kept in place (not deleted) per this
> campaign's tier-continuity rule; treat their content below as a closed
> record, not an open item.

______________________________________________________________________

## High-priority opportunities

### H-1: `reinterpret_cast` scatter for Cvar ABI list traversal

**Status: Implemented.** Added `cvar_list_next(Cvar *)`, `cvar_list_next(const Cvar *)`,
and `cvar_list_set_next(Cvar *, Cvar *)` to `context_impl.hpp`. All 10 call
sites in `cvar_ops.cpp` and `context_init.cpp` replaced. The `cvar_unlink`
tail-pointer trick (`Cvar **tail = reinterpret_cast<Cvar **>(&abi.next)`) was
rewritten using a `new_tail` pointer to avoid the type-punning alias entirely.
`cvar_get_list()` changed from `reinterpret_cast<CvarAbi*>(head)` to `&head->abi`.

- **File(s)**: `src/cmd_cvar/cvar_ops.cpp` lines ~75, ~100, ~120, ~250, ~260,
  ~290, ~310, ~320, ~355, ~365; `src/cmd_cvar/context_init.cpp` lines ~214, ~238

- **Current pattern**:

  ```cpp
  // 11+ occurrences across two files:
  Cvar *next = reinterpret_cast<Cvar *>(cv->abi.next);
  cv->abi.next = reinterpret_cast<CvarAbi *>(impl_->cvar_list_head);
  tail = reinterpret_cast<Cvar **>(&cv->abi.next);
  for (Cvar *cv = ...; cv; cv = reinterpret_cast<Cvar *>(cv->abi.next))
  ```

- **Suggested replacement**: Add two inline helpers to
  `include/xash3dpp/private/cmd_cvar/context_impl.hpp` (or a new
  `cvar_abi_list.hpp`):

  ```cpp
  inline Cvar *cvar_list_next(const Cvar *cv) noexcept {
      return reinterpret_cast<Cvar *>(cv->abi.next);
  }
  inline void cvar_list_set_next(Cvar *cv, Cvar *next) noexcept {
      cv->abi.next = reinterpret_cast<CvarAbi *>(next);
  }
  ```

  Every call site becomes `cvar_list_next(cv)` or `cvar_list_set_next(cv, next)`. The `reinterpret_cast` is reviewed and tested in one place only.

- **Boundary-safe**: Yes — the cast is already correct and ABI-required; this
  just consolidates 11 open-coded copies into two reviewed, named helpers.

- **Rationale**: Any future reordering of `CvarAbi` fields, or any wrong
  substitution of `cv->abi.next` with a different field, silently miscompiles
  all 11 sites. Consolidation into a named helper makes the cast auditable in
  one location and documents the required invariant (`abi` must be first in
  `Cvar`). This was identified as "Cluster 2" in the consolidation plan and
  explicitly deferred; this report records it as the highest-value remaining
  mechanical improvement.

> **2026-07-20 re-verification:** confirmed — `grep -rn reinterpret_cast`
> over `src/cmd_cvar` and both `cmd_cvar` include trees returns exactly the
> three casts inside `cvar_list_next`/`cvar_list_next(const)`/
> `cvar_list_set_next` (`context_impl.hpp:132-143`); everything else is a
> comment. `cvar_get_list()` reaches the ABI head via `&head->abi` (a
> stronger form than a cast). The layout precondition is enforced by
> `static_assert(__builtin_offsetof(Cvar, abi) == 0, ...)` at
> `cvar.hpp:157` — if that assert is ever deleted, the three helpers become
> UB with no compile-time signal. **No action remains; retiered to
> informational.**

______________________________________________________________________

### H-2: `const char *` pool-owned fields force `const_cast` at every free site

**Status: Partially implemented.** `Command::name`, `Command::desc`, and
`AliasDef::value` changed to `char *`; all `const_cast` at their 8 free sites
removed. `cv->abi.name` no-op casts (already `char *` ABI field) also cleaned up.
`Cvar::def_string` and `Cvar::desc` kept as `const char *` — they are assigned
from `constexpr char[]` string literals at init time (before `cvar_register_engine`
pool-dups them), so changing to `char *` would require adding `const_cast` at
those init sites. Net: removed 8 casts, added 0.

- **File(s)**:

  - `include/xash3dpp/private/cmd_cvar/registry_types.hpp` lines ~29–32
    (`Command::name`, `Command::desc`); line ~39 (`AliasDef::value`)
  - `include/xash3dpp/cmd_cvar/cvar.hpp` lines ~153, ~155
    (`Cvar::desc`, `Cvar::def_string`)
  - Free sites: `src/cmd_cvar/cvar_ops.cpp` lines ~260–280 (`cvar_unlink`);
    `src/cmd_cvar/context_init.cpp` lines ~242–285 (`shutdown`);
    `src/cmd_cvar/cmd_ops.cpp` lines ~30, ~60, ~90 (`cmd_remove`, `cmd_unlink`)

- **Current pattern** (~10 occurrences):

  ```cpp
  memory::mem_free(const_cast<char *>(cmd->name));
  memory::mem_free(const_cast<char *>(cv->def_string));
  memory::mem_free(const_cast<char *>(al->value));
  ```

- **Suggested replacement — Option A** (preferred): Change the pool-owned
  pointer fields to `char *` in the internal structs, since they are always
  mutable (pool-allocated and pool-freed by the engine):

  ```cpp
  // registry_types.hpp
  struct Command {
      char        *name;   // pool-owned (was const char *)
      char        *desc;   // pool-owned (was const char *)
      ...
  };
  struct AliasDef {
      char        *value;  // pool-owned (was const char *)
      ...
  };
  // cvar.hpp — internal extensions only (not the ABI-frozen CvarAbi fields)
  char *def_string;        // was const char *
  char *desc;              // was const char *
  ```

  All `const_cast` calls at free sites disappear. Read paths are unaffected
  (implicit `char * → const char *` conversion on use).

- **Suggested replacement — Option B** (minimal, no struct change): Add a
  single free helper:

  ```cpp
  inline void mem_free_const(const char *p) noexcept {
      memory::mem_free(const_cast<char *>(p));
  }
  ```

  This does not remove the `const_cast` concept but concentrates the
  justification in one reviewed location.

- **Boundary-safe**: Yes — `Command`, `AliasDef`, and the non-ABI fields of
  `Cvar` are internal; no frozen header exposes them.

- **Rationale**: Ten scattered `const_cast<char *>` + `mem_free` calls obscure
  intent and make audits harder. Each site requires the reader to verify that
  the pointer is genuinely pool-owned before concluding the cast is safe.

> **2026-07-20 re-verification:** confirmed as partial-by-design, not a
> gap. `def_string`/`desc` stay `const char *` because they take *both*
> string literals (`context_init.cpp:63,74`; `sound.cpp:165`) and
> `pool_dup`'d buffers (`cvar_ops.cpp:49,122`); the `const_cast` at the four
> free sites (`cvar_ops.cpp:273,279`; `context_init.cpp:263,272`) is
> deliberately concentrated there rather than scattered, which is the
> tree's standard shape for a dual-provenance field. **No action remains;
> retiered to informational.**

______________________________________________________________________

## High-priority: new architecture-level findings (2026-07-20)

### H-3: `get_compat_policy()` is unreachable — every GoldSrc parity quirk is inert in production

**Confirmed** (survived four independent refutation attempts). The GoldSrc
compat-policy factory is *defined* in both compat translation units but
*declared in no header*, so no other TU can call it without hand-declaring
the symbol — and none does.

- **File(s)**: `src/cmd_cvar/compat_goldsrc.cpp:102-106` (definition);
  `src/cmd_cvar/compat_null.cpp:27-31` (null-policy counterpart);
  `include/xash3dpp/private/cmd_cvar/compat_policy.hpp:27-45` (declares only
  `struct ICompatPolicy`, no free function); `include/xash3dpp/host/engine_context.hpp:52-54`
  (the injection point that is never given a non-null value)

- **Current pattern**: `grep -rn get_compat_policy xash3dpp/` returns
  exactly two hits, both *definitions*. `grep -rn compat_policy` over
  `src/`+`include/`+`tests/` finds only the member declarations, the two
  forwarding assignments (`host/engine_context.cpp:45`,
  `cmd_cvar/context_init.cpp:40`), and the three null-guarded uses
  (`cvar_ops.cpp:33,105`; `cmd_ops.cpp:45`). `EngineContextInitParams::compat_policy`
  is never assigned a non-null value anywhere in production. The 108-line
  `compat_goldsrc.cpp` TU — carrying `kCvarRedirects` (the HL25
  `gl_widescreen_yfov → r_adjust_fov` redirect), `kFilterableExemptions`
  (15 entries), `kOverridableCommands` (5 entries), each with an
  `engine/common/*.c` legacy citation — never runs.

- **Suggested replacement**: Add one line to
  `private/cmd_cvar/compat_policy.hpp`:
  `[[nodiscard]] ICompatPolicy &get_compat_policy() noexcept;` (both
  definitions already exist and match). Default the injection in
  `engine_context.cpp:45`:
  `cp.compat_policy = p.compat_policy ? p.compat_policy : &cmd_cvar::get_compat_policy();`
  Land it together with a parity test asserting `GoldSrcCompatPolicy` is
  selected under `XASH_GOLDSRC_COMPAT=1`, since activating it **changes
  observable behaviour by design** — that is the point, and it must not
  ship silently.

- **Boundary-safe**: Yes. No frozen-ABI struct or signature is touched; the
  change is a header declaration plus a null-coalescing default at an
  existing injection site.

- **Rationale**: This is the one place in the subsystem where **subtraction
  is the wrong answer** (see the "Investigated and refuted" note below for
  the contrast with `ICvarObserver`). The code behind the unreachable
  pointer is not speculative infrastructure — it is a transcription of
  *observable legacy behaviour* with a per-table legacy citation. Leaving
  it unwired converts a two-line wiring omission into a silent parity
  regression that no test would catch, in a fork whose entire premise is
  parity. `is_filterable_exempt` (one of `ICompatPolicy`'s three methods,
  also zero callers) should be **kept**, not deleted alongside the others —
  `FCVAR_FILTERABLE` is already set on 24 production cvars
  (`cmd_dispatch.cpp:148-157`) with nothing consuming it yet; add a chunk
  marker (`// XASH3DPP-STUB(chunk12): ...`, cross-referenced from
  `cmd_dispatch.cpp:151`) rather than deleting the method.

- **consumer**: `cvar_ops.cpp:33`, `cvar_ops.cpp:105`, `cmd_ops.cpp:45` —
  three null-guarded call sites already in the tree [exists-in-tree]
- **ext_tags**: G-2, P-6 (Q-12 link-time compat selection — a decision
  register entry, not a north-star axis, but directly implements it)
- **parity_fence**: touches no HB-2 kernel; changes observable legacy
  behaviour by design, so land with the parity test, not silently.

______________________________________________________________________

### H-4: `ICvarObserver` — dead in every direction; recommended for deletion

**`[EXT:G-1]` `[EXT:G-5]` `[EXT:P-4]`** — tier bumped from the digest's original
Low (F54) / Medium (F63) verdicts by the tree-wide subtraction lens (L11),
per this campaign's "give L11 deletions in your subsystem a High slot"
guardrail. `ICvarObserver` is the **only** interface in the entire tree
that the subtraction lens found dead in every direction simultaneously —
zero implementations (production or test), zero registration call sites
anywhere including tests, and — unlike every one of the tree's other 12
zero-production-implementation interfaces — **no in-code chunk marker**.

- **File(s)**: `include/xash3dpp/cmd_cvar/observers.hpp:41-49`
  (`struct ICvarObserver`); `include/xash3dpp/cmd_cvar/context.hpp:206`
  (`add_cvar_observer` declaration); `src/cmd_cvar/cvar_ops.cpp:13-20`
  (definition); `include/xash3dpp/private/cmd_cvar/context_impl.hpp:45-50`
  (`std::array<ObserverEntry, limits::cmd_observer_max>` + `observer_count`
  storage); `src/cmd_cvar/cvar_ops.cpp:224` and `:379` (the two dispatch
  loops, one per write path)

- **Current pattern**: Every cvar write — both `cvar_set_direct` (:224) and
  `cvar_full_set` (:379) — iterates a permanently empty 16-entry
  `std::array<ObserverEntry>` on the write hot path:

  ```cpp
  for (std::size_t i = 0; i < impl_->observer_count; ++i) { ... }
  ```

  `observer_count` is 0 forever: `add_cvar_observer()` (`cvar_ops.cpp:13-20`)
  has zero callers anywhere in `src/` or `tests/` — confirmed by grep, not
  estimated.

- **Suggested replacement**: Delete `struct ICvarObserver`
  (`observers.hpp:41-49`), `CmdCvarContext::add_cvar_observer`
  (`context.hpp:206`, `cvar_ops.cpp:13-20`), the `ObserverEntry` type plus
  the array and count in `context_impl.hpp:45-50`, and the two dispatch
  loops at `cvar_ops.cpp:224` and `:379`. ~43 lines net. Re-label the D3 row
  in `cmd_cvar-boundary.md` from "Matches the spec" to "removed 2026-07 —
  zero registrations in 4 chunks; re-add per the recorded shape (below)
  when Chunk-12 client userinfo or a G-1 read hook needs it."

- **Boundary-safe**: Yes — `cmd_cvar` has no named HB-2 kernel, and
  `ICvarObserver` is not part of any frozen ABI (`CvarAbi` layout is
  untouched by this deletion).

- **Rationale**: The counter-position is on record (digest F54, Low/
  UNVERIFIED: "keep the interface, add a door-debt row") and was
  deliberately weighed, not overlooked — it is defensible *if and only if*
  a chunk number is attached, and none of the tree's 69 other deferred
  obligations lacks one while this construct does. Absent that marker, an
  interface with no implementations, no callers, and no chunk after four
  shipped chunks of `cmd_cvar` work is exactly the §6 speculative
  infrastructure the anti-gold-plating rule exists to kill — and its cost
  is not just the 43 lines, it is two loops over a permanently empty table
  running on the write path of every cvar set in the engine.

  **Shape constraint for reconstruction** (never build ahead of this):
  when cvar-change notification is genuinely needed, it must be rebuilt in
  the shape deleted here — a pure-virtual single-method interface,
  `noexcept`, protected constructor/destructor, no RTTI; registration into
  a fixed-capacity bounded array sized from `limits.hpp` (never
  `std::vector`, never `std::function` — both banned/absent tree-wide);
  zero-heap dispatch; a flag-mask filter so the write path skips
  uninterested observers. This is a copy of the deleted form, not a
  redesign.

- **consumer**: n/a — deletion; the write-path loops it removes are the
  day-one beneficiary [exists-in-tree]
- **ext_tags**: G-1, G-5, P-4 — applies when Chunk-12 client wires userinfo
  cvar propagation, or a G-1 MCP read hook needs cvar-change events; record
  the shape constraint in `cmd_cvar-boundary.md`'s D3 row.
- **parity_fence**: Clear. No HB-2 kernel, no frozen-ABI symbol touched.

______________________________________________________________________

### H-5: `ITrustOracle` — boundary doc's "headline G-1 provider, already exists" framing is false in production

**Confirmed** (four independent refutation attempts all failed). The
interface, its injection point, and its null-safe dispatch guard are all
**correctly built** per design D2 — this is not a cmd_cvar code defect.
What is false is the boundary doc's characterization of it as an existing
G-1 asset.

- **File(s)**: `include/xash3dpp/cmd_cvar/observers.hpp:63-71`
  (`struct ITrustOracle`); `include/xash3dpp/host/engine_context.hpp:50-53`
  (injection point, defaults to `nullptr`); `src/cmd_cvar/cmd_dispatch.cpp:198`
  (the one production use — correctly null-guarded); `include/xash3dpp/server/server.hpp:89`
  (a stale `TODO(chunk6-S9)` naming this obligation against chunk numbers
  that have since shipped)

- **Current pattern**: `ITrustOracle` gates `stuffcmd` privilege escalation
  correctly at `cmd_dispatch.cpp:198`. But `EngineContextInitParams::trust_oracle`
  defaults to `nullptr`, and **no production class implements `ITrustOracle`
  anywhere in the tree** — only 5 test doubles do
  (`tests/cmd_cvar/test_stubs.hpp:10,14`; three more across
  `tests/sound/`, `tests/content/`). The legacy privileged-stuffcmd path
  (`SV_Active() && SV_GetMaxClients() == 1`, `observers.hpp:55-57`) —
  singleplayer listen-server privilege — **cannot fire in any shipped
  build.**

- **Suggested replacement**: Not a cmd_cvar code change — the interface,
  injection, and dispatch guard need nothing. Two doc actions: (1) replace
  `server.hpp:89`'s stale `TODO(chunk6-S9)` with a live chunk number now
  that the boundary doc's implementation obligation is understood as a
  Server-side gap, not a cmd_cvar gap; (2) soften `cmd_cvar-boundary.md`'s
  Extension-axes framing from "already exists" to "injection point and
  null-guarded dispatch exist; the Server-side implementation is a
  legacy-parity divergence, not a design gap."

- **Boundary-safe**: N/A — documentation-only for cmd_cvar; the code
  obligation belongs to Server.

- **Rationale**: `facts.json` originally scored `ITrustOracle` as
  `prod_impls: 2`, both of which are files that merely *mention* the
  interface (`engine_context.hpp`, `server.hpp`) rather than implement it —
  a systematic over-count the corrections file documents tree-wide. The
  real count is zero. A boundary doc calling a permanently-inert path
  "already exists" is the failure mode SUB-9 (tooling hardening) exists to
  catch mechanically going forward.

- **consumer**: n/a — documentation correction [exists-in-tree]
- **ext_tags**: G-1
- **parity_fence**: Clear. Documentation only.

______________________________________________________________________

### H-6: Two cvar registries — the DLL-registration door exists and is unwired; Chunk 12 makes it a third store

**`[EXT:G-2]`** — from cross-cutting lens L4 (`state-shape-g2`), whose
evidence set includes cmd_cvar's own unification API. Not forced by any
frozen ABI; it is a half-built seam with a live behavioural gap.

- **File(s)**: `include/xash3dpp/cmd_cvar/context.hpp:89`
  (`cvar_register_dll(CvarAbi *)`); `src/cmd_cvar/cvar_ops.cpp:61`
  (implementation); `include/xash3dpp/cmd_cvar/context.hpp:103` and
  `src/cmd_cvar/cvar_ops.cpp:249` (`cvar_unlink(owner_flags_mask)`,
  owner-tagged for DLL unload); the only caller of either is
  `tests/cmd_cvar/test_cvar_registry.cpp:148`. (The other half of the
  split — `EngineBridge::external_cvars` and the six frozen `pfnCVar*`
  slots that resolve only against it — lives in `server`, out of this
  report's scope; see `server-modernization.md`.)

- **Current pattern**: `cmd_cvar` owns the engine's cvar registry
  (`sv_gravity` etc., read via `movevars.cpp:56` `rt.cvars->cvar_variable_value`).
  A game DLL's own `cvar_t` structs are threaded through a *separate*
  unsorted LIFO chain in `EngineBridge::external_cvars`, with the six
  frozen `pfnCVarGet*`/`pfnCVarSet*`/`pfnCvar_DirectSet` slots resolving
  only against that second chain. `cmd_cvar` already ships a door built
  precisely for this — `cvar_register_dll`/`cvar_unlink` with
  `FCVAR_EXTDLL|FCVAR_CLIENTDLL|FCVAR_GAMEUIDLL|FCVAR_REFDLL` owner
  tagging — with **zero production callers**. Legacy
  (`engine/common/cvar.c:506-553`) links game and engine cvars into **one**
  alphabetically-ordered list with a `BaseCmd_FindAll` collision check,
  `Cvar_UpdateInfo` (serverinfo/userinfo propagation), and `Cvar_Changed`;
  the two-registry split loses all three, which is a parity divergence,
  not an ABI constraint.

- **Suggested replacement**: This is a register-entry decision, not a pure
  refactor — `cvar_register_dll` currently **copies** the DLL's five ABI
  fields into a pool-allocated wrapper (`cvar_ops.cpp:69-85`, deliberately,
  because writing extended fields through a 5-field `cvar_t*` would be
  out-of-bounds UB), but legacy semantics require the DLL's struct to be
  the *live* storage (game code reads `pCvar->value` directly off its own
  static — `engine_bridge.hpp:126-128`). Three options, ranked:
  (a) **lookup fall-through only** — S effort, one file, closes the read
  gap, leaves two stores, no `cvarlist`/serverinfo/collision-check parity;
  (b) **indirect storage** — give the registry node an optional
  `CvarAbi *storage` defaulting to `&abi` so writes land in the DLL's
  struct — M effort, touches `cvar.hpp:113-120`'s offset-0 layout contract
  and needs a `static_assert` review on both arches, but restores full
  legacy semantics with one store — **recommended**; (c) write-through
  mirror — S effort but institutionalises the hand-synced-mirror pattern
  that already produced a live `sv_time` defect elsewhere in the bridge
  (see `server-modernization.md`) — recommend against.

- **Boundary-safe**: NeedsVerification — option (b) touches `cvar.hpp`'s
  frozen offset-0 `CvarAbi` layout contract and needs the `static_assert`
  re-verified on both x86 and x64 before landing; options (a)/(c) touch no
  frozen shape.

- **Rationale**: `FCVAR_CLIENTDLL` already exists in `cmd_cvar`'s flag
  enum — Chunk 12's client DLL will register cvars through it. If the
  storage-ownership decision is not taken before Chunk 12 starts, a third
  chain gets written, and the door built here (unification API, zero
  callers) becomes the same "door built, consumer present, never
  connected" failure mode already confirmed for `ITrustOracle`/`ICvarObserver`.

- **consumer**: Server's six `pfn_cvar_*` slots (currently broken against
  engine cvars) [exists-in-tree]; Chunk 12's client DLL cvar surface
  [scheduled-chunk-N]
- **ext_tags**: G-2
- **parity_fence**: No HB-2 kernel. The frozen slot signatures do not
  change — only what they resolve against. `cvar.hpp`'s `CvarAbi` layout
  is `static_assert`ed against `cvar_t` on both arches today; option (b)
  must keep `abi` at offset 0 and re-verify that assert on x86 and x64.

______________________________________________________________________

### H-7: Decision-free deletion batch — 105 comment-only lines, two permanently-dead stats fields

Promoted from the digest's Medium (F50, F61 — both UNVERIFIED) per this
campaign's subtraction-lens guardrail: L11 independently re-verified both
items (`wc -l`, tree-wide grep for the field names) and included them in
its cross-subsystem pure-deletion batch (SUB-1). Neither needs a design
decision or a consumer — that is what makes them High rather than merely
tidy.

- **File(s)**: `src/cmd_cvar/cmd.cpp:1-54`; `src/cmd_cvar/cvar.cpp:1-51`;
  `src/cmd_cvar/CMakeLists.txt:17-18`; `include/xash3dpp/cmd_cvar/stats.hpp:28-29`

- **Current pattern**: `cmd.cpp` (54 lines) and `cvar.cpp` (51 lines)
  contain **zero** declarations, definitions, or `static_assert`s — only a
  header comment and a `TODO` block describing an implementation that
  landed elsewhere (in `cvar_ops.cpp`/`cmd_ops.cpp`/`cmd_dispatch.cpp`)
  months ago. Both are still listed in `CMakeLists.txt:17-18` and compiled
  into the library on both arches; the comments are now actively
  misleading (e.g. `cvar.cpp`'s TODO for `cvar_set_direct`'s write tail no
  longer matches the shipped three-copy version M-7/M-8 below describe).
  Separately, `CmdCvarStats::peak_cvar_count`/`peak_command_count`
  (`stats.hpp:28-29`) are declared, documented in three places
  (`cmd_cvar-boundary.md`, `architecture/cmd_cvar/instrumentation.md`, and
  this doc's own status table history), and **never assigned** — grep for
  both names tree-wide finds only the declaration and its doc mirrors.
  They are also the struct's only two non-atomic fields (see M-12's tier-
  dependent thread-contract note).

- **Suggested replacement**: Delete `cmd.cpp` and `cvar.cpp` outright and
  their two `CMakeLists.txt` rows (`base_cmd.cpp` **stays** — unlike these
  two it carries three load-bearing `static_assert`s and is a documented
  HB-11 placeholder, not stale prose). Delete `CmdCvarStats::peak_cvar_count`/
  `peak_command_count` and their three doc mirrors.

- **Boundary-safe**: Yes — pure deletion of dead comment TUs and
  never-written struct fields; no frozen ABI, no HB-2 kernel.

- **Rationale**: Zero production callers or references beyond documentation
  for either item; neither requires a design decision, a consumer, or a
  fence judgement. Deleting the two peak-counter fields has a bonus:
  `CmdCvarStats` becomes fully atomic in every build tier, clearing its
  future G-3 cross-thread-read bar with no lock — see M-12 for the
  tier-dependent-thread-contract hazard this removes.

- **consumer**: n/a — pure deletion [exists-in-tree]
- **ext_tags**: G-3 (dead-field deletion; makes the struct's thread
  contract tier-independent)
- **parity_fence**: Clear of both fences.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `cmd_execute_string` heap-allocates a `std::string` solely for null-termination

**Status: Implemented.** Changed to `std::memcpy` + manual null-terminator into
a `char buf[limits::cmd_line_max]` stack buffer. `#include <string>` removed from
`cmd_dispatch.cpp`.

- **File(s)**: `src/cmd_cvar/cmd_dispatch.cpp` lines ~185–188

- **Current pattern**:

  ```cpp
  void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
  {
      std::string line{ text };        // heap allocation just for '\0'
      dispatch_cmd(*impl_, *this, line.c_str(), true, 0);
  }
  ```

- **Suggested replacement**: Stack-copy with `utilities::strncpy`:

  ```cpp
  void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
  {
      char buf[limits::cmd_line_max];
      utilities::strncpy(buf, text.data(),
                         text.size() + 1 < sizeof(buf) ? text.size() + 1 : sizeof(buf));
      dispatch_cmd(*impl_, *this, buf, true, 0);
  }
  ```

  Alternatively, change `dispatch_cmd` to accept `std::string_view` and build
  the per-fragment null-terminated copy only at fragment boundaries (already
  done for `line_copy` internally; the outer conversion becomes unnecessary).

- **Boundary-safe**: Yes — `cmd_execute_string` is internal.

- **Rationale**: With `/EHs-c-` (no exceptions), a `std::string` constructor
  failure produces a `std::bad_alloc` that cannot be caught. Using a stack
  buffer matches the no-allocation contract the rest of the dispatch path
  honours (all other callers pass already-null-terminated strings).

______________________________________________________________________

### M-2: Public registry API takes `const char *` where `std::string_view` is natural

**Status: Implemented (2026-07-06).** All name/value view parameters on
`context.hpp` (`cvar_find`, `cvar_get_or_create`, `cvar_set`, `cmd_add`,
`cmd_remove`, `cmd_describe`, `cmd_exists`, `cmd_execute_string`, `cbuf_*`, …)
now take `std::string_view`. **Superseded 2026-07-06:** the prior "Remaining"
status is closed. **Caveat:** the internal call chains resolve the view with
`name.data()` and feed it to the NUL-required `CmdHashMap`/`stricmp` layer —
this is the new **M-5** over-read hazard; the M-2 signature migration is
otherwise complete.

- **File(s)**: `include/xash3dpp/cmd_cvar/context.hpp` lines ~80–180

- **Current pattern**:

  ```cpp
  Cvar        *cvar_find(const char *name) noexcept;
  void         cvar_set (const char *name, const char *value, ...) noexcept;
  bool         cmd_exists(const char *name) const noexcept;
  // (and six other name-lookup methods)
  ```

- **Suggested replacement**: Accept `std::string_view` for all view-only name
  and value parameters. Call sites with `const char *` literals or variables
  are unaffected (implicit conversion). Internal call chains that ultimately
  need null-terminated strings for `pool_dup` or `CmdHashMap::find` add a
  `sv.data()` / local-copy step only once at the boundary.

- **Boundary-safe**: Yes — `context.hpp` is not a frozen header. `std::string_view`
  is implicitly constructible from `const char *`, so all existing call sites
  continue to compile unchanged.

- **Rationale**: Five methods on the command-buffer side already use
  `std::string_view` (`cbuf_add_text`, `cbuf_insert_text`, `cbuf_stuff_text`,
  `cbuf_clear`-adjacent, `cmd_execute_string`). Completing the migration removes
  the inconsistency and prevents accidental passing of unterminated spans to
  internal C-string APIs.

______________________________________________________________________

### M-3: `CmdHashMap::bucket_histogram` takes a raw pointer + length

**Status: Implemented.** Changed to `std::span<std::size_t>`. `<span>` added to
`cmd_hash_map.hpp`. Call site in `context_misc.cpp` simplified to `map.bucket_histogram(hist)`.

- **File(s)**: `include/xash3dpp/private/cmd_cvar/cmd_hash_map.hpp` line ~138

- **Current pattern**:

  ```cpp
  void bucket_histogram(std::size_t *out, std::size_t count) const noexcept;
  ```

- **Suggested replacement**:

  ```cpp
  void bucket_histogram(std::span<std::size_t> out) const noexcept;
  ```

  `std::span` carries the length and is constructible from `std::array` or raw
  arrays; the caller in `context_misc.cpp` passes `hist` + `limits::cvar_hash_buckets`
  and would become `bucket_histogram(hist)`.

- **Boundary-safe**: Yes — debug-only method, not exposed outside the subsystem.

- **Rationale**: C-array + length is pattern 2-C in the audit; `std::span` is
  the C++20 replacement and reduces accidental mismatch between the array size
  and the explicit length argument.

______________________________________________________________________

### M-4: `std::snprintf` used directly instead of `utilities::snprintf`

**Status: Implemented.** All three sites (`context_init.cpp` cmdlist/cvarlist
handlers, `context_misc.cpp` `dump_hash_stats`) changed to `utilities::snprintf`.
Orphaned `#include <cstdio>` removed from both files.

- **File(s)**:

  - `src/cmd_cvar/context_init.cpp` lines ~204, ~222 (cmdlist, cvarlist handlers)
  - `src/cmd_cvar/context_misc.cpp` line ~118 (dump_hash_stats)

- **Current pattern**:

  ```cpp
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%zu command(s)\n", count);
  platform::console::write(buf);
  ```

- **Suggested replacement — Option A** (preferred, C++20):
  Since `platform::console::write` accepts `std::string_view`, use
  `std::format` directly and eliminate the intermediate buffer:

  ```cpp
  platform::console::write(std::format("{} command(s)\n", count));
  ```

  Note: `std::format` allocates and may throw `std::bad_alloc`. Evaluate
  acceptability under the no-exceptions constraint (see Open Questions).

- **Suggested replacement — Option B** (safe): Replace `std::snprintf` with
  `utilities::snprintf`, which guarantees null-termination even on truncation:

  ```cpp
  char buf[64];
  utilities::snprintf(buf, sizeof(buf), "%zu command(s)\n", count);
  platform::console::write(buf);
  ```

- **Boundary-safe**: Yes — all three sites are in built-in command handlers
  and debug utilities.

- **Rationale**: `utilities::snprintf` is the project-standard safe wrapper;
  using raw `std::snprintf` in three places is inconsistent. The `std::format`
  option is the clean C++20 path but requires verifying allocation acceptability.

______________________________________________________________________

### M-5: `std::string_view` → C-string over-read at every name-lookup site (NEW)

> **RESOLVED 2026-07-19 (HB-1, consolidation audit).** Implemented exactly as
> suggested below: `CmdHashMap` gained bounded `std::string_view`
> `find`/`remove` overloads and a size-bounded djb2 hash; `pool_dup` gained a
> bounded `string_view` overload; the six `.data()` sites in
> `cvar_ops.cpp`/`cmd_ops.cpp` now pass the view through; and the private
> `ICompatPolicy` methods take `std::string_view` (Q-17 enumeration in the
> commit). Covered by `test_unterminated_view_lookup` in
> `tests/cmd_cvar/test_cvar_registry.cpp`. Retained below for the record.

**Status: New / Remaining (found 2026-07-06).** This is the direct by-product
of M-2: the public API accepts `std::string_view`, but the lookup layer
(`CmdHashMap::hash`, `find`, `remove`) and `utilities::stricmp` all iterate
`while (*s)` and therefore assume a NUL terminator. Every lookup path resolves
the view with `name.data()` and passes it straight down, so a caller that
supplies a non-terminated `string_view` (e.g. a substring of a larger buffer)
causes an out-of-bounds read in `hash()` / `stricmp()`.

- **File(s) / sites**:

  - `src/cmd_cvar/cvar_ops.cpp` line ~29 (`cvar_find`: `const char *cname = name.data();`)
  - `src/cmd_cvar/cvar_ops.cpp` line ~101 (`cvar_get_or_create`)
  - `src/cmd_cvar/cmd_ops.cpp` line ~18 (`cmd_add`)
  - `src/cmd_cvar/cmd_ops.cpp` line ~61 (`cmd_remove`: `cmd_map.remove(name.data())`)
  - `src/cmd_cvar/cmd_ops.cpp` line ~112 (`cmd_describe`)
  - `src/cmd_cvar/cmd_ops.cpp` line ~119 (`cmd_exists`)
  - propagates into `compat_goldsrc.cpp` (`redirect_cvar_name` / `is_*` compare
    `std::string_view == const char*`, which `strlen`s the C-string)

- **Currently latent**: all in-tree callers pass NUL-terminated `const char*`
  literals from the ABI shim, so no live over-read occurs today. The hazard is
  the *contract*: the `string_view` signature invites an unterminated argument
  that the lookup layer cannot handle.

- **Suggested replacement**: add a bounded case-insensitive comparison and a
  length-aware hash so the map can be keyed by `std::string_view` directly —
  e.g. `CmdHashMap::find(std::string_view)` computing the djb2 hash over
  `sv.size()` bytes and comparing with a `ci_compare(sv, key)` that stops at
  `sv.size()`. This removes the `name.data()` step at all six sites and closes
  the class of bug at one location.

- **Boundary-safe**: Yes — `CmdHashMap` and the lookup helpers are private
  (`include/xash3dpp/private/cmd_cvar/`); no frozen header is touched.

- **Cross-subsystem**: same root cause as **utilities M-4** (`ci_less`
  string_view→`strnicmp`) and **filesystem M-7** (`archive_helpers.hpp`). A
  single bounded `ci_compare(std::string_view, std::string_view)` in
  `utilities` would fix all three. Flagged for the Phase-14 synthesis sweep.

______________________________________________________________________

## Medium-priority: new findings (2026-07-20)

### M-6: `is_filterable_exempt` has zero callers — keep it and mark it, do not delete it

**AMENDED** (digest F53). The finder's original recommended action
(option a: delete `is_filterable_exempt` and `kFilterableExemptions`) was
refuted; keep it (option b), as H-3's rationale also states.

- **File(s)**: `include/xash3dpp/private/cmd_cvar/compat_policy.hpp:34-36`;
  `src/cmd_cvar/cmd_dispatch.cpp:148-157`; `src/cmd_cvar/compat_goldsrc.cpp:40-46`

- **Current pattern**: `is_filterable_exempt` is declared, implemented
  twice (`compat_goldsrc.cpp:87`, `compat_null.cpp:16`) plus once in test
  doubles, backed by a 15-entry `constexpr` table — and called by nothing.
  `execute_tokenized` (`cmd_dispatch.cpp:135-166`) gates only on
  `FCMD_PRIVILEGED` at `:151` and never consults `cl_filterstuffcmd` (which
  *is* a registered built-in cvar, `context_init.cpp:67-76`).

- **Suggested replacement**: Add an explicit chunk marker at the
  declaration in the tree's existing style —
  `// XASH3DPP-STUB(chunk12): no caller until the client console wires the
  cl_filterstuffcmd gate at cmd_dispatch.cpp:151`, cross-referenced from
  `cmd_dispatch.cpp:151` itself.

- **Boundary-safe**: Yes.

- **Rationale**: `FCVAR_FILTERABLE` is already set on 24 production cvars
  with nothing consuming it — the surrounding machinery is live and
  actively maintained, which distinguishes this from `ICvarObserver`
  (H-4): a real future caller (Chunk 12's client console) is named, only
  its chunk marker is missing.

- **ext_tags**: G-1, Q-12

______________________________________________________________________

### M-7: The cvar write-tail is hand-copied into `cvar_set_direct` and `cvar_full_set` — and the two copies have already drifted

**`[EXT:G-3]`** (digest F51, UNVERIFIED — well-evidenced, included per the
brief's rule for well-evidenced unverified findings).

- **File(s)**: `src/cmd_cvar/cvar_ops.cpp:200-231` (`cvar_set_direct`);
  `src/cmd_cvar/cvar_ops.cpp:368-385` (`cvar_full_set`);
  `src/cmd_cvar/cvar_ops.cpp:239-245`

- **Current pattern**: Both write paths open-code the identical seven-step
  tail (set `FCVAR_CHANGED`, release-bump `generation`, `XASH_STATS`
  `write_count`/`last_write_source`, observer mask-dispatch,
  `cvars_written` counter). The comment at `:368` explicitly claims "same
  write-tail as `cvar_set_direct`" — it is not: the `full_set` copy
  silently omits the `XASH_DEBUG_CVARS` change-log-append branch that
  `cvar_set_direct` has.

- **Suggested replacement**: Extract one file-local
  `static void cvar_write_tail(Impl &impl, Cvar *cv, const char *old_value,
  CvarWriteSource source) noexcept` in `cvar_ops.cpp` and call it from all
  three write sites (`cvar_set_direct`, `cvar_full_set`, and the
  auto-create branch of `cvar_set` after `get_or_create`). Net deletion of
  roughly 25 duplicated lines.

- **Boundary-safe**: Yes — internal helper, no ABI or frozen-header change.

- **Rationale**: The two copies have *already* drifted (the missing
  `XASH_DEBUG_CVARS` branch), which is the live proof this class of
  duplication regresses silently. `Cvar::generation`'s release-`fetch_add`
  (see L1-R5, Low-priority section below) lives inside this tail, so
  unifying it also removes one of the two places that counter can drift
  out of sync.

______________________________________________________________________

### M-8: Verbatim intrusive-list-unlink duplication — collapse the `alias`/`unalias` copy only

**AMENDED** (digest F52). The original two-part proposal survives in part
and is refuted in part; do only the surviving part.

- **File(s)**: `src/cmd_cvar/context_init.cpp:131-141` (`alias`'s one-arg
  delete path); `src/cmd_cvar/context_init.cpp:167-177` (`unalias`)

- **Current pattern**: The two sites are character-for-character the same
  six-line head/tail-pointer-to-pointer intrusive-list rebuild inside two
  adjacent lambdas, each paired with an `alias_map.remove` call and the
  same two-step free — with one asymmetry: `:131` discards the
  `[[nodiscard]]` result of `CmdHashMap::remove` (`cmd_hash_map.hpp:100-102`)
  while `:166` binds it.

- **Suggested replacement**: Extract one file-local
  `static void alias_erase(Impl &impl, const char *name) noexcept` in
  `context_init.cpp` encapsulating map-remove + list-rebuild + two-step
  free, and call it from both the `alias` one-arg delete path and
  `unalias`. Pure deletion of ~10 lines, no new type, no header change; it
  also removes the discarded-`[[nodiscard]]` construct at `:131`.

- **Boundary-safe**: Yes.

- **Rationale — what to *not* do**: The finding's original step (b)
  proposed unifying this with the four other "unlink one node" loops
  across `cmd_ops.cpp` (`cmd_remove`, `cmd_unlink`) and `cvar_ops.cpp`
  (`cvar_unlink`) by adding an `insert_front`/generic-unlink API to
  `CmdHashMap`. **Refuted**: reading `cmd_ops.cpp:92-109` (`cmd_remove`),
  `cmd_ops.cpp:118-134` (`cmd_unlink`), and `cvar_ops.cpp:255-295`
  (`cvar_unlink`) shows they are not five instances of one operation —
  `cmd_remove` is erase-one-known-node, while `cmd_unlink`/`cvar_unlink`
  are filter-many-nodes-by-predicate. Building a shared abstraction across
  two genuinely different shapes for the sake of five call sites is the
  §6 gold-plating failure the brief warns against; a fourth registry that
  actually needs the "unlink one known node" shape can copy the extracted
  `alias_erase` pattern, not a generic list API. Record this refutation so
  it is not re-derived.

______________________________________________________________________

### M-9: Two `const_cast<this>` exist only because `cvar_find` lacks a `const` overload

**`[EXT:G-3]`** — tier bumped from the digest's original Low (F55) because
it blocks a read-only, off-Main-eligible accessor path.

- **File(s)**: `src/cmd_cvar/cvar_ops.cpp:318-323`, `:325-330`;
  `include/xash3dpp/private/cmd_cvar/cmd_hash_map.hpp:69-77`;
  `include/xash3dpp/cmd_cvar/context.hpp:73`

- **Current pattern**:

  ```cpp
  const Cvar *cv = const_cast<CmdCvarContext *>(this)->cvar_find(name);
  ```

  `cvar_find` (`context.hpp:73`) is non-`const` purely by omission — its
  body does nothing but call `ICompatPolicy::redirect_cvar_name` (a
  `const` method) and `CmdHashMap::find` (already declared `const` at
  `cmd_hash_map.hpp:69`). The underlying hash map is already
  const-correct; the C-shaped omission is at one call site up.

- **Suggested replacement**: Add
  `[[nodiscard]] const Cvar *cvar_find(std::string_view name) const noexcept;`
  beside the existing overload (same three-line body with a `const`
  `CmdHashMap::find`), and delete both `const_cast`s plus their SAFETY
  comments. Note `cvar_get_list() const` (`cvar_ops.cpp:298`) legitimately
  returns a mutable `CvarAbi *` — that one is frozen-ABI-mandated and must
  stay as-is.

- **Boundary-safe**: Yes.

- **Rationale**: This is C-shaped by inertia, not by any real constraint —
  the underlying map is already const-correct. It is a small piece of the
  Main-only cvar-read surface threading-model.md §8.3 discusses (M-16
  below); a genuinely `const`-qualified read path is a cheap precondition
  for any future read-only accessor, whether or not the shared_mutex
  retrofit itself is ever built.

______________________________________________________________________

### M-10: Three helper functions take `auto &impl` solely to avoid naming a private nested type

- **File(s)**: `src/cmd_cvar/cmd_ops.cpp:12-24`; `src/cmd_cvar/cmd_dispatch.cpp:65-80`,
  `:134-136`; `include/xash3dpp/cmd_cvar/context.hpp:224-231`

- **Current pattern**: `cmd_add_impl`, `dispatch_cmd`, and
  `execute_tokenized` all take `auto &impl` — a deduced, unconstrained
  template parameter — because `CmdCvarContext::Impl` is private and a
  free function cannot name it (the comments at `cmd_ops.cpp:12-16` and
  `cmd_dispatch.cpp:65-70` state this motive explicitly). Consequence: each
  helper accepts *any* type with the right member names, with zero
  compile-time structural checking.

- **Suggested replacement**: Move `cmd_add_impl` into `CmdCvarContext` as a
  private static member (or a member of `Impl`), and make `dispatch_cmd`/
  `execute_tokenized` private member functions of `CmdCvarContext`. At that
  point `impl` becomes `impl_`, the separate `ctx` parameter disappears
  from both signatures, mutual-recursion forward declarations are no
  longer needed, and the three functions get real, checked parameter
  types. No behaviour change.

- **Boundary-safe**: Yes — purely internal restructuring.

- **Rationale**: An unconstrained `auto &impl` template silently accepts a
  wrong type with matching member names; three call sites currently rely
  on this working by convention rather than by the type system enforcing
  it.

______________________________________________________________________

### M-11: `cbuf_stuff_text` is the odd one of three enqueue entry points — skips splitting and the high-water stat

**CONFIRMED** (digest F57) — `[EXT:G-1]`, tier bumped from Low because the
asymmetry sits directly on the untrusted-stuffcmd queue G-1's `stuffcmd`
trust gate (D2) depends on.

- **File(s)**: `src/cmd_cvar/cmd_ops.cpp:219-222`, `:200-217`;
  `src/cmd_cvar/cmd_dispatch.cpp:199-205`

- **Current pattern**: Three public ways to enqueue text, three shapes.
  `cbuf_add_text` splits on quote-aware `;`/`\n` boundaries **and**
  maintains `buffer_high_water`; `cbuf_insert_text` splits but does not
  touch the stat; `cbuf_stuff_text` does neither — it pushes the raw text
  as one queue entry. Verified against `cmd_dispatch.cpp:79-131,182-206`:
  `dispatch_cmd`'s internal `;` loop never re-checks `cmd_wait`, and
  `cbuf_execute` only checks between queue entries, so a whole
  multi-command `stuffcmd` line gets asymmetric `wait` granularity versus
  the other two entry points.

- **Suggested replacement**: Route `cbuf_stuff_text` through
  `cbuf_split_push(impl_->filteredcmd_text, text, false)` so all three
  entry points share one splitting rule, and factor the high-water update
  into a small shared helper applied to whichever deque was just pushed
  (so the untrusted queue is measured too). If the single-entry behaviour
  is deliberate legacy parity for `Cbuf_AddFilteredText`, document that at
  `cmd_ops.cpp:219` instead of changing it — verify against
  `engine/common/cmd.c` before choosing.

- **Boundary-safe**: NeedsVerification — the fix itself touches no frozen
  ABI, but whether the current behaviour is intentional legacy parity
  needs a look at `engine/common/cmd.c` before either branch is taken.

______________________________________________________________________

### M-12: Close cmd_cvar's thread-assert gap before any cvar threading change is contemplated

From cross-cutting lens L8 (`thread-model-packet`), §4 "Sizing the
cmd_cvar `shared_mutex` retrofit" — `[EXT:P-8]`.

- **File(s)**: `src/cmd_cvar/context_init.cpp` (2 `assert_thread_role`
  sites); `src/cmd_cvar/context_misc.cpp` (2 sites); all nine other
  `cmd_cvar` translation units (0 sites)

- **Current pattern**: `cmd_cvar` has **4** `assert_thread_role` call sites
  across a ~40-entry-point public surface, and **zero**
  `compliance-allow(thread-assert)` waivers. The subsystem's "Main-only"
  claim — which `threading-model.md` §8.3 and `decisions-architecture.md`
  §2.5 both rest on — is therefore enforced on roughly 10% of the surface,
  and asserted nowhere on the read path that the tree's 40 production
  cvar-read call sites (sound 17, server 23, including `pm_trace.cpp` ×2
  and `hulls.cpp` ×2 on the physics hot path) actually use. Compare `save`
  (37 assert sites, 0 waivers) and `server` (101 sites, 2 waivers) —
  `cmd_cvar` is the tree's outlier on this metric, and it is also the
  subsystem the design doc names as the highest-priority retrofit
  candidate.

- **Suggested replacement**: Add `assert_thread_role(ThreadRole::Main)` to
  `cmd_cvar`'s public mutating and lookup entry points. This costs nothing
  under "stay Main-only" (the status quo) and is the mandatory precondition
  for ever measuring which callers a future off-main retrofit would break.

- **Boundary-safe**: Yes — assert-only, no behavioural change on a correct
  caller. Asserts are `XASH_FATAL` and always-on; measure the cost on the
  physics-path callers (`pm_trace.cpp`, `hulls.cpp`) before landing, since
  those are hot paths.

- **Rationale**: This is the one piece of the cvar-threading story that is
  **not** speculative — it is owed today against a claim two binding
  documents already make, independent of whether the retrofit itself
  (M-16) is ever built.

______________________________________________________________________

### M-13: `cmd_cvar`'s CMake link visibility — `memory`/`utilities` are `PUBLIC` but no public header uses them

- **File(s)**: `src/cmd_cvar/CMakeLists.txt:35-36`

- **Current pattern**:

  ```cmake
  target_link_libraries(xash3dpp_cmd_cvar
      PUBLIC  xash3dpp_utilities
      PUBLIC  xash3dpp_memory
      ...)
  ```

  `grep '#include <xash3dpp/(memory|utilities)/' ` over
  `include/xash3dpp/cmd_cvar/**` returns zero hits — the public API
  (`context.hpp`, `cvar.hpp`, `command.hpp`, `observers.hpp`, `stats.hpp`)
  only pulls in `<atomic>`/`<cstdint>`/`<string_view>` and other `cmd_cvar`
  public headers. Both `memory::` (`PoolHandle`, `mem_alloc`) and
  `utilities::` (`string`, `ci_compare`) are used only by the private
  `Impl` (`context_impl.hpp`).

- **Suggested replacement**: Change both to `PRIVATE`, matching `core` and
  `platform`'s existing pattern. No consumer needs a build-graph change —
  every current consumer already links `memory`/`utilities` directly.

- **Boundary-safe**: Yes — pure CMake visibility tightening.

- **Rationale**: A `PUBLIC` link that no public header requires forces
  every downstream consumer to transitively re-link two libraries it may
  not otherwise need, and misrepresents the subsystem's actual public
  dependency surface.

______________________________________________________________________

### M-14: No "Module statics" table exists; the boundary doc's P-3 row claims a "sole global" the code contradicts

**`[EXT:P-3]` `[EXT:P-8]`** — tier bumped from the digest's original Low
(F58). Cross-referenced with lens L4's recommendation (L4-R4) to point the
tree-wide P-3 rule at a section every boundary doc already has, rather
than adding a 17th empty table.

- **File(s)**: `xash3dpp/docs/boundaries/cmd_cvar-boundary.md:414-456`;
  `src/cmd_cvar/context.cpp:17-19`; `src/cmd_cvar/compat_goldsrc.cpp:102-106`

- **Current pattern**: Neither `cmd_cvar-boundary.md` nor
  `docs/architecture/cmd_cvar/` contains a "Module statics" table (the
  literal heading the P-3 door rule at `extension-goals.md:202-206`
  requires), so the subsystem's file-scope mutable state is only
  discoverable by reading source. The Q-21 P-3 row currently claims the
  host's `g_cmd_cvar` pointer is "the sole global" — the tree-wide
  `module_statics` scan's false negative is `thread_local CmdCvarContext
  *tls_ctx` (`private/cmd_cvar/context_impl.hpp:125`, set/cleared around
  each dispatch at `cmd_dispatch.cpp:152/155`), which the regex-based
  fact base missed entirely. `cmd_cvar-boundary.md:414` *does* already
  document `tls_ctx` under its existing "Owned state" section — the doc is
  internally inconsistent, not silent.

- **Suggested replacement**: This tree-wide, not cmd_cvar-specific.
  `cmd_cvar` is one of only 3 of 16 subsystems with statics-table content
  under any heading, while all 16 boundary docs already have an "Owned
  state" section where authors, including this one, already write this
  down. Point the P-3 door rule at "Owned state" instead of a
  "Module statics" table that exists in 1 of 16 boundary docs; reword this
  subsystem's P-3 row from "the sole global is `g_cmd_cvar`" to "the only
  file-scope mutable state is the ABI-shim pointer plus the documented
  `thread_local tls_ctx` (see Owned state)."

- **Boundary-safe**: Documentation only.

- **Rationale**: Asking every subsystem to duplicate its already-written
  "Owned state" content into a differently-named table is the exact
  gold-plating the anti-gold-plating rule targets — a rule that is
  unsatisfiable as written for 4 of 16 subsystems should be pointed at the
  section authors already use, not multiplied by 16.

______________________________________________________________________

### M-15: Record `Cvar::generation` as a publisher with zero consumers

From cross-cutting lens L1 (`publish-primitive`), recommendation L1-R5 —
`[EXT:P-2]`. Corrects a fact-base error the campaign's own brief carried.

- **File(s)**: `include/xash3dpp/cmd_cvar/cvar.hpp:130`;
  `src/cmd_cvar/cvar_ops.cpp:204`, `:370`

- **Current pattern**: `cv->generation.fetch_add(1u,
  std::memory_order_release)` fires at both cvar write paths — **two**
  release publishers. `grep -rn '\.load()'` against `generation` across
  `src/`, `include/`, and `tests/` returns **zero** hits: nothing ever
  reads it.

- **Suggested replacement**: Not a code change. Add a boundary-spec row to
  `cmd_cvar-boundary.md`'s Extension-axes P-2 section stating the field has
  two release publishers and zero consumers, and record the decision to
  **keep** it — 4 bytes per `Cvar` plus one release `fetch_add` per write
  is the tree's only ready-made lock-free change-epoch and the natural
  first test case for any future HB-5 published-snapshot brief — with the
  explicit caveat that it is *not* evidence of a working publish mechanism
  and must not be cited or imitated as one.

- **Boundary-safe**: Documentation only.

- **Rationale**: This campaign's own common brief originally characterized
  `generation` as having "no publisher at all" — backwards. Without a
  boundary row stating the true shape, the next audit will either
  re-flag it as a defect or miscount it toward the tree's published-
  snapshot mechanism tally, which has already happened once (to this
  exact field) inside this campaign.

______________________________________________________________________

### M-16: `threading-model.md` §8.3's cmd_cvar retrofit description is wrong as written — "add a `shared_mutex`" would fix nothing

From cross-cutting lens L8, recommendation L8-R4.

- **File(s)**: `docs/design/threading-model.md` §8.3 and §11 (cvar-read
  row); `src/cmd_cvar/cvar_ops.cpp:185`, `:357` (the frees that make the
  naive fix unsound); `src/cmd_cvar/context.cpp:19` (`tls_ctx`)

- **Current pattern**: §8.3 currently reads "a `shared_mutex` will be
  added to permit concurrent readers" and calls this "the highest-priority
  retrofit candidate." As written, this is unsound: `cvar_variable_string`
  returns a raw `const char *` that `cvar_set_direct` (`:185`) and
  `cvar_full_set` (`:357`) later `mem_free`. A `shared_lock` held only for
  the duration of the lookup returns a pointer that **escapes the lock
  scope** — a lock that compiles clean and produces a use-after-free that
  looks synchronised.

- **Suggested replacement**: Replace the sentence with the four-part shape:
  (i) `shared_mutex` on the registry (necessary but insufficient alone);
  (ii) a **value-returning** off-main read API —
  `cvar_value(name) -> float`, `cvar_integer(name) -> int`,
  `cvar_string(name, std::span<char> out) -> size_t` — without which (i)
  is decorative; (iii) an explicit, permanent Main-only fence on
  `cvar_find`/`cvar_get_list`, because `pfnCVarGetPointer` hands the game
  DLL an escaping `cvar_t *` — frozen ABI, not a design choice; (iv) the
  `tls_ctx` `thread_local` trap (`context.cpp:19`) under which an off-main
  command dispatch silently **no-ops** rather than crashing or asserting.
  Also record the shipped precedent that already solves the tree's one
  real instance of this problem with zero `cmd_cvar` changes: sound reads
  all 17 of its cvars on Main, packs them into a `MixConfigSnapshot` POD,
  and pushes it across the MPSC `AudioCommand` queue
  (`dsp.cpp:60-74`; applied decoder-side at `audio_command.cpp:263-275`).

- **Boundary-safe**: Documentation only; the retrofit itself has no named
  consumer today (see Open questions) and must not be built speculatively.

- **Rationale**: Someone will eventually execute the retrofit from that
  one sentence in §8.3. Correcting the description costs nothing now and
  is the entire value of deferring the actual work — see the Open
  questions section for the full precondition chain (M-12's assert gap
  must close first).

______________________________________________________________________

## Low-priority / cosmetic opportunities

| # | File(s) | Current | Suggested | Status |
|---|---------|---------|-----------|--------|
| L-1 | `context_impl.hpp` line ~51 | `ObserverEntry observers[limits::cmd_observer_max]` | `std::array<ObserverEntry, limits::cmd_observer_max>` | **Implemented** (2026-07-06) |
| L-2 | `context_impl.hpp` lines ~73–74 | `const char *tok_argv[k_max_argc]`, `char tok_argsBuffer[cmd_line_max]` | `std::array<const char*, k_max_argc>`, `std::array<char, cmd_line_max>` | **Implemented** (2026-07-06) |
| L-3 | `context_misc.cpp` ~L110 | `std::size_t hist[limits::cvar_hash_buckets] = {}` | `std::array<std::size_t, limits::cvar_hash_buckets> hist {}` | **Implemented** (2026-07-06) |
| L-4 | `context_misc.cpp` ~L112 | `MapStats maps[]` (local array) | `std::array<MapStats, 3> maps` | **Implemented** (2026-07-06) |
| L-5 | `compat_goldsrc.cpp` lines ~37–57 | `constexpr const char *kFilterableExemptions[]`, `kOverridableCommands[]` | `constexpr std::array<std::string_view, N>` | **Implemented** (2026-07-06) |
| L-6 | `context_impl.hpp` ~L122 | `utilities::strlen(src)` in `pool_dup` — null guard already above | `std::strlen(src)` (src guaranteed non-null at that point) | **Implemented** (2026-07-06) |
| L-7 | `compat_goldsrc.cpp:61-68`, `:78-85` | bespoke `cstr_span_contains` template for two flat `string_view` tables; open-coded range-for over the `{from,to}` pair table | `std::ranges::contains(kFilterableExemptions, sv)` / `std::ranges::find(kCvarRedirects, sv, &Entry::from)` | **New (2026-07-20)** — see below |

### L-7 detail: hand-rolled table lookups where `std::ranges` is a real (small) gap

- **File(s)**: `src/cmd_cvar/compat_goldsrc.cpp:61-68` (`cstr_span_contains`,
  used at `:89`, `:94`); `:78-85` (open-coded range-for over the
  `{from,to}` redirect pairs)

- **Current pattern**: Three `constexpr` tables, two lookup idioms — a
  bespoke `cstr_span_contains` function template for the two flat
  `string_view` arrays, and an open-coded range-for for the pair array.
  Neither is wrong; the helper exists only because the standard spelling
  was not used, and it is a 6-line template C++23 makes redundant.

- **Suggested replacement**: Delete `cstr_span_contains` and rewrite the
  three lookups with `std::ranges::contains` / `std::ranges::find` plus a
  projection. **This would be the tree's first `std::ranges` use**
  (`facts.json`: 0 tree-wide) — verify both x86 and x64 accept it under
  `/std:c++latest` before committing; treat it as a small portability
  probe, not a drive-by change.

- **Boundary-safe**: Yes — internal to `compat_goldsrc.cpp`.

- **Rationale**: Low priority and low blast radius (3 call sites). Only
  worth doing when H-3 (wiring `get_compat_policy()`) or M-6
  (`is_filterable_exempt`'s chunk marker) already has this file open —
  do not open it for this alone.

______________________________________________________________________

## Investigated and refuted

These were considered this pass and rejected; recorded so a future audit
does not re-derive and re-reject them.

- **"cmd_cvar's `generation` counter has no publisher at all."** Backwards.
  It has **two** release publishers (`cvar_ops.cpp:204`, `:370`) and zero
  consumers — see M-15. This was the campaign's own common-brief framing,
  corrected by lens L1 after reading the code; recorded here so nobody
  re-imports the original (wrong) framing from an older draft of this
  document or the campaign brief.
- **Unifying all five intrusive-list-unlink loops (`cmd_ops.cpp`,
  `cvar_ops.cpp`, `context_init.cpp` ×2) behind one generic
  `CmdHashMap::insert_front`/unlink API.** Refuted at M-8: only two of the
  five loops share the "unlink one known node" shape the finding claimed
  for all five; `cmd_unlink`/`cvar_unlink` are filter-many-by-predicate,
  a genuinely different operation. Only the two verbatim `alias`/`unalias`
  copies were consolidated.
- **Deleting `is_filterable_exempt` and `kFilterableExemptions` alongside
  `ICvarObserver`** (the original form of digest F53's proposed action).
  Refuted: `FCVAR_FILTERABLE` is already set on 24 production cvars with
  nothing consuming it yet — a real, named Chunk-12 consumer, unlike
  `ICvarObserver`'s absence of one. See M-6 and the H-4/H-3 contrast in
  H-3's rationale for the general test ("does this code have a
  behavioural contract derived from the legacy engine or a named future
  consumer, or only a hoped-for one?").
- **Retiering H-1/H-2 out of the document entirely.** Considered and
  rejected — this campaign's rule is to mark resolved findings resolved
  in place with evidence, not delete them; a report that silently drops a
  finding is untrustworthy. They stay, retiered to informational (see the
  2026-07-20 re-verification notes under each).

______________________________________________________________________

## Out of scope / ABI-frozen

| Symbol / pattern | Why it must not change |
|------------------|----------------------|
| `CvarAbi` field order and sizes | `cvar_t`-compatible layout; game DLLs dereference `name`, `string`, `flags`, `value`, `next` by offset. Any reorder silently breaks all DLL builds. |
| `CvarAbi::next` typed as `CvarAbi *` | `cvar_t::next` is `struct cvar_s *` in legacy headers; DLLs cast the list head to `cvar_t *` and walk `.next` directly. The engine-side `Cvar *` must be the same object, hence `reinterpret_cast` is required and cannot be removed — only encapsulated (H-1). |
| `CvarFlags` numeric values | Frozen by `common/cvardef.h`. Values are compared by game DLL code compiled against the SDK. The enum wrapper itself is free to change shape but the integer values are frozen. |
| `CommandFn = void (*)()` | ABI-compatible with legacy `xcommand_t`. Cannot become `std::function` (different calling convention and layout). |
| `pfnCvar_RegisterVariable`, `pfnCVarGetPointer`, `pfnCvar_DirectSet`, `pfnAddServerCommand`, all `pfnRegister*` / `pfnGetCvar*` / `pfnAddCommand` / `pfnClientCmd` / `pfnServerCmd` | Defined in `engine/eiface.h`, `engine/cdll_int.h`, `engine/menu_int.h` — fully frozen SDK headers. |
| `cvar_find` / `cvar_get_list` — permanently Main-only (**new 2026-07-20**) | Not a design choice: `pfnCVarGetPointer` hands the game DLL an escaping `cvar_t *` (frozen ABI signature). No off-main read API may ever return `const char *` or `Cvar *`/`CvarAbi *` for this reason — see M-16. Off-main reads must be value-returning (`float`/`int`/span-copied string) if they are ever built. |

______________________________________________________________________

## Open questions

1. **`std::format` under `/EHs-c-`**: `std::format` can throw `std::bad_alloc`
   and `std::format_error`. With MSVC's `/EHs-c-` the throw cannot be caught.
   Determine whether the project's OOM strategy (abort) makes this acceptable
   for non-critical display paths (`cmdlist`, `cvarlist`, `dump_hash_stats`),
   or whether `utilities::snprintf` (already chosen for M-4) is the permanent
   solution. `std::format` remains an option once the no-exceptions stance is
   re-evaluated.

2. **Cvar storage ownership (H-6)**: fall-through lookup only (S), indirect
   storage via an optional `CvarAbi *storage` on the registry node (M,
   recommended), or a write-through mirror (S, not recommended — repeats
   the hand-synced-mirror pattern that already produced a live defect
   elsewhere in the ABI bridge)? This must be decided before Chunk 12
   wires the client DLL's cvar surface, or a third registry chain gets
   written. Owner: cmd_cvar/server boundary owners jointly (the other half
   of the split, `EngineBridge::external_cvars`, lives in `server`).

3. **Is the cross-thread cvar-read retrofit (M-16) ever built, and who
   forces it?** Nothing in the tree forces it today — networking reads
   zero cvars, and sound's `MixConfigSnapshot` precedent already solves
   the one real cross-thread cvar-configuration problem the tree has had,
   with zero `cmd_cvar` changes. It would be forced by a G-3 debug thread
   enumerating cvars, or a render thread reading `gl_*` per frame — both
   speculative today (no chunk number). Until one of those lands, this
   stays a shape constraint (M-16), not work, and M-12's assert-gap
   closure is the only piece of it that is owed regardless.

4. **`ICvarObserver` shape reconstruction (H-4)**: settled this pass as
   "delete now, reconstruct from the recorded shape when a real consumer
   appears" — flagged here only because the counter-position (keep +
   chunk marker) is still defensible in principle if a Chunk-12/G-1 owner
   wants to attach a chunk number instead of deleting. Revisit if Chunk 12
   kickoff names a concrete consumer before this lands.
