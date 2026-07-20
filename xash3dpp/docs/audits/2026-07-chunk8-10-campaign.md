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
| 16 | C/S8.2 | FIELD_STRING value encoded as token index (deep-dive §4 loose line over-read) | parity gate | **DIVERGENCE (bidirectionally load-breaking)** | FIXED pre-commit: payload = null-terminated TEXT, tokens are NAMES only; deep-dive line corrected at source; cross-engine witness test added (hand-built legacy bytes deserialize) |
| 17 | C/S8.2 | All-zero fields not skipped (DataEmpty semantics missing) | parity gate | DIVERGENCE | FIXED pre-commit: has_* guards on all 5 ETABLE fields; skipped fields intern no name token |
| 18 | C/S8.2 | Block-header count hardcoded 5, not actualCount | parity gate | DIVERGENCE | FIXED pre-commit: computed post-skip count |
| 19 | C/S8.2 | entry_for pent→row scan = unevidenced surface (brief mis-cited EntryInTable) | parity gate | ADVISORY | dropped (door rules: no engine-side consumer; pent→index is the game DLL's CSave::EntityIndex) |
| 20 | C/S8.2 | S8.3 writer must populate row.pent before pfnSave | parity gate | NOTE | recorded in save-boundary (FENTTABLE_PLAYER tagging + validity screening depend on it) |
| 21 | C/S8.3 | Zero-filled FIELD_CHARACTER tails vs legacy stack garbage (incl. empty-skyName field-count consequence) | parity gate (32/33 confirmed) | DEVIATION (benign, round-trip-safe) | recorded in save-boundary deviation table; no code change — legacy output is itself non-deterministic here |
| 22 | C/S8.3 | Goldens pin small token tables, not the 4095-slot production image | parity gate | NOTE (coverage) | production byte-parity witness = S8.8 legacy-fixture tier; latent notes (POINTER/FUNCTION gSizes on x64, FIELD_EDICT low-4-byte DataEmpty) recorded in save-boundary Uncertainties |
| 23 | C/S8.5 | SV_GetSaveComment day-of-month not zero-padded ("Jan5" vs strftime's "Jan05"); test codified the wrong value | parity gate (38/41 confirmed) | DIVERGENCE | FIXED pre-commit: pad2 + both padding shapes pinned (UTC dates independently recomputed at re-verify) |
| 24 | C/S8.5 | FIELD_MODELNAME/SOUNDNAME raw 4-byte string_t copy — legacy WriteString family emits the resolved TEXT inline (STATICENTITY wire incompatibility) | parity gate | **DIVERGENCE (wire-breaking)** | FIXED pre-commit: text-family dispatch + FieldTextBinding companion table + StaticEntityEntry (entity_state_t is ABI-frozen); hand-built legacy witness added; false-premise codec comment corrected |
| 25 | C/S8.5 | age_save_list omits the GL_FreeImage .bmp thumbnail eviction (client/renderer capability) | parity gate | OBSERVATION (scoped) | deferral made explicit: doc comment + save-boundary Dependencies row (Chunk-12 caller obligation) |
| 26 | C/S8.6 | Mid-chunk 7-hazard verification pack (H1-H7) | 7-agent workflow | 4 CONFIRMED / 2 SEAM-DEFERRED (H1 orchestration + H3 edict materialization — documented S8.7 seams; RAII SaveBuffer structurally closes the ledger-#3 leak) / 1 test-gap | H7 gap closed pre-commit: test_container_restore_global_state_callback pins the pre-extraction firing position with a pointer-wired before/after snapshot |
| 27 | C/S8.7 | Watchdog HOLD: szCurrentMapName never populated on pfnRestore paths (global-entity field, landmark-only manifestation) + restore-window scoping vs legacy shared-buffer convention | abi-watchdog | **BLOCKER** + WARNING | FIXED pre-commit: ICurrentMapSink threading (per-connection = the ADJACENT level per the :1941-1965 trace) + legacy window projection (pBaseData=region base, size=location, bufferSize=capacity); fake-DLL capture test added |
| 28 | C/S8.7 | Parity A-E: skill/sky/lightstyles written-but-never-reapplied on load; aged-count 1 vs legacy 2; changelevel-flag premature clear; + 3 lessers (Host_Error semantics, paused-clear site, argc/S_USAGE) | parity gate (39/47 confirmed) | 5 DIVERGENCES + 3 lessers | ALL FIXED pre-commit; brief's E premise corrected from source (clear lives in SV_ActivateServer — rewrite was already right, the premature manual clear removed); round-trip tests now assert skill/sky/lightstyle restoration |
| 29 | C/S8.7 | ExtractedRecord span dangled into load_sav_file's transient image (S8.5 defect surfaced by wiring) | S8.7 lane | DEFECT (own-slice) | FIXED: records own their bytes; wire format unchanged |
| 30 | C/S8.7 | MILESTONE: real-hl.dll save->load round trip on c0a0 (pfnSave + pfnRestore through retail hl.dll) | extended smoke | WITNESS | PASSED 18/18, re-run green after the fix pass — the Chunk 8 deliverable achieved |
| 31 | E/S10 | Input lane worktree init hit the wrong-base failure mode (stale master) | lane agent | NOTE | self-corrected (fast-forward to the S9.0 base; merge-base verified) — the recorded orchestration hazard held |
| 32 | E/S10 | Census corrections: keynames = 101 rows (spec ~130), cvars = 66 (spec ~40) | lane + parity gate (both re-counted) | NOTE | source trusted over spec draft; spec figures were estimates |
| 33 | E/S10 | Reviewer: using-namespace in a private header (convention: no debate) + 7 nodiscard discards | reviewer + merge build | WARNING | fixed pre-commit: targeted using-DECLARATIONS (14 names); (void) casts on validated discards |
| 34 | E/S10 | Parity: 6 substantive divergences (ESC-unbind + sibling console messages dropped; unbound-warning NULL-vs-empty; FCVAR_CHANGED reparse flag eaten by axis events; want_visible_cursor missing 2 of 3 terms; touch.cfg header/stroke/highlight emission gaps vs the documented Compat contract; stale finger trackers on non-game touches) + 3 minors + makehelp unregistered + 2 missing chunk12 tags | parity gate (87/96 confirmed) | DIVERGENCES | ALL fixed pre-commit with per-divergence regression pins (incl. the D3 axis-event flag-swallow witness); minor int-truncation reproduced; makehelp registered with a chunk12 stub |
| 35 | D/S9.3 | Pitch computed in double throughout — legacy rounds basePitch*0.01 to FLOAT at the VOX_ModifyPitch parameter and multiplies float*float (every pitched sound de-syncs the resample accumulator; masked by basePitch=100 tests) | parity gate (43/44 confirmed; kernels verified by independent macro expansion + vector re-derivation) | DIVERGENCE | FIXED pre-commit (orchestrator): compute_channel_pitch reproduces the exact double-multiply -> float-param-round -> float*float chain; pinned by a test asserting the float value AND rejecting the double value |
| 36 | D/S9.4 | VOX: 45/47 confirmed (all 4 legacy embedded tests ported verbatim; stereo trim stride + FreeWord zeroing quirks pinned); 2 hardening deviations were in-code-only | parity gate | 2 DEVIATIONS (1 legal-input: the trailing-whitespace degenerate entry; 1 UB-only: negative-overflow handle) | sanctioned in the sound-boundary deviations section (document-not-reproduce adjudication) |
| 37 | D/S9.5 | DSP: 60/61 confirmed (both 30-row preset tables, four passes incl. the rgsxlp FIFO self-overwrite, SND-OQ-6 sentinel row proven); 1 divergence — `dsp_coeff_table` stored as `int`, but legacy coerces the cvar TWO ways: `switch((int)value)` for the table pointer (s_dsp.c:787) vs exact-float `== 1.0f` for the reverb gain (s_dsp.c:703), so fractional 1.5 gets alpha table + release gain | parity gate | DIVERGENCE | FIXED pre-commit (orchestrator): field stored as float, both coercions kept distinct at their legacy sites; fractional-1.5 asymmetry pinned on BOTH sides (gain in the reverb test, table selection in the mono-delay test) |
| 38 | D/S9.6 | Entry surface: 46/57 confirmed (constants byte-for-byte, all 25 cvars + 13 commands name/default/flag-exact incl. the play2-never-removed leak, S_StartSound operation order + every field write, pick/alter steal quirks, pan law float-exact). Findings: F-1 sentence steal-order DIVERGENCE (time_left skipped lookahead resolution, under-reporting vs legacy's forced S_LoadSound s_main.c:311-313 → wrong eviction in saturated scenes); F-2 sqrtf-vs-double-sqrt distance MINOR; F-3 per-term-truncating accumulation MINOR; F-11 dsp_profile restricted command missing; F-4 always-lazy register + hardening/scope items | parity gate (60/61-style line audit) + reviewer gate (SHIP, 1 bounds-assert warning) | 1 DIVERGENCE + 2 MINOR + 1 gap | F-1/F-2/F-3/F-11 ALL FIXED pre-commit (orchestrator): time_left force-resolves with load_word bookkeeping + legacy float compound-assign shape (both pinned by new tests incl. the -1+0.7f→0 discriminator), spatialize distance through double-sqrt (VectorLength shape), dsp_profile registered FCMD_PRIVILEGED wired to RoomDsp::profile; F-4/#5 sanctioned in sound-boundary entry-surface deviations; F-5/F-7/F-9/F-10 recorded deferred-with-owner; reviewer bounds assert added |
| 39 | D/S9.7a | Core queue family (`MpscQueue<T,Cap,Reserve>` Vyukov ticket queue + `SpscRing<T,Cap>`): adversarial concurrency review re-derived every atomic access on a weak (ARM) model and REFUTED-the-code on all four attack vectors it was asked to break (producer publish, slot-reuse free/overwrite, relaxed dequeue gate, SPSC wrap-vs-publish). Findings were edge-hardening: F1 logical over-admission at the 32-bit `size_t` counter wrap (physical overwrite never possible), F2 signed-overflow UB in the Vyukov generation diff, F3 occupancy accessors can underflow to a wrapped-huge value under concurrent sampling, F4 a false "2^64 unreachable" comment on 32-bit, F8 missing `Threads::Threads` (breaks the GCC/Clang target, invisible on MSVC) | adversarial reviewer (opus, refute posture) | SHIP + 4 MINOR/NIT + 1 portability | ALL FIXED pre-commit (orchestrator): counters widened to explicit `uint64_t` (kills F1/F4 on every target) with an `is_always_lock_free` static_assert; diff subtracts unsigned then casts once (F2); occupancy samples the TRAILING counter first and clamps (F3); `find_package(Threads)` + `Threads::Threads` on both core tests (F8). F5 (counter-wrap regime untestable through the public API) and F6 (x86-TSO cannot discriminate a dropped release/acquire — green x86 is NOT ordering proof) recorded as in-file caveats; TSan/ARM side-lane remains the deferred item |
| 40 | D/S9.7b | Sound thread topology (first multi-threaded subsystem in the tree): a **four-dimension gate** — parity-of-moved-semantics, adversarial concurrency, lifecycle/teardown, decision conformance — with an independent refute pass on every finding (33 agents). 85 claims checked, 52 confirmed correct, 36 findings raised, **11 refuted** by verification (2 reported as BLOCKER/DIVERGENCE turned out NOT-A-DEFECT). Confirmed defects: **cross-thread use-after-free** — `SfxRegistry::release()` destroyed a shared cache entry from T_AudioDecoder during VOX word retirement while other live channels and in-flight `AudioCommand::source` pointers still borrowed it (converged on independently by parity F-1/F-2 AND concurrency CONC-1; the epoch fence orders commands, it does not protect buffers); `dsp_profile` mutating decoder-owned RoomDsp from T_Main (a race the ORCHESTRATOR introduced in the S9.6 gate fix); `flush()` failing OPEN on two paths despite being the quiesce licence; MouthSlots (entnum, amplitude) pair tearing; FrameUpdate droppable; `resolve_origin` unable to express legacy's IN/OUT contract (`cl_frame.c:1387-1392`) so a sound with a valid server `pos` for an unparsed entity went SILENT instead of playing; unlocked registry read from `soundlist`; no quiesce against an external OS callback at ring teardown; priority inversion holding the registry mutex across file I/O | 4-dimension gate + refute pass (33 agents) | 1 UAF + 1 race + 7 MAJOR/MINOR | ALL FIXED pre-commit: decoded audio RETAINED for the registry lifetime (address stability made unconditional; adjudicated deviation recorded), dsp_profile refused while running, `flush()` now `[[nodiscard]] bool`, mouth pair packed into one atomic, FrameUpdate on the reserved lane, resolve_origin made true IN/OUT + local-player branch, registry read locked, in-callback quiesce counter, decode moved outside the lock (double-checked install). 8 new regression tests incl. the end-to-end F-8 witness (empty snapshot before the fix, one audible channel after). **SND-OQ-3 AMENDED** (bounded-spin-then-counted-drop ratified over the register's "blocks/asserted in debug" — an assert on producer saturation turns a stalled decoder into an abort and makes the guarantee untestable); **SND-OQ-2 RESOLVED**. Master volume partially wired (fade curve + gate producer marked chunk12) |
| 41 | D/S9.8 | Sink-device integration witness — the Chunk 9 closer: console-scripted 25-frame sequence (play/playvol/play2/speak/stopsound + room_type/waterlevel/s_lerping cvars) through the full pipeline into SinkDevice; PCM byte-identical across 12 in-process repeats x 10 standalone runs x BOTH arches (FNV 0xDF1088E7D0D531CF, authoritative check is memcmp vs committed bytes so a digest collision cannot mask divergence; determinism argued source-by-source: no wall clock in the measured path, total happens-before via role workers, no map iteration or pointer values in output, /fp:precise + integer-generated sources for cross-arch float identity). Resolves gate finding CONC-6: internal_pump now DERIVED from IAudioDevice::drives_own_callback() instead of hardcoded. New Q-4 seams: SoundInitParams::audio_loader (injected decode seam), external_decoder + Sound::decoder_step() (commanded pump). The plan's post-chunk fan-out (kernel-parity matrix + channel-allocation policy agent) is adjudicated DISCHARGED-BY-EXCEEDING: S9.3's parity gate verified the kernel matrix by independent macro expansion, S9.6's line-audit covered the allocation policy, and the S9.7b four-dimension gate re-verified both post-move | witness + focused reviewer on the 3 non-test seams | — | committed with Chunk 9 closure |
| 42 | F/close-out | **Campaign close-out mini-audit** (9 auditors: doc accuracy x3, Q-21 posture x2 via extension-door-auditor, threading currency x2, plus the abi-watchdog and dependency-graph exit gates). 145 claims checked, 27 findings. **Both hard exit gates GREEN**: abi-watchdog CLEAR across the vendored `sound_api.hpp` pins, input's keydefs static_assert approach, save's TYPEDESCRIPTION usage, and the fake-DLL probes (14 checked, 0 findings); dependency-graph confirms `xash3dpp_sound`/`xash3dpp_input` are LEAF static libs with no host/server/launcher edge and core gained no downward edges (6 checked, 0 findings). Findings were doc-currency and annotation debt, not defects: **the campaign's own "Chunk 10 doc closure 2026-07-20" claim was FALSE** — `input-boundary.md` still read "pre-implementation draft, 0 TUs" and had not been touched since the recon commit; `input.hpp`'s header claimed "every mutating entry point asserts ThreadRole::Main" against 7 assert sites for ~29 entries; sound carried 13 unresolved thread-assert candidate-warnings; `sound-boundary.md`'s own OQ table still showed SND-OQ-4/OQ-5 open after the campaign decided them (drift that had propagated into `codec.hpp`); `core-boundary.md` never mentioned the queue family it now owns and still called its synchronisation footprint exhaustive; the design brief documented a templated `spawn_thread` signature that was never built that way; `thread_role.hpp` cited pre-renumbering chunk numbers; and three cvar/table censuses undercounted legacy (touch 14→22, joy/gyro ~26→32, keynames ~130→101) | close-out audit (9 agents) | 13 MAJOR + 13 MINOR + 1 NOTE, 0 BLOCKER | ALL FIXED at close-out: input-boundary reconciled with an as-built banner and corrected censuses, input assert coverage brought up to its claim rather than the claim narrowed, sound's 13 candidates resolved per-site (real Main asserts on the device lifecycle setters; conditional-role compliance-allow where asserting Main would be WRONG because the decoder owns the object), OQ rows and stale code comments corrected, core-boundary given a queue-family section, brief signature corrected, chunk numbers fixed. The false doc-closure claim is corrected in the plan heading rather than quietly overwritten |

*(Ledger continues at each phase; C/D/E gate-agent findings append here.)*

## Campaign close — final report (2026-07-20)

**Verdict: COMPLETE.** Chunks 8 (save/restore), 9 (sound) and 10 (input) all
shipped and are flipped DONE in `implementation-plan.md`; `status --check`
drift is clean.

### Exit gates

| Gate | Result |
|---|---|
| Build + tests, both arches | 124/124 x64, 124/124 x86, 0 errors, 0 new warnings |
| `status --check` drift | clean (input + sound rows flipped Complete) |
| `q21_scan` | 16/16 boundaries clean, 0 missing axes |
| `workflow_sync` (stage 2) | 0 findings |
| `markdown_lint` (docs scope) | 0 issues |
| abi-watchdog re-baseline | **CLEAR** — vendored `sound_api.hpp` pins, input keydefs static_asserts, save TYPEDESCRIPTION usage, fake-DLL probes; no frozen-header conflict |
| dependency-graph leaf rule | **CLEAR** — `xash3dpp_sound`/`xash3dpp_input` are leaf static libs, no host/server/launcher edge; `core` gained no downward edges |
| `compliance_scan` thread-assert | sound 0 candidates / 9 documented allows; input 0 candidates / 9 documented allows |

### Census delta (campaign start → close)

| Metric | Start | Close |
|---|---|---|
| src TUs | 132 | 167 |
| tests (per arch) | 95 | 124 |
| `assert_thread_role` sites | — | 248 (save 38, sound 27, input 37 new this campaign) |
| Subsystems with 0 TUs closed | save, sound, input | all three now Complete |

### Phase log

| Phase | Output |
|---|---|
| A — recon + boundary specs | 3 boundary specs + 2 deep-dives, ~14 read-only recon agents |
| B — decisions | Q-23 (backend stop-line), Q-24 (queue-family home), HB-4 design brief, the SND-OQ set, B5 command-context overload |
| C — Chunk 8 save | S8.1-S8.8 + platform spike; real retail `hl.dll` save→load witness |
| D — Chunk 9 sound | S9.0-S9.8; first threads in the tree; byte-identical cross-arch PCM witness |
| E — Chunk 10 input | S10.1-S10.6 in a worktree lane, merged and double-gated |
| F — close-out | 9-auditor mini-audit + exit gates + this report |

### What the gates caught (the case for the discipline)

Across the campaign the verification gates caught **1 cross-thread
use-after-free, 1 data race, 1 ABI blocker, and 30+ behavioural divergences
before any of it was committed** — including several that no test in the
suite would have failed on. The highest-value catches:

- **S9.7b — cross-thread use-after-free** (converged on independently by two
  gate dimensions): `SfxRegistry::release()` destroyed a decoded buffer from
  the decoder thread while other live channels and queued commands still
  borrowed it. The slice's own epoch-fence safety argument had a real gap —
  the fence orders commands, it does not protect buffers.
- **S9.7b — `dsp_profile` data race**: a race the ORCHESTRATOR introduced in
  the S9.6 gate fix, caught by the next gate. Gates catch their author too.
- **S9.7b — silent-sound divergence**: `resolve_origin` could not express
  legacy's IN/OUT contract, so a sound fired with a valid server position for
  an unparsed entity was inaudible where legacy plays it.
- **S8.2 / S8.5 — wire-breaking save-format divergences** (FIELD_STRING token
  index; FIELD_MODELNAME raw copy) — both would have produced saves the
  legacy engine could not read.
- **S9.3 — float-chain divergence**: pitch computed in double throughout,
  where legacy rounds to float at a parameter boundary; every pitched sound
  would have de-synced its resample accumulator, masked by the default
  `basePitch == 100`.
- **Close-out — a false claim by the orchestrator**: the campaign asserted
  "Chunk 10 doc closure 2026-07-20" while `input-boundary.md` still read
  "pre-implementation draft, 0 TUs". Corrected in place, not overwritten.

Equally important, the **adversarial verify passes refuted 11 of 36 findings**
on the highest-risk slice (two of them reported as blocker/divergence but not
defects at all) and refuted or downgraded several more elsewhere — acting on
raw finder output would have meant substantial churn on correct code.

### Decisions amended rather than obeyed

**SND-OQ-3** was ratified as "START overflow blocks the producer, asserted in
debug"; the implementation spins bounded and then drops with a counter. The
implementation is right — an assert on producer saturation turns a merely
stalled decoder into a process abort and makes the guarantee untestable — so
the register, the design brief, and the boundary spec were amended to say what
the code does and why the original wording was withdrawn. Recorded here
because "the decision was wrong" is a legitimate gate outcome and should not
be laundered into silent drift.

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
- clang/TSan side-lane over `core::MpscQueue`/`SpscRing`: **DECLINED for
  now, with reason** (S9.7a) — the toolchain here is MSVC-only, and the
  adversarial review established that x86-TSO cannot discriminate a
  dropped release/acquire in the first place, so a green local run would
  be false assurance either way. The ordering argument rests on the
  review's independent weak-model re-derivation (all four vectors
  REFUTED-the-code) plus the in-header justification table. Owner: run
  under clang/TSan or on ARM hardware when either becomes available.
- Reserved sfx slot 0 (`*default`): legacy reserves `s_knownSfx[0]` as an
  always-valid silence sfx so real handles start at 1 (s_load.c:361-366);
  the S9.6 registry starts real handles at 0. Self-consistent today
  (handles never cross the wire), but the precache/networking wiring must
  either reserve slot 0 or re-audit every "handle 0 == default" assumption.
  Owner: Chunk 12 client wiring (S9.6 audit F-5).
- Channel `name[16]` truncation for serialization: see the sound-boundary
  entry-surface deviations — the Chunk-8/12 save wiring owns the decision
  (S9.6 audit F-7).
- Ambient channels (`S_InitAmbientChannels`/`S_UpdateAmbientSounds`) +
  `S_ClearBuffer` in stop-all: need world-leaf/client state; the ambient
  range `[0,4)` is carved and excluded from all S9.6 allocation scans
  (audit F-9 verified no ported path depends on it). Owner: Chunk 12.
- `soundlist`/`s_info`/`music`/`s_fade` full command bodies: registered
  with correct names/flags but summary bodies pending s_stream (SND-OQ-4)
  and the fade curve. Owner: Chunk 12/13 (S9.6 audit F-10).
- `warning C4530` (chrono try/catch under `/EHs-c-`) on FRESH rebuilds of
  any TU that includes `xash3dpp/filesystem/filesystem.hpp` (its public
  `file_time()` returns `std::filesystem::file_time_type`, dragging
  `<filesystem>` -> `<chrono>` into no-exceptions TUs; sound.cpp is the
  first sound-side instance, and legacy-abi TUs show the same class).
  Benign under the no-exceptions posture (throw => terminate is the
  intended contract), invisible to incremental gate builds. Real fix is an
  interface change (POD file-time return) across the 7 filesystem
  backends — its own gated slice. Owner: post-campaign hygiene follow-up.
