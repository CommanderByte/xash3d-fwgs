# ref/

## Purpose

Pluggable renderer implementations for Xash3D. Each renderer (GL, software, null) exposes a common interface (`ref_interface_t`) loaded dynamically by the engine, handling scene rendering, entity management, lighting, texture management, decals, studio models, and frame composition. Renderers compile as separate DLLs/SOs and communicate with the engine through the `ref_api.h` ABI.

## Source Files

**ref/gl** — OpenGL renderer (primary desktop backend)

- `gl_rmain.c`, `gl_studio.c`, `gl_image.c`, `gl_rsurf.c`, `gl_beams.c`, `gl_decals.c`, `gl_backend.c`, `gl_context.c`
- `gl2_shim/` — ES2 fixed-function pipeline emulation
- `vgl_shim/` — PSVita nanoGL wrapper

**ref/soft** — software renderer (legacy CPU rasterization)

- `r_main.c`, `r_studio.c`, `r_bsp.c`, `r_rast.c`, `r_scan.c`, `r_image.c`, `r_edge.c`, `r_polyse.c`

**ref/common** — shared code across renderers

- `ref_context.c`, `ref_image.c`, `ref_light.c`, `ref_math.c`

## Renderer ABI

`engine/ref_api.h` (version 17) defines the bidirectional contract:

- **Engine → Renderer**: `GetRefAPI(version, ref_interface_t*, ref_api_t*, ref_globals_t*)` — renderer fills `ref_interface_t` with callbacks (`R_Init`, `R_RenderScene`, `R_AddEntity`, `GL_LoadTextureFromBuffer`, …) and receives engine functions (`Cvar_Get`, `Cmd_AddCommand`, `FS_LoadImage`, `Mod_ForName`, particle/dlight allocators, memory pools)
- **Callbacks**: scene lifecycle (`R_BeginFrame`, `R_RenderScene`, `R_EndFrame`), entity drawing, texture/decal/light management, studio/sprite rendering, screenshot capture
- **Engine parameters** via `EngineGetParm()`: client/host pointers, lightstyles, dlights, gamma tables, world pointer, connection state, demo playback

**This ABI is internal — free to redesign.**

## Key Data Structures and Globals

- `gl_globals_t tr` (GL/soft) — viewport, texture tables, lightmap cache, BSP draw state, entity list, PVS data
- `ref_instance_t RI` — per-frame render state: current entity, view matrices, frustum, fog, blend
- `glState` (GL) — OpenGL context state
- `gl_texture_t` — texture entry: name, dimensions, GL target/format, buffer reference, mips
- `gldepthmin/max`, `xcenter/ycenter/xscale/yscale` (soft) — projection/screen mapping

## Dependencies

- **public/** (crclib, mathlib), **common/** (model/entity definitions), **engine/ref_api.h**
- **SDL2** — window/GL context (`ref/gl/gl_context.c`)
- **GL4ES** — ES2→GL translation for mobile/legacy
- **nanoGL** — PSVita ES emulation
- **filesystem API** for asset loading

## Coupling and Risks

1. **Large global state** — `tr`, `glState`, `RI` tightly coupled; hard to thread or parallelize frame prep
1. **BSP/studio code duplication** — each renderer reimplements model state setup, surface sorting, animation frame selection
1. **Fixed-function vs. ES2 compatibility layer** — `gl2_shim`/`vgl_shim` add complexity; GoldSrc rendering quirks need adapters
1. **GoldSrc rendering contracts** — hardcoded HL1 conventions (lightmap lightness, water transparency, skybox orientation, decal lifetime) and mod-specific hacks (`HACKS_RELATED_HLMODS`)
1. **Tight engine coupling** — assume synchronous frame callbacks; no async compute or deferred resource loading

## Modernization Opportunities

- **Centralize model state** — move BSP/studio bone/frame/remapping logic into shared code; renderers receive pre-computed draw calls
- **Decouple global state** — replace `tr`/`glState`/`RI` with frame contexts passed through call stack
- **Render graph abstraction** — explicit command generation + backend dispatch (compute, async texture uploads)
- **Modular backend support** — add Vulkan/Metal/D3D12 alongside GL via common IR
- **Incremental modernization** — keep legacy callbacks as adapter layer; migrate gradually
