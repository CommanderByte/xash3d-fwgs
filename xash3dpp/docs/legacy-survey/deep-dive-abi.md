# Deep Dive: Legacy Origins of `xash3dpp/abi` — the Game-DLL Export Negotiation and the `Host_Error` Frozen Contract

*Recon brief produced 2026-07-06 by a read-only survey agent as part of the
as-built documentation refresh. Scope: the legacy behaviours the `xash3dpp`
**abi** subsystem preserves — the server/client game-DLL load-time export
negotiation (`GiveFnptrsToDll` / `GetEntityAPI` / `GetEntityAPI2` /
`GetNewDLLFunctions`) and the `Host_Error` `GAME_EXPORT` direct-symbol contract.
Line numbers are against the working tree on that date; behaviour references,
not design constraints. Everything about the legacy is **reference-only** — and
uniquely for this subsystem, the frozen ABI it documents is **immovable**: the
job of `abi` is to preserve these contracts byte-for-byte, never to redesign
them.*

`abi` is not a distillation like `core`/`host` — it is a **vendoring + bridge**
layer. It does two things and only two: (1) it holds byte-exact C++ mirrors of
the frozen SDK structs/tables the game DLL reads and writes, and (2) it hosts
the `extern "C"` engine→game direct-export symbols the game DLL links against by
name. This brief documents the legacy mechanism behind (2) — the DLL load
negotiation and the `Host_Error` export — because that is the moving part; the
vendored *layouts* are catalogued in the server-side recon
(`deep-dive-server-game-dll-bridge.md`) and pinned by the layout tests.

Primary legacy sources:

- `engine/server/sv_game.c` (~5300 lines) — `SV_LoadProgs` (`~5215`), the
  `enginefuncs_t gEngfuncs` fill table, `GAME_EXPORT` slot bodies, the
  `GIVEFNPTRSTODLL` / `APIFUNCTION` / `APIFUNCTION2` / `NEW_DLL_FUNCTIONS_FN`
  typedefs (`~40`, `~5217`)
- `engine/common/host.c` — `Host_Error` (`~687`, `GAME_EXPORT`), `Sys_Error`
- `engine/platform/win32/lib_win.c` (`~283`) — the custom PE loader's special
  case for the `GiveFnptrsToDll` export name
- `engine/eiface.h` — `enginefuncs_t` (159), `DLL_FUNCTIONS` (50),
  `NEW_DLL_FUNCTIONS` (5), `INTERFACE_VERSION` frozen at 140, "ONLY ADD NEW
  FUNCTIONS TO THE END OF THIS STRUCT" (eiface.h:286)

**Global assumptions (legacy):** the game DLL is a single per-process module
(`svgame.hInstance`); the engine hands it one `enginefuncs_t` table by pointer
and receives one `DLL_FUNCTIONS` (+ optional `NEW_DLL_FUNCTIONS`) table back;
all of it runs on the single main thread; the returned tables are stored in the
process-global `svgame` struct for the module's lifetime.

______________________________________________________________________

## 1. The load-time export negotiation — `SV_LoadProgs`

`SV_LoadProgs` (`sv_game.c:~5215`) is the whole engine↔game handshake. Its
skeleton (function pointers resolved by name from the loaded module):

```c
static APIFUNCTION       GetEntityAPI;        // int (*)(DLL_FUNCTIONS*, int)
static APIFUNCTION2      GetEntityAPI2;        // int (*)(DLL_FUNCTIONS*, int*)
static GIVEFNPTRSTODLL   GiveFnptrsToDll;      // void (*)(enginefuncs_t*, globalvars_t*)
static NEW_DLL_FUNCTIONS_FN GiveNewDllFuncs;   // int (*)(NEW_DLL_FUNCTIONS*, int*)

svgame.hInstance = COM_LoadLibrary( name, true, false );          // 5235
GetEntityAPI    = COM_GetProcAddress( svgame.hInstance, "GetEntityAPI" );   // 5256
GetEntityAPI2   = COM_GetProcAddress( svgame.hInstance, "GetEntityAPI2" );  // 5257
GiveNewDllFuncs = COM_GetProcAddress( svgame.hInstance, "GetNewDLLFunctions" ); // 5258
if( !GetEntityAPI && !GetEntityAPI2 ) { /* fatal: missing both */ }         // 5260
GiveFnptrsToDll = COM_GetProcAddress( svgame.hInstance, "GiveFnptrsToDll" ); // 5270
if( !GiveFnptrsToDll ) { /* fatal: missing */ }                             // 5272

GiveFnptrsToDll( &gpEngfuncs, svgame.globals );   // 5282 — engine → game (push)

// optional new funcs, version-checked (warn-only mismatch)
if( GiveNewDllFuncs ) {                            // 5286
    version = NEW_DLL_FUNCTIONS_VERSION;
    if( !GiveNewDllFuncs( &svgame.dllFuncs2, &version )) { /* not present */ }
    else if( version != NEW_DLL_FUNCTIONS_VERSION ) { /* S_WARN */ }
}

version = INTERFACE_VERSION;                       // 5297 (140)
if( GetEntityAPI2 && GetEntityAPI2( &svgame.dllFuncs, &version )) { init = true; } // 5299
else if( version != INTERFACE_VERSION ) { /* S_WARN should be 140 */ }             // 5306
if( !init && GetEntityAPI && GetEntityAPI( &svgame.dllFuncs, version )) { init = true; } // 5309
```

Three contract facts fall out, and all three are **frozen**:

1. **Direction.** `GiveFnptrsToDll` is a **push** (engine → game): the engine
   passes the game its `enginefuncs_t` vtable *and* `gpGlobals`. `GetEntityAPI2`
   / `GetEntityAPI` are **pulls** (game → engine): the game fills the engine's
   `DLL_FUNCTIONS` table. Reversing this, or changing either signature, breaks
   every stock HL game DLL.
2. **Version negotiation is by-value and asymmetric.** `INTERFACE_VERSION` (140)
   is passed *by pointer* to `GetEntityAPI2` so the DLL can report its own; a
   mismatch is **warn-only**, not fatal — GoldSrc compatibility depends on the
   engine tolerating older DLLs. `GetEntityAPI` (the older single-int form) is
   the fallback.
3. **`GiveFnptrsToDll` is name-special even in the loader.** The Win32 custom PE
   loader (`lib_win.c:283`) hard-codes `"GiveFnptrsToDll"` as *the* user-DLL
   entry point when walking the export table — the name itself is part of the
   ABI.

The client side is the mirror image (not shown): `F` /
`Initialize(cl_enginefuncs_t*, int)` / `HUD_*` exports negotiated through
`cdll_int.h` / `cdll_exp.h` (frozen surfaces in the ABI table). The abi
subsystem is the seam that must not break *any* of these.

______________________________________________________________________

## 2. The `Host_Error` frozen direct export

`Host_Error` (`host.c:~687`, `GAME_EXPORT void Host_Error(const char *error,
...)`) is the one direct-export symbol the `xash3dpp_abi` shim ships today. It is
special because it is **not** an `enginefuncs_t` slot — game DLLs (and the
engine itself) call it **by name**, so it must be an exported symbol, not a
vtable entry. Its legacy logic (`host.c:687–748`), summarised (full flow in
`deep-dive-host.md` §4):

- Format into `static char hosterror1[MAX_SYSPATH]`.
- `host.framecount < 3` → `Sys_Error` (fatal, too early to recover);
  second error **this** frame (`errorframe == framecount`) → `Sys_Error` (Q-4).
- Otherwise print `S_RED`, set the recursion guard, clean up
  (`Cbuf_Clear`/`SV_Shutdown`/`CL_Drop`/…), then `Host_AbortCurrentFrame()` →
  `longjmp( g_abortframe, 1 )`.

The frozen contract the shim must honour: **the symbol name `Host_Error`, the C
signature `void Host_Error(const char *fmt, ...)`, the `GAME_EXPORT` linkage,
and the "this call does not return on a fatal error" behaviour.** Everything
*behind* the signature (the `longjmp`, the `static` scratch buffers, the state
machine) is legacy implementation the rewrite is free to replace.

______________________________________________________________________

## 3. What the rewrite keeps vs replaces

The rewrite splits the legacy `Host_Error` cleanly along the ABI line:

- **Frozen (kept verbatim in `xash3dpp_abi`):** the `extern "C"`
  `Host_Error(const char*, ...)` symbol, exported via `__declspec(dllexport)`
  (Win32) / `__attribute__((visibility("default")))` (POSIX). The `...` varargs
  and `const char*` are immovable — the signature *is* the ABI.
- **Replaced (behind the signature):** the `static hosterror1/2` buffers →
  formatted through `core::log_va` (Fatal); the `longjmp` →
  `Host::signal_frame_abort(core::ErrorCode::HostFatal, …)` reached through the
  `current_engine_context()` accessor (OQ-1 flag-poll, no `setjmp`/`longjmp`);
  the `framecount`/`errorframe` recursion guard → the host's recursive-abort →
  `XASH_FATAL` escalation (host Q-4); the "no live engine yet" early-fatal →
  `core::log_fatal` + `std::abort()` when `current_engine_context()` is `nullptr`.

Crucially, the accessor's **state** (`g_engine_ctx`) was RELOCATED out of `abi`
into `xash3dpp_host` (`src/host/engine_context_accessor.cpp`) under the D-1
dependency split (2026-07-06, OQ-10): the abi shim *reads* the host-owned
singleton, so the link is one-way `abi → host` and the historical `host ⇄ abi`
cycle is gone. The header keeps its `include/xash3dpp/abi/` path and `xash::abi`
namespace (see `deep-dive-host.md` §7 and `abi-boundary.md` As-built
reconciliation).

______________________________________________________________________

## 4. As-built mapping (legacy → `xash3dpp/abi`)

| Legacy construct | Where it lives now | Notes |
|------------------|--------------------|-------|
| `GiveFnptrsToDll(enginefuncs_t*, globalvars_t*)` push | vendored `enginefuncs_t`/`globalvars_t` decls (`eiface.hpp`) + server load path | Layouts frozen in `abi`; the *fill/negotiation* is a **server** concern (`src/server/abi/engine_table.cpp`, `deep-dive-server-game-dll-bridge.md`) |
| `GetEntityAPI` / `GetEntityAPI2(DLL_FUNCTIONS*, int*)` pull | vendored `DLL_FUNCTIONS` decl (`eiface.hpp`) + server | 50-slot table, `static_assert`ed; version-negotiation logic lives in server |
| `GetNewDLLFunctions(NEW_DLL_FUNCTIONS*, int*)` | vendored `NEW_DLL_FUNCTIONS` decl (`eiface.hpp`) | 5-slot table, `static_assert`ed; warn-only version mismatch preserved server-side |
| `INTERFACE_VERSION` (140), slot counts 159/50/5 | `static_assert`s in `eiface.hpp` + `k_interface_version` | Frozen (eiface.h:22/286); pinned by `tests/server/abi/test_*_layout.cpp` on 32/64-bit |
| `Host_Error(const char*, ...)` `GAME_EXPORT` symbol | `xash3dpp_abi` `src/abi/engine_funcs.cpp` | **Frozen C signature + linkage** (Q-14/OQ-10); the one shipped direct-export shim |
| `static hosterror1/2[MAX_SYSPATH]` + `S_RED` print | `core::log_va(Fatal)` + `core::log_fatal` | No static scratch; typed severity logging (QI: never raw stdio) |
| `longjmp( g_abortframe )` (in `Host_Error`) | `Host::signal_frame_abort` via `current_engine_context()` | OQ-1 flag-poll; no `setjmp`/`longjmp` (see `deep-dive-host.md` §4) |
| `framecount<3` / `errorframe==framecount` early-fatal + recursion | no-context branch → `log_fatal`+`std::abort`; host recursive-abort → `XASH_FATAL` | Q-4 escalation preserved; the "no live engine" case aborts hard to keep the game-DLL contract |
| `svgame.hInstance` / `COM_LoadLibrary` / `COM_GetProcAddress` | `platform` dynlib API + **server** load path | Module load is a platform/server concern; `abi` only supplies the vendored table types |
| `GiveFnptrsToDll` name-special in PE loader (`lib_win.c:283`) | `platform` custom PE loader | Legacy loader detail; the frozen *name* is the contract `abi`/server preserve |
| single global `svgame` (returned tables) | `EngineContext` (host) + server slot state | No process-global game struct; `abi` owns **no** state (accessor state is host-owned after D-1) |
| frozen static-return-buffer slots (`pfnGetCvarString` → `const char*`, `pfnVecToYaw` → scratch, …) | vendored decls in `eiface.hpp`; **buffers owned by server** | `const char*`/`float*` valid only for the call on Main — frozen thread contract (see `abi-threading.md`) |

______________________________________________________________________

## 5. New-in-rewrite (no legacy analog)

- **Typed forward target.** The legacy shim did its work inline (format +
  `longjmp`); the rewrite forwards to the typed
  `signal_frame_abort(core::ErrorCode, std::string_view)` — the C ABI is the
  only untyped surface, and it is confined to the one shim.
- **The `current_engine_context()` accessor as the *only* C-ABI reach-in.** The
  legacy engine reached game state through scattered globals (`svgame`, `sv`,
  `host`); the rewrite funnels every `extern "C"` symbol through one sanctioned
  atomic accessor (Q-2/OQ-10), whose state is host-owned after the D-1 split.
- **The G-2 door (post-parity, not built).** The frozen GoldSrc ABI is treated
  as **one load-time flavor** (Q-20): a future context-carrying v2 ABI would sit
  *alongside* it behind the `EntityView` seam, carrying a context handle in every
  slot. The legacy engine had no such notion — the ABI was simply *the* ABI. See
  `abi-boundary.md` Extension axes (Q-21) and `extension-goals.md` §G-2.
