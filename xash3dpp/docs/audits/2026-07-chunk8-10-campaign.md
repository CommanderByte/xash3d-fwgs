# Chunks 8/9/10 campaign — report and adjudication ledger

> **Status**: LIVE (opened 2026-07-19 at Phase B; finalized at Phase F).
> Campaign plan: session plan file (user-approved after a 5-lens contrarian
> review); precedent: `2026-07-consolidation-audit.md`. Scope: Chunk 8
> save/restore, Chunk 9 sound, Chunk 10 input. Ordering: specs-all → 8 →
> 9 ∥ 10 (input lane worktree-isolated).

## Phase log

| Phase | Status | Commits | Notes |
|---|---|---|---|
| A — recon + boundary specs | ✅ 2026-07-19 | `ee9d8107..04fe7ee4` (4) | 14-agent tiered fan-out (622 claims, 0 failures) → 3 sonnet drafters → orchestrator adjudication; deep-dive merges verified claim-complete (2 haiku critics: 0 dropped, 0 altered citations); q21_scan 16/16, markdown_lint 0, workflow_sync 0/0 |
| B — decisions | ✅ this commit | — | Q-23/Q-24 register entries; SYNC-CORE twins Q-count; §3a rows for SAV/SND/INP OQ sets; server OQ-4 closed; OQ-6 restore-sufficiency note; HB-4 brief delivered; threading-model §3.4/§5.1/§5.2 + assert-semantics amendments; extension-goals §4 chunk-10 row + chunk-7 shortfall note; B5 decision below |
| C — chunk 8 save | pending | | S8.1–S8.8 per plan |
| D — chunk 9 sound | pending | | S9.0–S9.8 per plan |
| E — chunk 10 input | pending | | S10.1–S10.6, worktree lane |
| F — close | pending | | mini-audit + exit gates |

## Decision record — B5 (command context)

**Problem**: `cmd_cvar::CommandFn` is `void (*)()` (layout-identical to
legacy `xcommand_t`) with no user-data slot; built-ins reach state via a
`tls_ctx` pointer to the `CmdCvarContext` only. The campaign registers ~50
new handlers (save/load family, sound play/stop family, input bind family)
that need their subsystem's context. The only sanctioned capture-less
carve-out is `g_bridge` (ABI-forced). P-3 requires the exception class to
shrink, not grow.

**Decision (a)**: cmd_cvar gains a **non-breaking context overload** —
`CommandCtxFn = void (*)(void *user)` and
`cmd_add(name, CommandCtxFn fn, void *user, uint32_t flags, const char
*desc)`. The registry stores the optional `user` pointer per entry;
dispatch calls the ctx variant when present. Existing capture-less
registrations are untouched; the legacy ABI-facing registration paths
(`pfnAddServerCommand` etc.) keep the legacy shape. Lands as its own small
`cmd_cvar:` slice before S8.7 (the first consumer). Rejected alternative
(b): per-subsystem bridge statics with compliance-allow rows — grows the
P-3 exception class for no gain.

## Scope fences (named)

- Voice chat (1,207 lines + Opus + capture device): OUT — own future slice.
- mp3/libmpg + ogg/opus decoders: OUT of initial Chunk 9 (wav only);
  `IAudioCodec` registry makes them additive; mp3/libmpg Q-11 ambiguity
  recorded in sound-boundary Satellite components (adjudication at its
  slice, recommendation: separate target).
- Music streaming (`s_stream.c`): OUT per SND-OQ-4; `IAudioStream` vend
  reserved.
- Touch/OSK renderer drawing: OUT (Chunk 12/13; fence list in
  deep-dive-input.md); event models IN.
- Save client side (saveshot, `CL_ParseRestore`, menu comment bridge): OUT;
  `SV_GetSaveComment` itself IN.
- Listen-server `.HL2` capability inputs: injected interfaces, null on
  dedicated, null block still legacy-loadable (server OQ-4 resolution).
- SoundAPI override: layouts pinned + main-thread-bound constraint
  recorded; plumbing deferred.
- SDL anywhere: OUT (Q-23); backend placement decided at Chunk 12/13 via
  Q-11.

## Adjudication ledger

| # | Phase | Finding | Source | Severity | Disposition |
|---|---|---|---|---|---|
| 1 | A | Deep-dive save line-count 2492 vs 2493 (trailing blank) | V8.1 | NOTE | cosmetic, no action |
| 2 | A | `IsValidSave` precondition chain richer than deep-dive §3 | V8.1 | NOTE | fuller list adopted into save-boundary Quirks |
| 3 | A | `SV_SpawnServer`-failure `pSaveData` leak (legacy `// ???`) | V8.1 | WARNING | **deviation (bug-fix class)** — rewrite closes the leak; recorded in save-boundary |
| 4 | A | Container extract is extension-blind BUT `*.HL?` cleanup glob orphans unknown extensions | V8.2 | LOAD-BEARING | drives SAV-OQ-1's `.HLX` reserved namespace (matches cleanup glob, invisible to exact-name readers) |
| 5 | A | `SV_GetSaveComment` NULL-deref on corrupted token | V8.2 | WARNING | **deviation (bug-fix class)** — reject gracefully via `SaveError` |
| 6 | A | SNDDMA seam is 5+4 fns (brief said 3 VoiceCapture); `s_stub` omits `Activate` | R9.6 | NOTE | counts corrected in sound-boundary; stub asymmetry folded into SND-OQ-2 |
| 7 | A | `sound_interface_t` is 8 slots (drafter wrote 7) | R9.6/adjudication | NOTE | corrected at finalize |
| 8 | A | DSP `idsp_room == 29` off-by-one vs 28-max preset table | R9.4 | WARNING | SND-OQ-6 (reproduce-vs-clamp), open |
| 9 | A | Legacy sound commands unrestricted except `music`/`dsp_profile` | R9.1 | NOTE | parity beats the plan's blanket-privilege assumption; registration mirrors legacy flags |
| 10 | A | Only 4 `Platform_*` input fns dereference `hWnd` | R10.2 | LOAD-BEARING | `IWindowControls` = exactly that set + cursor composite; device fns stay on `IEventSource` (interface segregation) |
| 11 | A | One dinput stub, not two (`pfnSetMouseEnable` only) | R10.5 | NOTE | corrected in input-boundary |
| 12 | A | Legacy-path engine move-merge never reads mouse deltas (`includeMouse=false`) | R10.5 | LOAD-BEARING | recorded in input-boundary Interface + Quirk 12 |
| 13 | A | `joy_axis_binding` doc-string trigger-label inversion | R10.3 | NOTE | recorded as legacy doc bug (Quirk 8), not behaviour |
| 14 | A | `Platform_Input`/`Platform_SetTimer` scoping ambiguity | R10.2 | NOTE | adjudicated: dedicated console text / platform time surface — both out of input |
| 15 | B | OQ-6 sufficiency for restore | campaign | NOTE | confirmed: load-time string_t offsets transient, never persisted — §3a note |

*(Ledger continues at each phase; C/D/E gate-agent findings append here.)*

## Deferred with owner

- `diagnostics_dump` stats aggregator — overdue by the debug-stats-design
  §6.4 trigger (≥3 stats structs; the tree has 10+, this campaign adds 3).
  Owner: post-campaign follow-up; the three new stats structs must be
  aggregator-compatible (plain `stats()` accessors) so the wiring is
  mechanical.
- Real-retail legacy save fixtures beyond the env-gated tier
  (`XASH3DPP_LEGACY_SAVE_DIR`): regenerating fixtures by scripting the
  legacy engine build is recorded as deferred unless tier-2 proves
  insufficient (testing strategy, plan).
- clang/TSan side-lane over `core::MpscQueue`/`SpscRing`: developer-run,
  recorded taken-or-declined at S9.7a.
