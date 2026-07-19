# Consolidation Audit — 2026-07-19

Full-tree verification of all 13 implemented subsystems across five
dimensions, followed by a gated fix wave. Campaign plan approved by the
user (one campaign; bounded HB items folded in; full agent matrix), with
one mid-campaign escalation batch (host pump: **wire now**; MapLoader
stats: **add**; studio tripwire: **implement now**).

## How it ran

- **P0** — deterministic scans (compliance/limits/stub/crosswalk/status/
  finish x13) captured as the agents' fact base; the missing Q-21 lens
  authored as permanent workflow machinery (`extension-door-auditor`
  charter + `/audit-extension-doors` prompt, commit 9897b48d).
- **P1** — 53 read-only verifier agents via one Workflow run
  (13 subsystem packs x D1 doc-accuracy / D2 binding rules / D3 Q-21
  extension posture / D4 threading+modernization currency, + 1 D5
  abi-watchdog baseline). 53/53 completed, 0 errors, ~3.54M subagent
  tokens, 12 min wall clock. Models tiered: opus on the four
  parity-critical subsystems' D1, sonnet for judgment lenses, haiku for
  mechanical tiers.
- **P2** — every finding reproduced from evidence before acceptance and
  dispositioned four-way (fix / compliance-allow / false-positive /
  deferred); no silent drops. All six blockers reproduced exactly.
- **P3** — 14 gated commits (one per subsystem + cross-cutting), each
  build+test green at commit time.
- **P4** — exit gates below.

## Verdict matrix (P1, before fixes)

| Subsystem | D1 doc-accuracy | D2 rules | D3 Q-21 | D4 thr+mod | Findings |
|---|---|---|---|---|---|
| server | 32c/1f | 27c/1f | CONTRADICTED (P-1) | clean | 9 |
| networking | 24c/3f | 106c/4f | GAPS | 2f (path/line refs) | 14 |
| map_loader | 22c/1f | 27c/3f | GAPS | clean | 9 |
| content | 30c/1f | 14c/3f | GAPS | clean | 10 |
| filesystem | 17c/1f | 25c/1f | CONTRADICTED (P-7) | clean | 7 |
| cmd_cvar | 24c/4f | 13c/1f | GAPS | clean | 11 |
| host | 24c/0f | 15c/0f | GAPS | clean | 5 |
| platform | 17c/1f | 62c/0f | GAPS | clean | 4 |
| utilities | 24c/1f | 8c/0f | GAPS | clean | 4 |
| core | 14c/3f | 14c/0f | GAPS | clean | 8 |
| memory | 34c/2f | 6c/0f | GAPS | clean | 5 |
| abi | 47c/2f | 43c/0f | GAPS | clean | 6 |
| launcher | 26c/0f | 10c/0f | GAPS | clean | 1 |
| **D5 tree** | | | | | CLEAR + 1 warn |

`Nc/Mf` = N claims verified, M findings. 94 findings total: 6 blockers,
88 warnings. The dominant pattern (56 findings) was the systematic
Extension-axes lag behind the current G-1..G-5 / P-1..P-8 set — the
root cause (a hardcoded axis list in the Q-21 mandate text) is fixed.

## Headline results

1. **Host frame pump wired** — `Host::RunFrame` never called
   `Server::frame()` in production (the Chunk-6 TODO survived
   completion; the milestone frame ran via the test harness), and
   `Clock::tick()` was never called anywhere. Both wired (93860120);
   the server-boundary P-1 row that wrongly claimed otherwise corrected.
2. **HB-1 security fix landed end-to-end** — bounded `ci_compare` in
   utilities (ebeb1763); filesystem's own `strnicmp` over-read in
   `ci_find_by_name` closed (b1a7b4de); cmd_cvar's six `.data()`
   lookup sites + `CmdHashMap` + `pool_dup` + the private
   `ICompatPolicy` seam made view-bounded end-to-end, with an
   unterminated-slice regression test (5befba72).
3. **HB-3 closed** — host (3 sites) + map_loader (5 sites incl. the
   observer mutators) now assert `ThreadRole::Main`; server was already
   the clean reference.
4. **Studio ABI layout tripwire shipped** — `test_studio_layout.cpp`
   pins 7 strides + 26 header offsets + sub-struct offsets against the
   sealed real `engine/studio.h`; compiled clean first try on both
   arches, independently confirming the hand-verification.
5. **Retired-P-7 pattern** — three boundary docs (filesystem, platform,
   launcher) answered the pre-2026-07-06 "over-aligned allocation"
   meaning of P-7; all rewritten to the current pool-owned-RAII axis.
   (Two were found by cross-checking after the D3 agents accepted one —
   the audit's own verdict-consistency check earning its keep.)
6. **False annotations corrected** — the delta codec's
   'stateless/pure' compliance-allow rationales and the
   `T_NetIO-confined` claims contradicted the subsystem's own threading
   doc; all 12 rewritten to the sim-thread contract.

## Exit gates (P4)

- build + ctest **x64**: 0 errors, 94/94 (was 93; +test_studio_layout)
- build + ctest **x86**: 0 errors, 94/94 (bit-exact goldens hold on the
  32-bit hl.dll chain)
- `status_table --check`: **drift = [] (clean)** — HB-10 reconciled
- abi-watchdog post-fix re-run: **CLEAR, 0 blockers / 0 warnings**
  (baseline warning — the missing tripwire — resolved)
- `workflow_sync` stage 1+2: 0 findings
- `markdown_lint`: 0 issues on every touched doc
- Stub debt moved by the wave: host 10 → 7 (pump TODOs consumed),
  content 2 → 1 (tripwire item done)

## Deferred with owner

- **HB-4..HB-7** (DOOR class) — each needs its own design brief before
  code; unchanged.
- **HB-12** shared deterministic RNG — future harmonization session.
- `SV_ClipMoveToEntity` studio hitbox trace-loop — remains gated on the
  hl.dll smoke (Chunk 7 finishing step; unchanged by this audit).
- content `docs/architecture/content/` — authored at Chunk 7 close
  (`document-architecture`).

## Door-debt register

No **unrecorded** door-rule deviations were found (D3, all 13
subsystems). Standing recorded debts: networking's 55
`compliance-allow(thread-assert)` sites flip to `T_NetIO` asserts at
the NetIO split (boundary flip table authoritative); server P-2
introspection snapshot is bring-up work for G-1/G-3; host P-1 inbox
lands with the Chunk-7 queue family.

## Adjudication ledger

Dispositions: 81 fix-doc, 7 fix-code, 2 false-positive (limits-scan
candidate classes working as designed; recorded, no scanner change),
2 compliance-allow, 2 escalated-to-user (both approved as fixes),
plus the 11 tracked cross-cutting work items below.

| Subsystem | Ref | Sev | Verdict | Disposition | Adjudication / claim |
|---|---|---|---|---|---|
| server | D1#0 | warn | STALE | fix-doc | HB-10: status table server src 1->30 (+host 2->3, utilities 9->10) |
| server | D2#0 | warn | violation | fix-code | physics.cpp:50 k_max_clip_planes -> xash::limits::server_clip_planes (same value 5, no behaviour change) |
| server | D3#0 | blocker | contradicted | fix-doc | P-1 row rewrite: drain lives in physics.cpp host_server_frame via Server::frame(); host pump NOT wired (host.cpp:233 TODO). + HB-11 line. Wiring itself = escala |
| server | D3#1 | warn | missing | fix-doc | add G-4 row to server-boundary Extension axes |
| server | D3#2 | warn | missing | fix-doc | add G-5 row to server-boundary Extension axes |
| server | D3#3 | warn | missing | fix-doc | add P-5 row to server-boundary Extension axes |
| server | D3#4 | warn | missing | fix-doc | add P-6 row to server-boundary Extension axes |
| server | D3#5 | warn | missing | fix-doc | add P-7 row to server-boundary Extension axes |
| server | D3#6 | warn | missing | fix-doc | add P-8 row to server-boundary Extension axes |
| networking | D1#0 | warn | WRONG | fix-doc | boundary P-3 row: GoldSrc driver shipped, not deferred TODO |
| networking | D1#1 | warn | STALE | fix-doc | arch protocol-driver.md null-registry claim: both drivers available |
| networking | D1#2 | warn | WRONG | fix-doc | find_driver() -> resolve() |
| networking | D2#0 | blocker | violation | fix-code | delta_codec.cpp allow rationales: 'stateless pure' is false -> 'single-thread caller contract (sim thread; Main today), mutates Impl delta state' |
| networking | D2#1 | warn | violation | fix-code | delta.hpp @thread-safety: T_NetIO-confined -> sim-thread-confined (matches threading doc:22-24) |
| networking | D2#2 | warn | false-positive-candidate | false-positive | limits candidates: wire-frozen constants, correctly local per detail-audit exemption; candidate class working as designed |
| networking | D2#3 | warn | false-positive-candidate | false-positive | shadow candidates: value coincidences across unrelated domains |
| networking | D3#0 | warn | missing | fix-doc | add P-7 row to networking-boundary Extension axes |
| networking | D3#1 | warn | missing | fix-doc | add P-8 row to networking-boundary Extension axes |
| networking | D3#2 | warn | missing | fix-doc | add G-3 row to networking-boundary Extension axes |
| networking | D3#3 | warn | missing | fix-doc | add G-4 row to networking-boundary Extension axes |
| networking | D3#4 | warn | missing | fix-doc | add G-5 row to networking-boundary Extension axes |
| networking | D4#0 | warn | WRONG | fix-doc | threading doc path: src/networking/context_impl.hpp -> include/xash3dpp/private/networking/context_impl.hpp |
| networking | D4#1 | warn | WRONG | fix-doc | line refs 100/108 -> 101/109 |
| map_loader | D1#0 | warn | STALE | fix-doc | boundary §2: add private/map_loader/fat_vis.hpp to inventory |
| map_loader | D2#0 | warn | missing | fix-code | HB-3: add assert_thread_role(Main) to new_game/change_level/clear_world + extend header enumeration |
| map_loader | D2#1 | warn | contradicted | escalate | MapLoader stats(): rule-compliant Stats struct vs compliance-allow (Q2) |
| map_loader | D2#2 | warn | false-positive-candidate | compliance-allow | disk_format.hpp constants are frozen on-disk BSP format, exempt per CHECK-LIMITS wire carve-out |
| map_loader | D3#0 | warn | missing | fix-doc | add G-4 row to map_loader-boundary Extension axes |
| map_loader | D3#1 | warn | missing | fix-doc | add G-1 row to map_loader-boundary Extension axes |
| map_loader | D3#2 | warn | missing | fix-doc | add G-5 row to map_loader-boundary Extension axes |
| map_loader | D3#3 | warn | missing | fix-doc | add P-7 row to map_loader-boundary Extension axes |
| map_loader | D3#4 | warn | missing | fix-doc | add P-8 row to map_loader-boundary Extension axes |
| content | D1#0 | warn | STALE | fix-doc | boundary: 5 -> 6 StudioView sub-views (add BoneControllerView) |
| content | D2#0 | warn | violation | fix-code | studio.cpp:175 add // SAFETY: comment |
| content | D2#1 | warn | false-positive-candidate | fix-code | content.hpp InitParams: add @lifetime tags to filesystem&/post_process (scanner-strict; harmless, kills 0/2 noise) |
| content | D2#2 | warn | false-positive-candidate | fix-code | codec_png.cpp: move SAFETY comment adjacent to the cast token line |
| content | D3#0 | warn | missing | fix-doc | add G-1 row to content-boundary Extension axes |
| content | D3#1 | warn | missing | fix-doc | add G-3 row to content-boundary Extension axes |
| content | D3#2 | warn | missing | fix-doc | add G-4 row to content-boundary Extension axes |
| content | D3#3 | warn | missing | fix-doc | add G-5 row to content-boundary Extension axes |
| content | D3#4 | warn | missing | fix-doc | add P-7 row to content-boundary Extension axes |
| content | D3#5 | warn | missing | fix-doc | add P-8 row to content-boundary Extension axes |
| filesystem | D1#0 | warn | WRONG | fix-doc | deep-dive §8: ArchiveRegistry -> ArchiveType + k_archive_types table |
| filesystem | D2#0 | warn | STALE (documented as deferred; binding r | compliance-allow | Q-4 InitParams deferral documented with owner 6B-S8-host |
| filesystem | D3#0 | warn | missing | fix-doc | add G-1 row to filesystem-boundary Extension axes |
| filesystem | D3#1 | warn | missing | fix-doc | add G-3 row to filesystem-boundary Extension axes |
| filesystem | D3#2 | warn | missing | fix-doc | add G-4 row to filesystem-boundary Extension axes |
| filesystem | D3#3 | blocker | contradicted | fix-doc | P-7 row rewrite to current axis (pool-owned RAII; code already conforms); alignment note moves to HB-7 cross-ref |
| filesystem | D3#4 | warn | missing | fix-doc | add P-8 row to filesystem-boundary Extension axes |
| cmd_cvar | D1#0 | blocker | WRONG | fix-doc | arch cvar-registry.md: rewrite FCVAR table from cvar.hpp:27-54 |
| cmd_cvar | D1#1 | blocker | WRONG | fix-doc | arch index/command-buffer: built-ins live in context_init.cpp; exec/stuffcmds stubs; if/else not built |
| cmd_cvar | D1#2 | warn | WRONG | fix-doc | arch DI doc old_value -> default-string (align with boundary:375-381 recorded gap) |
| cmd_cvar | D1#3 | warn | WRONG | fix-doc | arch DI doc: remove phantom cmdline field |
| cmd_cvar | D2#0 | warn | STALE | fix-doc | boundary QO table: add ASCII bounds 32/126 rows |
| cmd_cvar | D3#0 | warn | missing | fix-doc | add G-2 row to cmd_cvar-boundary Extension axes |
| cmd_cvar | D3#1 | warn | missing | fix-doc | add G-3 row to cmd_cvar-boundary Extension axes |
| cmd_cvar | D3#2 | warn | missing | fix-doc | add G-4 row to cmd_cvar-boundary Extension axes |
| cmd_cvar | D3#3 | warn | missing | fix-doc | add P-5 row to cmd_cvar-boundary Extension axes |
| cmd_cvar | D3#4 | warn | missing | fix-doc | add P-7 row to cmd_cvar-boundary Extension axes |
| cmd_cvar | D3#5 | warn | missing | fix-doc | add P-8 row to cmd_cvar-boundary Extension axes |
| host | D3#0 | warn | missing | fix-doc | add G-1 row to host-boundary Extension axes |
| host | D3#1 | warn | missing | fix-doc | add G-3 row to host-boundary Extension axes |
| host | D3#2 | warn | missing | fix-doc | add G-4 row to host-boundary Extension axes |
| host | D3#3 | warn | missing | fix-doc | add P-5 row to host-boundary Extension axes |
| host | D3#4 | warn | missing | fix-doc | add P-7 row to host-boundary Extension axes |
| platform | D1#0 | warn | WRONG | fix-doc | boundary banner: 9 -> 16 compliance-allow adjudications (12 thread-assert + 4 mutable-global) |
| platform | D3#0 | warn | missing | fix-doc | add G-4 row to platform-boundary Extension axes |
| platform | D3#1 | warn | missing | fix-doc | add P-5 row to platform-boundary Extension axes |
| platform | D3#2 | warn | missing | fix-doc | add P-8 row to platform-boundary Extension axes |
| utilities | D1#0 | warn | WRONG | fix-doc | arch index cxx_std 20 -> 23 |
| utilities | D3#0 | warn | missing | fix-doc | add G-1 row to utilities-boundary Extension axes |
| utilities | D3#1 | warn | missing | fix-doc | add G-3 row to utilities-boundary Extension axes |
| utilities | D3#2 | warn | missing | fix-doc | add G-4 row to utilities-boundary Extension axes |
| core | D1#0 | blocker | STALE | fix-doc | arch README: D-1 split (log/thread_role in platform target), cycle broken one-way core->platform |
| core | D1#1 | warn | WRONG | fix-doc | arch index C++20 -> C++23 |
| core | D1#2 | warn | STALE | fix-doc | arch index: source files error.cpp+clock.cpp; add clock.hpp/error.hpp to API table |
| core | D3#0 | warn | missing | fix-doc | add G-2 row to core-boundary Extension axes |
| core | D3#1 | warn | missing | fix-doc | add G-4 row to core-boundary Extension axes |
| core | D3#2 | warn | missing | fix-doc | add G-5 row to core-boundary Extension axes |
| core | D3#3 | warn | missing | fix-doc | add P-2 row to core-boundary Extension axes |
| core | D3#4 | warn | missing | fix-doc | add P-5 row to core-boundary Extension axes |
| memory | D1#0 | warn | WRONG | fix-doc | allocation-api.md: null-pool still prefixes AllocHeader |
| memory | D1#1 | warn | WRONG | fix-doc | allocation-api.md: OOM handler IS called on overflow |
| memory | D3#0 | warn | missing | fix-doc | add G-1 row to memory-boundary Extension axes |
| memory | D3#1 | warn | missing | fix-doc | add G-4 row to memory-boundary Extension axes |
| memory | D3#2 | warn | missing | fix-doc | add P-8 row to memory-boundary Extension axes |
| abi | D1#0 | warn | WRONG | fix-doc | deep-dive: INTERFACE_VERSION 138 -> 140 (legacy eiface.h:22 confirmed) |
| abi | D1#1 | warn | WRONG | fix-doc | boundary: finish item 6 reports 8 test files (server layout-pin TUs), not 0 |
| abi | D3#0 | warn | missing | fix-doc | add P-1 row to abi-boundary Extension axes |
| abi | D3#1 | warn | missing | fix-doc | add P-5 row to abi-boundary Extension axes |
| abi | D3#2 | warn | missing | fix-doc | add P-7 row to abi-boundary Extension axes |
| abi | D3#3 | warn | missing | fix-doc | add P-8 row to abi-boundary Extension axes |
| launcher | D3#0 | warn | missing | fix-doc | add P-8 row to launcher-boundary Extension axes |
| tree | D5#0 | warn | WARNING | escalate | studio layout tripwire test: implement now vs keep Chunk-7-deferred (Q3) |
| host | HB-3 | - | tracked-work | fix-code | add assert_thread_role(Main) to RunFrame:200/RequestShutdown:243/signal_frame_abort:259 (host.hpp:115-119 contract; threading-doc Recommendation 1) |
| utilities | HB-1 | - | tracked-work | fix-code | bounded ci_compare in string.hpp + adopt at ci_less/ci_equal + filesystem ci_find_by_name + cmd_cvar sites; delete M-4/M-7/M-5 findings on adoption |
| cross | HB-2 | - | tracked-work | fix-doc | float/byte-EXACT no-touch invariant note in decisions-architecture.md |
| cross | HB-8 | - | tracked-work | fix-doc | miniz shared-target ownership note |
| cross | HB-9 | - | tracked-work | fix-doc | enforcement-free-by-role note (abi/launcher) |
| cross | HB-10 | - | tracked-work | fix-doc | status/plan drift: content row wording, src counts, core cvar-name compat note |
| cross | HB-11 | - | tracked-work | fix-doc | add host Server::RunFrame pump TODO (host.cpp:233) to the HB-11 inventory |
| cross | HB-12 | - | tracked-work | deferred | shared deterministic RNG refactor - owner: future harmonization session |
| cross | INS | - | tracked-work | fix-doc | instructions §logging platform::log -> core::log + echoes in sweep-module/retriever prompts (verify namespace first) |
| filesystem | rename | - | tracked-work | fix-doc | git mv xash3dpp-src-filesystem-modernization.md -> filesystem-modernization.md + link updates |
| arch-Q21 | template | - | tracked-work | fix-doc | decisions-architecture Q-21 text: axis enumeration G-1..G-4/P-1..P-6 -> current full set |
