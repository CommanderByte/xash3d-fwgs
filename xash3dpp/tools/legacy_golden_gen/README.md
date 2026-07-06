# legacy_golden_gen — verbatim-legacy studio-math golden generator (Q-18)

Dev-only tool. Emits **bit-exact** expected values for the xash3dpp studio
bone-solver by compiling and running the **frozen legacy kernels**
(`public/xash3d_mathlib.c`, `public/matrixlib.c`). Output →
`xash3dpp/tests/goldens/studio_math_goldens.inc` (committed source).

## Why this exists

The studio bone math has trig-interior paths — non-axis-aligned quaternions,
slerp interpolation, rotated `Matrix3x4_CreateFromEntity` — that cannot be
hand-derived to bit-exactness without hand-executing double-precision `sin`/`cos`.
The project parity rule (`tests/README.md`, Q-18) forbids copying a golden from
the implementation's own output. This harness is the sanctioned alternative:
run the legacy kernel, emit its result, cross-check the port against it.

Hand-derivable-exact cases (dyadic quaternions, position-only decode, slerp
endpoints, identity `CreateFromEntity`, hull planes) live **directly in the
tests** — they are not generated here. This file carries only the values that
genuinely need the legacy kernel.

## Regenerate

```powershell
xash3dpp\tools\legacy_golden_gen\regen.ps1
```

Requires an MSVC toolchain. The script finds `VsDevCmd.bat` via `vswhere`, or set
`XASH_VSDEVCMD` to point at it. Build artifacts land in `build/` (gitignored);
the exe compiles `gen.c` + the two legacy `.c` files as C and writes the `.inc`.

Re-run whenever the case tables in `gen.c` change. Commit the regenerated `.inc`
in the same change. It is **not** part of the normal xash3dpp build — the shipped
libraries never depend on the legacy tree; only this offline generator does.

## Layout

- `gen.c` — the generator: case tables + emit helpers (hex-float literals).
- `regen.ps1` — toolchain resolve + compile + run.
- `build/` — gitignored scratch (objs, `gen.exe`).
