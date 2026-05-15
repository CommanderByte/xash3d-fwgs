# public/ + common/ + pm_shared/

## Purpose

- **`public/`** is a portable, dependency-free utility library (C) for math, compression, string handling, and build metadata.
- **`common/`** contains SDK-style header definitions — the ABI contract that game and client DLLs must comply with (**frozen**).
- **`pm_shared/`** holds player movement definitions shared between client prediction and server authoritative simulation.

## public/ (utility library)

- **crtlib** — custom C runtime partially replacing libc; string, memory, I/O for cross-platform compatibility
- **crclib** — CRC32 checksums for data validation
- **matrixlib** — 3x4 and 4x4 matrix operations; entity transforms and rendering
- **xash3d_mathlib** — float vector math (vec2, vec3, vec4, quaternions)
- **miniz** — zlib-compatible compression; archives and network compression
- **utflib** — UTF-8 string utilities
- **atlas** — strip-based 2D texture atlas packing (max 1024×1024)
- **getopt** — POSIX-style command-line parsing
- **build** — build metadata (version, timestamp, commit hash)
- **dllhelpers** — platform-specific DLL loading utilities

## common/ (SDK structures) — **FROZEN ABI**

Headers define the immutable ABI surface for game and client DLLs:

- **const.h** — game constants (entity flags: FL_FLY, FL_SWIM, FL_GODMODE, FL_ONGROUND, …)
- **xash3d_types.h** — base types (vec_t, vec3_t, quat_t, matrix3x4) and platform alignment rules
- **com_model.h** — model formats (brush, sprite, alias, studio); mesh vertices, edges, clip nodes
- **cl_entity.h** — client-side entity representation
- **entity_state.h** — network-serialized entity state (origin, angles, effects, animation, color)
- **event_args.h** — game event arguments
- **ref_params.h** — render parameters (viewpoint, time, aspect ratio)
- **weaponinfo.h** — weapon metadata
- **edict.h, eiface.h, cdll_int.h, cdll_exp.h** — server and client engine interfaces
- Plus: bspfile.h, entity_types.h, studio_event.h, sound_api.h, render_api.h, netadr.h, pmove.h, …

## pm_shared/ (player movement) — **FROZEN ABI**

- **pm_defs.h** — physics constants (MAX_PHYSENTS=600), trace flags (PM_STUDIO_IGNORE, PM_WORLD_ONLY), `physent_t`
- **pm_info.h** — player movement state shared between client prediction and server simulation

## Dependencies

`public/` has no external dependencies (portable C). `common/` and `pm_shared/` are headers-only.

## Coupling and Risks

1. **ABI rigidity** — struct layouts in `common/` and `pm_shared/` are immutable; alignment/field-order/size changes break DLL compatibility
1. **Dispersed `public/` consumption** — crtlib, matrixlib, crclib used globally; changes affect the entire codebase
1. **Player state desync** — `pm_shared/` client prediction must match server simulation exactly; FP precision differences cause hitbox divergence
1. **Custom CRT limitations** — crtlib does not implement full libc; mismatch on platforms with stricter SDK policies (iOS, Xbox)
1. **Type definition stability** — `xash3d_types.h` defines `vec_t` as `float`; shader and physics code assumes this precision

## Modernization Opportunities

Critically distinguish:

- `common/` + `pm_shared/` headers must be preserved bit-for-bit (SDK ABI)
- `public/` utilities are free to be rewritten in C++ behind a clean facade

Concrete directions:

- **C++ wrapper facade** — rewrite `public/` utilities in modern C++ (SIMD-optimized matrixlib, move semantics); thin C API for ABI preservation
- **Fixed-point physics option** — migrate `pm_shared/` predictor/simulator pair to fixed-point arithmetic to guarantee determinism across CPUs
- **Standardize type aliases** — consolidate `vec_t`, `float32_t`, platform-specific overrides
- **Unicode UTF-8 throughout** — extend `utflib`; enforce UTF-8 in asset pipelines
- **Memory pool APIs** — promote `poolhandle_t` to replace static allocators; eliminate hidden global state
