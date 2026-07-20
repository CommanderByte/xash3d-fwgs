---
name: "Analyse modernization pack — read-only audit slice"
description: "Read-only per-subsystem modernization analysis slice for multi-agent audit campaigns. Returns a structured facts inventory plus findings along one axis (currency / shape / seam) WITHOUT writing the subsystem's modernization report. Use this when fanning many agents across the tree; use /analyse-modernization instead when one agent owns one subsystem's report end to end."
argument-hint: "subsystem [axis], e.g. 'sound A2' — axis is one of A1|A2|A3|all (default all)"
agent: agent
tools: [read, search, execute, cpp-lsp/*, xash-tools/*]
model: claude-sonnet-4-6
---

# Modernization Pack: `$ARGUMENTS`

A **read-only slice** of a modernization analysis, shaped for parallel
fan-out across the tree.

**Analysis only — do not modify any file, and in particular do NOT write
`xash3dpp/docs/modernization-opportunities/<sub>-modernization.md`.** That is
the difference between this prompt and `/analyse-modernization`: when many
agents run at once, each writing that file would clobber the others and
renumber IDs that other reports cite in prose. Report findings; the campaign
orchestrator merges and assigns IDs.

---

## Why the ID rule matters

Existing findings are numbered per report, per tier (`H-1`, `M-4`, `L-2`) and
are **cross-cited across reports** (`launcher-modernization.md` refers to
"utilities (M-4)"). Renumbering silently breaks those references, including
the ones the harmonization backlog cites.

- **Never renumber an existing finding.**
- New findings continue that report's tier sequence from max+1 — so report
  the current max `H`/`M`/`L` number you observed.
- Number your own output with a local counter only.

---

## Method

Follow `.github/prompts/analyse-modernization.prompt.md` for the scope rules
and the 2-A..2-L scan categories — **read it first** — with these
differences:

1. Its Step 4 (write the report) is **replaced** by returning structured
   output.
2. You additionally return a **facts inventory** block, separate from
   findings: registries and their shapes, interfaces with production-vs-test
   implementation counts, publish/snapshot mechanisms, stats structs and
   whether each is atomic, the error idiom, RNG uses, allocation entry
   points, thread-assert sites and waivers with their rationales, file-scope
   mutable statics and whether each is documented, and every
   `chunk11..chunk14` marker with what the future chunk actually owes.
   *Inventory is not a finding* — do not convert a census into findings
   unless something is wrong.
3. You classify one axis (or all three):
   - **A1 currency** — re-check every existing `H-n`/`M-n`/`L-n` against
     today's code: still-valid / done / obsolete / wrong-as-written.
   - **A2 shape** — interfaces, registries and dispatch, dead helpers (2-J),
     lifecycle (Q-22/P-7), P-3/P-5 signature shape, C-by-choice vs C-by-inertia.
   - **A3 seam** — exports and dependencies, owned state, **Main-pinning by
     design vs incidentally**, thread-assert waiver quality, stats atomicity,
     snapshot shapes, fence classification, future-chunk obligations.

Ground the mechanical halves first:

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\compliance_scan.py $ARGUMENTS --checks all --json
& .venv\Scripts\python.exe xash3dpp\tools\stub_scan.py $ARGUMENTS --json
& .venv\Scripts\python.exe xash3dpp\tools\census.py --json
```

*(MCP: xash-tools `compliance_scan` / `stub_scan` with `subsystem="$ARGUMENTS"`,
and `census` — same data.)* Read these before judging: `compliance_scan`'s
globals/statics findings are the P-3 fact base, and `census` carries the
`assert_thread_role` and `compliance-allow` tallies that A3 must reconcile
against the code rather than re-derive.

---

## The two fences — apply BEFORE writing a finding

- **Frozen ABI**: `engine/eiface.h`, `engine/edict.h`, `engine/cdll_int.h`,
  `engine/cdll_exp.h`, `pm_shared/**`, shared SDK structs in `common/**`, and
  everything vendored under `xash3dpp/include/xash3dpp/abi/`.
- **Byte-exact kernels (HB-2, `decisions-architecture.md:946-954`) —
  KERNEL-scoped, not subsystem-scoped**: map_loader Q-18 trace/PVS/CRC;
  content studio bone math; networking wire bit-codec, delta field widths,
  LZSS, OOB packet magic; server rotated-brush ULP at `clip.cpp:211`;
  utilities double-precision studio math. Inside these, byte-exact parity
  beats every other rule — FMA, reassociation and `std::ranges` rewrites
  included.

Do not over-fence: most of `server` is not the fenced kernel. Over-fencing
kills legitimate work as surely as under-fencing wastes an implementation
thread.

---

## Guardrails

- The legacy C engine at the **repository root is reference-only**. Read it
  for behaviour; never propose changing it.
- No exceptions (`/EHs-c-`), no RTTI (`/GR-`): a proposal needing `throw`,
  `try`, `dynamic_cast` or `typeid` is invalid here. Both x64 and x86 ship.
- **Anything you propose BUILDING needs a named day-one consumer.** Per
  `extension-goals.md` §6 a door rule constrains *shape*; it is not an
  instruction to build speculative infrastructure. With no consumer that
  survives inspection, the finding is recordable only as a shape constraint,
  never as work.
- Every finding carries at least one `file:line`. No evidence, no finding.
- `blast_radius` is grepped, not estimated.
- Re-read the axis set from `extension-goals.md` §2/§3 at analysis time;
  never use a cached `G-*`/`P-*` list.
- Prefer deletions. An audit that only ever adds structure has failed — and
  this tree already carries 13 interfaces with zero production
  implementations.
