# PM Determinism: Float vs. Fixed-Point — Decision Brief

*Drafted 2026-07-04. Status: awaiting sign-off. Gates Chunk 5 (map_loader)
per implementation-plan.md ("must be decided before this chunk ships").*

## The question, corrected

The implementation plan framed this as "float vs. fixed-point for the
pm_shared layer". Reconnaissance shows the premise needs correcting first:

**The engine does not own the player-movement math.** `pm_shared/` in this
repo holds only headers (`pm_defs.h`, `pm_info.h`); there is no `pm_move.c`
in the engine tree. `PM_Move` is compiled by *mod authors* into both their
game DLL and client DLL (HLSDK model) and invoked through frozen
function-pointer seams:

- server: `svgame.dllFuncs.pfnPM_Move(svgame.pmove, true)` — `engine/server/sv_pmove.c:969`
- client: `clgame.dllFuncs.pfnPlayerMove(clgame.pmove, false)` — `engine/client/dll_int/cl_pmove.c:942`
- ABI: `engine/eiface.h:461`; `playermove_t` (`pm_shared/pm_defs.h:79-215`) is all `float`/`vec3_t`, FROZEN.

What the engine *does* own — and what the rewrite must actually decide the
math representation for — is:

1. the trace/contents helpers plugged into `playermove_t`
   (`PM_PlayerTraceExt`, `PM_TestPlayerPosition`, `PM_PointContents`, … in
   `engine/common/pm_trace.c` / `pm_surface.c`), i.e. exactly the Chunk 5
   map_loader spatial-query API, and
2. engine-side entity physics (`sv_phys.c`, Chunk 6+).

## Facts gathered

- **Both DLL copies are float.** Mod binaries ship with float PM math the
  engine cannot change. Any engine-side representation change cannot make
  the *system* deterministic; it can only make the engine's half diverge
  from the mod's half.
- **Divergence is already managed, not prevented.** Client prediction
  reconciles against authoritative server state every update:
  `CL_CheckPredictionError` (`cl_pmove.c:190`) measures
  predicted-vs-actual origin, corrects smoothly, and treats >64 units as
  teleport. `SV_RunCmd` sub-steps long commands (`sv_pmove.c:925-934`) and
  compares with `ON_EPSILON` tolerances (`sv_pmove.c:868`).
- **The wire re-quantises PM state at every boundary.** Origins travel as
  16-bit 1/8-unit fixed coords (`MSG_WriteCoord`, `net_buffer.c:449`),
  angles as bit-angles, and delta fields apply integer multipliers. Small
  FP drift is absorbed by quantisation before it can accumulate across the
  network.
- **Legacy builds use conservative FP by default.** The Waf release profile
  is `/O2` (MSVC) / `-O3` (GCC/Clang) with **no** `/fp:fast`,
  `-ffast-math`, or `-ffp-contract` anywhere
  (`scripts/waifulib/compiler_optimizations.py`); only the opt-in
  `fast`/`fastnative` build types add unsafe math. So the GoldSrc-parity
  baseline is plain IEEE float with compiler defaults.
- **The shared float primitives are platform-stable by construction** where
  it matters most: `Q_rsqrt` is the integer bit-twiddled fast inverse sqrt
  (`public/xash3d_mathlib.c:144`), identical on every IEEE-754 platform.
- **xash3dpp CMake currently pins nothing** beyond `/EHs-c- /GR-`; FMA
  contraction (`-ffp-contract=fast` is the GCC default at -O2+) could make
  x86 and ARM builds of the *same* engine source disagree.

## Options

**A. Float + pinned-conservative FP flags (recommended).**
Keep all engine-side PM/trace/physics math in `float` (matching `vec_t` and
the frozen ABI), and pin the toolchain so identical source produces
identical results across compilers/architectures as far as practical:
MSVC `/fp:precise` (explicit, already the default), GCC/Clang
`-ffp-contract=off` plus never enabling fast-math for the map_loader /
world / physics / server targets. Same-platform determinism becomes exact;
cross-platform residual drift is absorbed by the existing
prediction-reconciliation + wire quantisation, exactly as GoldSrc has
operated for 25 years.
*Cost:* a few CMake lines; disallowing FMA in hot trace loops is a
negligible perf cost at these workloads. *Risk:* none to compatibility —
this is what legacy effectively does.

**B. Fixed-point engine math.**
Deterministic in isolation, but: it cannot change the float math inside
every shipped mod DLL, so client/server *system* determinism is not
achieved; the frozen `playermove_t`/trace ABI is float, forcing conversions
at every boundary (reintroducing rounding, now asymmetric with the DLL's
own float math — strictly worse parity than Option A); and it requires
rewriting the entire trace/physics layer plus bespoke sqrt/trig. High cost,
negative compatibility value for the GoldSrc target.

**C. Doubles internally.**
Improves precision but *guarantees* divergence from the mods' float
computations and the float ABI, and doubles the bandwidth of quantisation
error asymmetries. Rejected.

## Recommendation

**Option A.** Concretely:

1. Chunk 5 (map_loader) implements BSP traces/PVS in `float`, semantics
   matched to `pm_trace.c` (same epsilons, same clip logic).
2. Add to `xash3dpp/CMakeLists.txt`: `/fp:precise` for MSVC (explicit
   documentation of intent) and `-ffp-contract=off` for GCC/Clang, global;
   forbid fast-math build types for engine targets.
3. Determinism regression test at Chunk 5: golden trace fixtures — fixed
   input rays/hulls against a known BSP → byte-exact expected outputs on
   the same platform, epsilon-bounded across platforms.
4. Revisit only if a future non-GoldSrc protocol wants lockstep
   cross-platform simulation; that would be a new opt-in quantised path,
   not a change to the compat engine (record as a deferred note, not a
   blocker).

## Proposed decision-table row (for decisions-architecture.md on sign-off)

| ID | Decision | Verdict |
|----|----------|---------|
| Q-18 PM_FP_MODEL | Engine-side PM/trace/physics math representation | `float` matching the frozen `playermove_t` ABI, with pinned conservative FP flags (`/fp:precise`, `-ffp-contract=off`, fast-math forbidden). Fixed-point rejected: pm_shared executes inside mod DLLs as float — engine-side fixed-point worsens parity instead of fixing it. Determinism is managed by prediction reconciliation + wire quantisation (legacy model). Golden trace fixtures gate map_loader. |
